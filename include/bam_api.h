/* Some additional functions to simplify pulling rows from BAM data */
#ifndef _BAMAPIH_
#define _BAMAPIH_

#include "htslib/sam.h"

const char *bam_get_rname(const bam1_t *row, const bam_hdr_t *header);
const char *bam_get_rnext(const bam1_t *row, const bam_hdr_t *header);

/* Return pointer to beginning of formatted string in work_buffer,
 * advance work_buffer to the element after the formatted string */
char *bam_cigar_str(const bam1_t *row, char *work_buffer);
char *bam_seq_str(const bam1_t *row, char *work_buffer);
char *bam_qual_str(const bam1_t *row, char *work_buffer);

char *bam_bx_str(const bam1_t *row, char *work_buffer);

typedef struct bam_sequence_row {
	char *qname;
	int flag;
	char *rname;
	int pos;
	int mapq;
	char *cigar;
	char *rnext;
	int pnext;
	int tlen;
	char *seq;
	char *qual;
	/* TODO: Add key value data */
} bam_sequence_row_t;

typedef struct bam_row_set {
	size_t n_entries;
	bam_sequence_row_t **rows;
} bam_row_set_t;

typedef struct offset_node {
	int64_t offset;
	struct offset_node *next;
} offset_node_t;

typedef struct offset_list {
	struct offset_node *head;
	struct offset_node *tail;
} offset_list_t;

bam_sequence_row_t *deserialize_bam_row(const bam1_t *row, const bam_hdr_t *header);
bam_sequence_row_t *get_bam_row(int64_t offset, samFile *input_file, bam_hdr_t *header);
void print_sequence_row(bam_sequence_row_t *row);
void destroy_bam_sequence_row(bam_sequence_row_t *row);
void free_row_set(bam_row_set_t *row_set);


#define bam_row_size(b) (sizeof(bam1_t) + b->l_data * sizeof(uint8_t))
#define bam_row_max_size(b) (sizeof(bam1_t) + b->m_data * sizeof(uint8_t))

/* I really hope we don't have sequences longer than this */
#define WORK_BUFFER_SIZE 65536

#endif
