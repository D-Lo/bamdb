#ifndef BAM_H
#define BAM_H

#define MAX_FILENAME 1024

const char* bamdb_version(void);
const char* htslib_version(void);

int generate_index_wgs(char *input_file_name, char *output_file_name);
int generate_index_ss(char *input_file_name, char *output_file_name);

#endif
