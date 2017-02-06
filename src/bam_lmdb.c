#include <ck_fifo.h>
#include <ck_pr.h>

#include <inttypes.h>
#include <limits.h>
#include <lmdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "bam_lmdb.h"
#include "bam_api.h"

#include "htslib/bgzf.h"
#include "htslib/hts.h"

#define LMDB_POSTFIX "_lmdb"
#define LMDB_COMMIT_FREQ 100000
#define LMDB_INIT_MAPSIZE 1024 * 1024 * 1024
#define WORK_BUFFER_SIZE 65536

#define DESERIALIZE_THREAD_COUNT 4
#define DESERIALIZE_BUFFER_SIZE 1024
#define MAX_WRITE_QUEUE_SIZE 65536

enum lmdb_keys {
	LMDB_QNAME = 0,
	LMDB_BX,
	LMDB_MAX
};

struct _bam_data {
	bam1_t *bam_row;
	int64_t voffset;
};

typedef struct write_entry {
	char *qname;
	char *bx;
	int64_t voffset;
} write_entry_t;

typedef struct _deserialize_buffer {
	struct _bam_data *bam_rows;
	uint64_t reader_pos;
	uint64_t deserialize_pos;
} deserialize_buffer_t;

typedef struct _deserialize_thread_data {
	bam_hdr_t *header;
	deserialize_buffer_t *deserialize_buffer;
	ck_fifo_mpmc_t *write_q;
} deserialize_thread_data_t;

typedef struct _writer_thread_data {
	MDB_env *env;
	ck_fifo_mpmc_t *write_q;
} writer_thread_data_t;

static const char *lmdb_key_names[LMDB_MAX] = {
	"qname",
	"bx"
};

int reader_running;
int deserialize_running;
int write_queue_size;

static char *
get_default_dbname(const char *filename)
{
	char *db_name;
	char *dot = strrchr(filename, '.');

	db_name = malloc(dot - filename + sizeof(LMDB_POSTFIX) + 1); /* Leak this */
	strncpy(db_name, filename, dot - filename);
	strcpy(db_name + (dot - filename), LMDB_POSTFIX);

	return db_name;
}


static int
start_transaction(MDB_env *env, MDB_dbi *dbi, MDB_txn **txn)
{
	int rc;

	rc = mdb_txn_begin(env, NULL, 0, txn);
	if (rc != MDB_SUCCESS) {
		fprintf(stderr, "Error beginning transaction: %s\n", mdb_strerror(rc));
		return 1;
	}

	for (int i = 0; i < LMDB_MAX; i++) {
		rc = mdb_dbi_open(*txn, lmdb_key_names[i], MDB_DUPSORT | MDB_CREATE, &dbi[i]);
		if (rc != MDB_SUCCESS) {
			fprintf(stderr, "Error opening database: %s\n", mdb_strerror(rc));
			return 1;
		}
	}

	return 0;
}


static int
commit_transaction(MDB_txn *txn)
{
	int rc;

	rc = mdb_txn_commit(txn);
	if (rc != MDB_SUCCESS) {
		fprintf(stderr, "Error commiting transaction: %s\n", mdb_strerror(rc));
		return 1;
	}

	return 0;
}


static int
put_entry(MDB_env *env, MDB_txn *txn, MDB_dbi dbi, MDB_val *key, MDB_val *data, int *mapsize)
{
	int rc = mdb_put(txn, dbi, key, data, 0);

	while (rc != MDB_SUCCESS) {
		if (rc == MDB_MAP_FULL) {
			*mapsize *= 2;
			mdb_env_set_mapsize(env, *mapsize);
		} else {
			fprintf(stderr, "Error putting entry: %s\n", mdb_strerror(rc));
			return 1;
		}

		rc = mdb_put(txn, dbi, key, data, 0);
	}

	return 0;
}


static void *
deserialize_func(void *arg)
{
	deserialize_thread_data_t *data = (deserialize_thread_data_t *)arg;
	deserialize_buffer_t *buffer = data->deserialize_buffer;
	char *work_buffer = malloc(WORK_BUFFER_SIZE);
	char *buffer_pos = work_buffer;
	bam1_t *bam_row;

	while (ck_pr_load_int(&reader_running) || ck_pr_load_64(&buffer->deserialize_pos) < ck_pr_load_64(&buffer->reader_pos)) {
		int buffer_slot = buffer->deserialize_pos % DESERIALIZE_BUFFER_SIZE;
		/* Make sure we stay behind the reader thread
		 * fully drain the buffer if reader is done */
		bool ahead_of_reader = ck_pr_load_64(&buffer->deserialize_pos) >= ck_pr_load_64(&buffer->reader_pos) && ck_pr_load_int(&reader_running) == 1;
		while (ahead_of_reader || ck_pr_load_int(&write_queue_size) >= MAX_WRITE_QUEUE_SIZE ) {
			usleep(100);
			ahead_of_reader = ck_pr_load_64(&buffer->deserialize_pos) >= ck_pr_load_64(&buffer->reader_pos) && ck_pr_load_int(&reader_running) == 1;
		}

		bam_row = buffer->bam_rows[buffer_slot].bam_row;
		buffer_pos = work_buffer;

		write_entry_t *entry = malloc(sizeof(write_entry_t));
		ck_fifo_mpmc_entry_t *fifo_entry = malloc(sizeof(ck_fifo_mpmc_entry_t));
		entry->qname = strdup(bam_get_qname(bam_row));
		entry->bx = strdup(bam_bx_str(bam_row, buffer_pos));
		entry->voffset = buffer->bam_rows[buffer_slot].voffset;

		ck_fifo_mpmc_enqueue(data->write_q, fifo_entry, entry);
		ck_pr_inc_64(&buffer->deserialize_pos);
		ck_pr_inc_int(&write_queue_size);
	}

	ck_pr_dec_int(&deserialize_running);
	pthread_exit(NULL);
}


static void *
writer_func(void *arg)
{
	writer_thread_data_t *data = (writer_thread_data_t *)arg;

	MDB_val key, val;
	MDB_txn *txn;
	MDB_dbi dbi[LMDB_MAX];
	uint64_t n;

	write_entry_t *entry;
	ck_fifo_mpmc_entry_t *garbage;

	int mapsize = LMDB_INIT_MAPSIZE;
	int rc = mdb_env_set_mapsize(data->env, mapsize);
	if (rc != MDB_SUCCESS) {
		fprintf(stderr, "Error setting map size: %s\n", mdb_strerror(rc));
	}

	/* TODO: check that this worked */
	start_transaction(data->env, dbi, &txn);

	while (ck_pr_load_int(&deserialize_running) > 0) {
		while (ck_fifo_mpmc_trydequeue(data->write_q, &entry, &garbage) == true) {
			free(garbage);

			val.mv_size = sizeof(int64_t);
			val.mv_data = &entry->voffset;

			/* insert voffset under qname */
			key.mv_size = strlen(entry->qname);
			key.mv_data = entry->qname;
			put_entry(data->env, txn, dbi[LMDB_QNAME], &key, &val, &mapsize);

			/* insert voffset under bx */
			key.mv_size = strlen(entry->bx);
			key.mv_data = entry->bx;
			put_entry(data->env, txn, dbi[LMDB_BX], &key, &val, &mapsize);

			free(entry->qname);
			free(entry->bx);
			free(entry);

			n++;
			ck_pr_dec_int(&write_queue_size);
			/* commit every so often for safety */
			if (n % LMDB_COMMIT_FREQ == 0) {
				commit_transaction(txn);
				printf("%" PRIu64 " records written\n", n);
				mdb_env_sync(data->env, 1);
				start_transaction(data->env, dbi, &txn);
			}
		}

		ck_pr_stall();
	}

	commit_transaction(txn);
	for (int i = 0; i < LMDB_MAX; i++) {
		mdb_dbi_close(data->env, dbi[i]);
	}
	mdb_env_close(data->env);

	pthread_exit(NULL);
}

int
convert_to_lmdb(samFile *input_file, char *db_name, int max_rows)
{
	MDB_env *env;
	int rc;
	int r = 0;
	int ret = 0;
	bam_hdr_t *header = NULL;
	uint64_t n;

	deserialize_buffer_t *deserialize_buffers = calloc(DESERIALIZE_THREAD_COUNT, sizeof(deserialize_buffer_t));
	writer_thread_data_t writer_thread_args;
	pthread_t threads[DESERIALIZE_THREAD_COUNT + 1];

	ck_fifo_mpmc_t *write_q = calloc(1, sizeof(ck_fifo_mpmc_t));
	ck_fifo_mpmc_init(write_q, malloc(sizeof(ck_fifo_mpmc_entry_t)));

	reader_running = 1;
	deserialize_running = DESERIALIZE_THREAD_COUNT;
	write_queue_size = 0;

	if (db_name == NULL) {
		db_name = get_default_dbname(input_file->fn);
	}
	printf("Attempting to convert bam file %s into lmdb database at path %s\n", input_file->fn, db_name);

	mkdir(db_name, 0777);

	rc = mdb_env_create(&env);
	if (rc != MDB_SUCCESS) {
		fprintf(stderr, "Error creating env: %s\n", mdb_strerror(rc));
		return 1;
	}

	rc = mdb_env_set_maxdbs(env, LMDB_MAX);
	if (rc != MDB_SUCCESS) {
		fprintf(stderr, "Error setting maxdbs: %s\n", mdb_strerror(rc));
		return 1;
	}

	rc = mdb_env_open(env, db_name, MDB_WRITEMAP | MDB_MAPASYNC, 0664);
	if (rc != MDB_SUCCESS) {
		fprintf(stderr, "Error opening env: %s\n", mdb_strerror(rc));
		return 1;
	}

	header = sam_hdr_read(input_file);
	if (header == NULL) {
		fprintf(stderr, "Unable to read the header from %s\n", input_file->fn);
		ret = 1;
		goto exit;
	}

	for (size_t i = 0; i < DESERIALIZE_THREAD_COUNT; i++) {
		deserialize_buffers[i].bam_rows = calloc(DESERIALIZE_BUFFER_SIZE, sizeof(struct _bam_data));
		for (size_t j = 0; j < DESERIALIZE_BUFFER_SIZE; j++) {
			deserialize_buffers[i].bam_rows[j].bam_row = bam_init1();
		}
	}

	for (size_t i = 0; i < DESERIALIZE_THREAD_COUNT; i++) {
		deserialize_thread_data_t deserialize_thread_args;
		deserialize_thread_args.header = header;
		deserialize_thread_args.deserialize_buffer = &deserialize_buffers[i];
		deserialize_thread_args.write_q = write_q;
		rc = pthread_create(&threads[1 + i], NULL, deserialize_func, &deserialize_thread_args);
		if (rc != 0) {
			fprintf(stderr, "Received non-zero return code when launching deserialize thread: %d\n", rc);
			ret = 1;
			goto exit;
		}
	}

	writer_thread_args.env = env;
	writer_thread_args.write_q = write_q;
	rc = pthread_create(&threads[DESERIALIZE_THREAD_COUNT + 1], NULL, writer_func, &writer_thread_args);
	if (rc != 0) {
		fprintf(stderr, "Received non-zero return code when launching writer thread: %d\n", rc);
		ret = 1;
		goto exit;
	}

	while (r >= 0) {
		deserialize_buffer_t *buffer = &deserialize_buffers[n % DESERIALIZE_THREAD_COUNT];
		int buffer_slot = buffer->reader_pos % DESERIALIZE_BUFFER_SIZE;

		/* Ensure that we don't overwrite data that has yet to be deserialized */
		while (buffer->reader_pos - ck_pr_load_64(&buffer->deserialize_pos) >= DESERIALIZE_BUFFER_SIZE - 1) {
			usleep(100);
		}

		r = sam_read1(input_file, header, buffer->bam_rows[buffer_slot].bam_row);
		buffer->bam_rows[buffer_slot].voffset = bgzf_tell(input_file->fp.bgzf);
		if (r >= 0) {
			ck_pr_inc_64(&buffer->reader_pos);
		}

		if (n++ % 100000 == 0) {
			printf("%" PRIu64 " rows read. Last voffset %" PRId64 "\n", n, buffer->bam_rows[buffer_slot].voffset);
		}
	}
	ck_pr_dec_int(&reader_running);

	if (r < -1) {
		fprintf(stderr, "Attempting to process truncated file.\n");
		ret = -1;
		goto exit;
	}
	for (size_t i = 0; i < DESERIALIZE_THREAD_COUNT + 1; i++) {
		pthread_join(threads[i], NULL);
	}

exit:

	return ret;
}
