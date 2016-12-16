#define _GNU_SOURCE

#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "htslib/sam.h"

/* I really hope we don't have sequences longer than this */
#define WORK_BUFFER_SIZE 65536

#define get_int_chars(i) ((i == 0) ? 1 : floor(log10(abs(i))) + 1)


static void
get_bam_tags(const bam1_t *row, char *buffer)
{
	/* Output as TAG:TYPE:VALUE
	 * TAG is two characters
	 * TYPE is a single character
	 * Example: QT:Z:AAFFFKKK */
	uint8_t *aux;
	uint8_t key[2];
	uint8_t type, sub_type;
	size_t buffer_pos = 0;
	uint32_t arr_size;
	char *dummy = 0;
	int foo = 0;

	aux = bam_get_aux(row);
	while (aux+4 <= row->data + row->l_data) {
		foo++;
		key[0] = aux[0];
		key[1] = aux[1];
		sprintf(buffer + buffer_pos, "%c%c:", key[0], key[1]);
		buffer_pos += 3;

		type = aux[2];
		aux += 3;

		/* TODO: add error handling for values that don't conform to type */
		switch(type) {
			case 'A': /* Printable character */
				sprintf(buffer + buffer_pos, "A:%c", *aux);
				buffer_pos += 3;
				aux++;
				break;
			case 'C': /*Signed integer */
				sprintf(buffer + buffer_pos, "i:%d", *aux);
				buffer_pos += 2 + get_int_chars(*aux);
				aux++;
				break;
			case 'c':
				sprintf(buffer + buffer_pos, "i:%" PRId8, *(int8_t*)aux);
				buffer_pos += 2 + get_int_chars(*aux);
				aux++;
				break;
			case 'S':
				sprintf(buffer + buffer_pos, "i:%" PRIu16, *(uint16_t*)aux);
				buffer_pos += 2 + get_int_chars(*aux);
				aux += 2;
				break;
			case 's':
				sprintf(buffer + buffer_pos, "i:%" PRId16, *(int16_t*)aux);
				buffer_pos += 2 + get_int_chars(*aux);
				aux += 2;
				break;
			case 'I':
				sprintf(buffer + buffer_pos, "i:%" PRIu32, *(uint32_t*)aux);
				buffer_pos += 2 + get_int_chars(*aux);
				aux += 4;
				break;
			case 'i':
				sprintf(buffer + buffer_pos, "i:%" PRId32, *(int32_t*)aux);
				buffer_pos += 2 + get_int_chars(*aux);
				aux += 4;
				break;
			case 'f': /* Single precision floating point */
				sprintf(buffer + buffer_pos, "f:%g", *(float*)aux);
				/* Figure out how many chars the fp takes as a string */
				buffer_pos += 2 + snprintf(dummy, 0, "%g", *(float*)aux);
				aux += 4;
				break;
			case 'd':
				/* Double precision floating point. This does appear to be in the BAM spec,
				 * I'm copying from samtools which does provide for this */
				sprintf(buffer + buffer_pos, "d:%g", *(float*)aux);
				/* Figure out how many chars the fp takes as a string */
				buffer_pos += 2 + snprintf(dummy, 0, "%g", *(float*)aux);
				aux += 4;
				break;
			case 'Z': /* Printable string */
			case 'H': /* Byte array */
				sprintf(buffer + buffer_pos, "%c:", type);
				buffer_pos += 2;
				while (aux < row->data + row->l_data && *aux) {
					sprintf(buffer + buffer_pos, "%c", *aux++);
					buffer_pos++;
				}
				aux++;
				break;
			case 'B': /* Integer or numeric array */
				sub_type = *(aux++);
				memcpy(&arr_size, aux, 4);

				sprintf(buffer + buffer_pos, "B:%c", sub_type);
				buffer_pos += 3;
				for (int i = 0; i < arr_size; ++i) {
					sprintf(buffer + buffer_pos, ",");
					buffer_pos++;
					switch (sub_type) {
						case 'c':
							sprintf(buffer + buffer_pos, "%d", *aux);
							buffer_pos += get_int_chars(*aux);
							aux++;
							break;
						case 'C':
							sprintf(buffer + buffer_pos, "%" PRId8, *(int8_t*)aux);
							buffer_pos += get_int_chars(*aux);
							aux++;
							break;
						case 'S':
							sprintf(buffer + buffer_pos, "%" PRIu16, *(uint16_t*)aux);
							buffer_pos += get_int_chars(*aux);
							aux += 2;
							break;
						case 's':
							sprintf(buffer + buffer_pos, "%" PRId16, *(int16_t*)aux);
							buffer_pos += get_int_chars(*aux);
							aux += 2;
							break;
						case 'I':
							sprintf(buffer + buffer_pos, "i:%" PRIu32, *(uint32_t*)aux);
							buffer_pos += 2 + get_int_chars(*aux);
							aux += 4;
							break;
						case 'i':
							sprintf(buffer + buffer_pos, "i:%" PRId32, *(int32_t*)aux);
							buffer_pos += 2 + get_int_chars(*aux);
							aux += 4;
							break;
						case 'f': /* Single precision floating point */
							sprintf(buffer + buffer_pos, "f:%g", *(float*)aux);
							/* Figure out how many chars the fp takes as a string */
							buffer_pos += 2 + snprintf(dummy, 0, "%g", *(float*)aux);
							aux += 4;
							break;
					}
				}
				break;
		}

		sprintf(buffer + buffer_pos, "\t");
		buffer_pos++;
	}
}


static int
get_bam_row(const bam1_t *row, const bam_hdr_t *header, char *work_buffer)
{
	static uint32_t rows = 0;

	printf("Row %u:\n", rows);

	/* QNAME */
	const char *qname = bam_get_qname(row);
	printf("\tQNAME: %s\n", qname);

	/* FLAG */
	uint32_t flag = row->core.flag;
	printf("\tFLAG: %u\n", flag);

	/* RNAME */
	const char *rname;
	if (row->core.tid >= 0) {
		rname = header->target_name[row->core.tid];
	} else {
		rname = "*";
	}
	printf("\tRNAME: %s\n", rname);

	/* POS */
	int32_t pos = row->core.pos;
	printf("\tPOS: %d\n", pos);

	/* MAPQ */
	printf("\tMAPQ: %u\n", row->core.qual);

	/* CIGAR */
	/* CIGAR is an array of uint32s. First 4 bits are the CIGAR opration
	 * and the following 28 bits are the number of repeats of the op */
	if (row->core.n_cigar > 0) {
		uint32_t *cigar = bam_get_cigar(row);
		size_t cigar_pos = 0;

		for (int i = 0; i < row->core.n_cigar; ++i) {
			int num_ops = bam_cigar_oplen(cigar[i]);
			char op = bam_cigar_opchr(cigar[i]);
			sprintf(work_buffer + cigar_pos, "%d%c", num_ops, op);
			/* Need enough space for the opcount and the actual opchar
			 * an example would be something like 55F */
			cigar_pos += get_int_chars(num_ops) + 1;
		}

		work_buffer[cigar_pos] = '\0';
	} else {
		strcpy(work_buffer, "*\0");
	}
	printf("\tCIGAR: %s\n", work_buffer);

	/* RNEXT */
	char *rnext;
	if (row->core.mtid < 0) {
		rnext = "*";
	} else if (row->core.mtid == row->core.tid) {
		/* "=" to save space, could also use full reference name */
		rnext = "=";
	} else {
		rnext = header->target_name[row->core.mtid];
	}
	printf("\tRNEXT: %s\n", rnext);

	/* PNEXT */
	int32_t pnext = row->core.mpos + 1;
	printf("\tPNEXT: %d\n", pnext);

	/* TLEN */
	int32_t tlen = row->core.isize;
	printf("\tTLEN: %d\n", tlen);

	/* SEQ */
	int32_t l_qseq = row->core.l_qseq;
	if (l_qseq > 0) {
		uint8_t *seq = bam_get_seq(row);
		for (int i = 0; i < l_qseq; ++i) {
			/* Need to unpack base letter from its 4 bit encoding */
			work_buffer[i] = "=ACMGRSVTWYHKDBN"[bam_seqi(seq, i)];
		}
		work_buffer[l_qseq] = '\0';
		printf("\tSEQ: %s\n", work_buffer);
	} else {
		printf("\tSEQ: *\n");
	}


	/* QUAL */
	uint8_t *qual = bam_get_qual(row);
	if (qual[0] == 0xff || l_qseq <= 0) {
		/* No quality data, indicate with a '*' */
		strcpy(work_buffer, "*\0");
	} else {
		for (int i = 0; i < l_qseq; ++i) {
			/* ASCII of base QUALity plus 33 */
			work_buffer[i] = qual[i] + 33;
		}
		work_buffer[l_qseq] = '\0';
	}
	printf("\tQUAL: %s\n", work_buffer);

	/* TAGs */
	get_bam_tags(row, work_buffer);
	printf("\tTAGs: %s\n", work_buffer);

	rows++;
	return 0;
}


int
main(int argc, char *argv[]) {
	int r = 0;
	char *in_file_name = 0;
	char *work_buffer;
	samFile *input_file = 0;

	bam_hdr_t *header = NULL;
	bam1_t *bam_row;

	/* get input file name from trailing arg */
	in_file_name = (optind < argc) ? argv[optind] : "-";
	if ((input_file = sam_open(in_file_name, "r")) == 0) {
		fprintf(stderr, "Unable to open file %s\n", in_file_name);
		return 1;
	}

	header = sam_hdr_read(input_file);
	if (header == NULL) {
		fprintf(stderr, "Unable to read the header from %s\n", in_file_name);
		return 1;
	}

	bam_row = bam_init1();
	work_buffer = malloc(WORK_BUFFER_SIZE);
	while ((r = sam_read1(input_file, header, bam_row)) >= 0) { // read one alignment from `in'
		get_bam_row(bam_row, header, work_buffer);
	}
	if (r < -1) {
		fprintf(stderr, "Attempting to process truncated file.\n");
		return 1;
	}
	free(work_buffer);
	bam_destroy1(bam_row);

	return 0;
}
