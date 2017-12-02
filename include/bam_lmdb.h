#ifndef BAMLMDB_H
#define BAMLMDB_H

#include <lmdb.h>

#include "bam_api.h"
#include "htslib/sam.h"


int convert_to_lmdb_wgs(samFile *input_file, char *db_path);   // NEED FOR SINGLE-CELL: CB, UB, QNAME

/* Return number of offsets on success, -1 on failure */
int get_offsets(offset_list_t *offset_list, const char *lmdb_db_name, const char *bx);   // NEED FOR BX, UX, OR BOTH

bam_row_set_t *get_bx_rows(char *input_file_name, char *db_path, char *bx);  
bam_row_set_t *get_bx_rows_wrapped(char **input_file_name, char **db_path, char **bx);  
bam_row_set_t *get_cb_rows(char *input_file_name, char *db_path, char *cb);   
bam_row_set_t *get_ub_rows(char *input_file_name, char *db_path, char *ub);   



#endif
