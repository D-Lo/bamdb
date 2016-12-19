/* API for converting between BAM and SQLite */

#include <sqlite3.h>

#include "htslib/sam.h"


int convert_to_sqlite(samFile *input_file, char *db_name, int max_rows);
