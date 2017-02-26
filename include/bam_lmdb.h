
#include <lmdb.h>

#include "bam_api.h"
#include "htslib/sam.h"


int convert_to_lmdb(samFile *input_file, char *db_path, int max_rows);
int get_offsets(offset_list_t *offset_list, const char *lmdb_db_name, const char *bx);
