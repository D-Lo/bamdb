/* Some additional functions to simplify pulling rows from BAM data */

#include "htslib/sam.h"

const char *bam_get_rname(const bam1_t *row, const bam_hdr_t *header);
const char *bam_get_rnext(const bam1_t *row, const bam_hdr_t *header);

/* Return pointer to beginning of formatted string in work_buffer,
 * advance work_buffer to the element after the formatted string */
char *bam_cigar_str(const bam1_t *row, char *work_buffer);
char *bam_seq_str(const bam1_t *row, char *work_buffer);
char *bam_qual_str(const bam1_t *row, char *work_buffer);

char *bam_bx_str(const bam1_t *row, char *work_buffer);


#define bam_row_size(b) (sizeof(struct bam1_t) + b->l_data * sizeof(uint8_t))
#define bam_row_max_size(b) (sizeof(struct bam1_t) + b->m_data * sizeof(uint8_t))
