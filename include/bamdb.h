#ifndef BAM_H
#define BAM_H

#define MAX_FILENAME 1024

const char* bamdb_version(void);
const char* htslib_version(void);

<<<<<<< HEAD
int generate_index_wgs(char *input_file_name, char *output_file_name);
int generate_index_ss(char *input_file_name, char *output_file_name);
=======
typedef struct bamdb_args {
	enum bamdb_convert_to convert_to;
	char input_file_name[MAX_FILENAME];
	char *index_file_name;
	char *output_file_name;
	char *bx;
} bam_args_t;

int generate_index_file(char *input_file_name, char *output_file_name);
void print_bx_rows(char **input_file_name, char **db_path, char **bx);

>>>>>>> c5c31b5cbe2af0aae58f195879b3a53792693018

#endif
