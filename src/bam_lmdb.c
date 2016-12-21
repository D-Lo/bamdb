#include <linux/limits.h>
#include <lmdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "bam_lmdb.h"
#include "bam_api.h"

#include "htslib/bgzf.h"
#include "htslib/hts.h"

#define LMDB_POSTFIX "_lmdb"
#define LMDB_COMMIT_FREQ 100000
#define LMDB_INIT_MAPSIZE 1024 * 1024 * 1024
#define WORK_BUFFER_SIZE 65536

enum lmdb_keys {
	LMDB_QNAME = 0,
	LMDB_BX,
	LMDB_MAX
};

static const char *lmdb_key_names[LMDB_MAX] = {
	"qname",
	"bx"
};

static char *
get_default_dbpath(const char *filename)
{
	char *db_path;
	char *dot = strrchr(filename, '.');

	db_path = malloc(dot - filename + sizeof(LMDB_POSTFIX) + 1); /* Leak this */
	strncpy(db_path, filename, dot - filename);
	strcpy(db_path + (dot - filename), LMDB_POSTFIX);

	return db_path;
}


static int
start_transactions(MDB_env **env, MDB_dbi *dbi, MDB_txn **txn)
{
	int rc;

	for (int i = 0; i < LMDB_MAX; i++) {
		rc = mdb_txn_begin(env[i], NULL, 0, &txn[i]);
		if (rc != MDB_SUCCESS) {
			fprintf(stderr, "Error beginning transaction: %s", mdb_strerror(rc));
			return 1;
		}

		rc = mdb_open(txn[i], NULL, 0, &dbi[i]);
		if (rc != MDB_SUCCESS) {
			fprintf(stderr, "Error opening transaction: %s", mdb_strerror(rc));
			return 1;
		}
	}

	return 0;
}


static int
commit_transactions(MDB_txn **txn)
{
	int rc;

	for (int i = 0; i < LMDB_MAX; i++) {
		rc = mdb_txn_commit(txn[i]);
		if (rc != MDB_SUCCESS) {
			fprintf(stderr, "Error commiting transaction: %s", mdb_strerror(rc));
			return 1;
		}
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
			fprintf(stderr, "Error putting qname: %s\n", mdb_strerror(rc));
			return 1;
		}

		rc = mdb_put(txn, dbi, key, data, 0);
	}

	return 0;
}


int
convert_to_lmdb(samFile *input_file, char *db_path, int max_rows)
{
	MDB_env *env[LMDB_MAX];
	MDB_dbi dbi[LMDB_MAX];
	MDB_val key, data;
	MDB_txn *txn[LMDB_MAX];
	char db_name[PATH_MAX];
	int mapsize = LMDB_INIT_MAPSIZE;
	int rc, r;
	int ret = 0;

	bam_hdr_t *header = NULL;
	bam1_t *bam_row;
	char *work_buffer = malloc(WORK_BUFFER_SIZE);
	char *buffer_pos = work_buffer;

	if (db_path == NULL) {
		db_path = get_default_dbpath(input_file->fn);
	}
	printf("Attempting to convert bam file %s into lmdb database at path %s\n", input_file->fn, db_path);

	mkdir(db_path, 0777);

	for (int i = 0; i < LMDB_MAX; i++) {
		sprintf(db_name, "%s/%s", db_path, lmdb_key_names[i]);
		mkdir(db_name, 0777);

		rc = mdb_env_create(&env[i]);
		if (rc != MDB_SUCCESS) {
			fprintf(stderr, "Error creating env %s: %s\n", db_name, mdb_strerror(rc));
			return 1;
		}

		rc = mdb_env_set_mapsize(env[i], mapsize);
		if (rc != MDB_SUCCESS) {
			fprintf(stderr, "Error setting map size %s: %s\n", db_name, mdb_strerror(rc));
			return 1;
		}

		//rc = mdb_env_open(env[i], db_name, MDB_DUPSORT | MDB_CREATE, 0664);
		rc = mdb_env_open(env[i], db_name, 0, 0664);
		if (rc != MDB_SUCCESS) {
			fprintf(stderr, "Error opening env %s: %s\n", db_name, mdb_strerror(rc));
			return 1;
		}
	}

	if (start_transactions(env, dbi, txn) != 0) {
		ret = 1;
		goto exit;
	}

	header = sam_hdr_read(input_file);
	if (header == NULL) {
		fprintf(stderr, "Unable to read the header from %s\n", input_file->fn);
		ret = 1;
		goto exit;
	}

	bam_row = bam_init1();
	uint32_t n = 0;
	while ((r = sam_read1(input_file, header, bam_row)) >= 0) {
		buffer_pos = work_buffer;

		int64_t voffset = bgzf_tell(input_file->fp.bgzf);
		char *qname = bam_get_qname(bam_row);
		char *bx = bam_bx_str(bam_row, buffer_pos);

		data.mv_size = sizeof(int64_t);
		data.mv_data = &voffset;

		/* insert voffset under qname */
		key.mv_size = strlen(qname);
		key.mv_data = qname;

		if (put_entry(env[LMDB_QNAME], txn[LMDB_QNAME], dbi[LMDB_QNAME], &key, &data, &mapsize) != 0) {
			ret = 1;
			goto exit;
		}

		/* insert voffset under bx */
		key.mv_size = strlen(bx);
		key.mv_data = bx;

		if (put_entry(env[LMDB_BX], txn[LMDB_BX], dbi[LMDB_BX], &key, &data, &mapsize) != 0) {
			ret = 1;
			goto exit;
		}

		if (++n % 100000 == 0) printf("%u rows inserted\n", n);

		if (max_rows > 0 && n >= max_rows) {
			break;
		}

		/* commit every so often for safety */
		if (n % LMDB_COMMIT_FREQ == 0) {
			if (commit_transactions(txn) != 0) {
				ret = 1;
				goto exit;
			}

			if (start_transactions(env, dbi, txn) != 0) {
				ret = 1;
				goto exit;
			}
		}
	}

	if (r < -1) {
		fprintf(stderr, "Attempting to process truncated file.\n");
		ret = 1;
		goto exit;
	}

	commit_transactions(txn);

exit:
	for (int i = 0; i < LMDB_MAX; i++) {
		mdb_close(env[i], dbi[i]);
		mdb_env_close(env[i]);
	}

	return ret;
}

