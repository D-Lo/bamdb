#define _GNU_SOURCE

#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "htslib/sam.h"
#include "htslib/bgzf.h"

#include "bamdb.h"
#include "bam_lmdb.h"
#include "bam_api.h"

#define get_int_chars(i) ((i == 0) ? 1 : floor(log10(abs(i))) + 1)    // is this necessary? also in bam_api.c 

#define BAMDBVERSION "1.0"
#define HTSLIBVERSION "1.5"



const 
char *bamdb_version(){
    return BAMDBVERSION;
}

const 
char *htslib_version(){
    return HTSLIBVERSION;
}


static void usage(FILE *fp){
    fprintf(fp,
"\n"
"Program: bamdb (Software for indexing and querying 10x BAMs)\n"
"Version: %s (using htslib %s)\n\n", bamdb_version(), htslib_version());
    fprintf(fp,
"Usage:   bamdb index <required-flag>\n"
"\n"
"Required flags:\n"
"  WGS            index QNAME and BX tags for 10x WGS BAM\n"
"  single-cell    index QNAME, CB, and UB tags for single-cell BAM\n"
"\n");




int 
generate_index_wgs (char *input_file_name, char *output_file_name)
{
	samFile *input_file = 0;

	if ((input_file = sam_open(input_file_name, "r")) == 0) {
		fprintf(stderr, "Unable to open file %s\n", input_file_name);
		return 1;
	}

	return convert_to_lmdb_wgs(input_file, output_file_name);   // this should work for QNAME & BX
}

int 
generate_index_ss (char *input_file_name, char *output_file_name)
{
	samFile *input_file = 0;                                              // these checks also exit in bam_lmdb.c  REMOVE? 

	if ((input_file = sam_open(input_file_name, "r")) == 0) {               // these checks also exit in bam_lmdb.c  REMOVE? 
		fprintf(stderr, "Unable to open file %s\n", input_file_name);
		return 1;
	}

	return convert_to_lmdb_ss(input_file, output_file_name);   // this should WILL NOT work, must replace `convert_to_lmdb()` for QNAME, CB, UB
}




int 
main(int argc, char *argv[]){
	if (argc < 2) { usage(stderr); return 1; }
	if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0){
		usage(stdout); 
		return 0;
			if (row_set != NULL) {
                            for (size_t j = 0; j < row_set->n_entries; ++j) {
            	                print_sequence_row(row_set->rows[j]);
                            }
                            free_row_set(row_set);
                        }
                }
	}

	int rc = 0;
	if (strcmp(argv[1], "index WGS") == 0 || strcmp(argv[1], "index --WGS") == 0)                         rc = generate_index_wgs(argc-1, argv+1);   // QNAME, BX
	else if (strcmp(argv[1], "index signle-cell") == 0 || strcmp(argv[1], "index --single-cell") == 0)    rc = generate_index_ss(argc-1, argv+1);    // QNAME, CB, UB

	else if (strcmp(argv[1], "version") == 0 || strcmp(argv[1], "--version") == 0){
        printf(
"bamdb %s\n"
"Using htslib %s\n"
               bamdb_version(), htslib_version());
    }
    else {
        fprintf(stderr, "[main] unrecognized command '%s'\n", argv[1]);
        return 1;
    }


	return rc;
}

