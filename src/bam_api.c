#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "htslib/sam.h"
#include "htslib/bgzf.h"

#include "bam_api.h"

#define get_int_chars(i) ((i == 0) ? 1 : floor(log10(abs(i))) + 1)

const char *
bam_get_rname(const bam1_t *row, const bam_hdr_t *header)
{
	const char *rname;
	if (row->core.tid >= 0) {
		rname = header->target_name[row->core.tid];
	} else {
		rname = "*";
	}

	return rname;
}


const char *
bam_get_rnext(const bam1_t *row, const bam_hdr_t *header)
{
	char *rnext;
	if (row->core.mtid < 0) {
		rnext = "*";
	} else if (row->core.mtid == row->core.tid) {
		/* "=" to save space, could also use full reference name */
		rnext = "=";
	} else {
		rnext = header->target_name[row->core.mtid];
	}

	return rnext;
}


char *
bam_cigar_str(const bam1_t *row, char *work_buffer)
{
	char *ret = work_buffer;
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
		work_buffer += cigar_pos + 1;
	} else {
		strcpy(work_buffer, "*\0");
		work_buffer += 2;
	}

	return ret;
}


char *
bam_seq_str(const bam1_t *row, char *work_buffer)
{
	char *ret = work_buffer;
	int32_t l_qseq = row->core.l_qseq;
	if (l_qseq > 0) {
		uint8_t *seq = bam_get_seq(row);
		for (int i = 0; i < l_qseq; ++i) {
			/* Need to unpack base letter from its 4 bit encoding */
			work_buffer[i] = "=ACMGRSVTWYHKDBN"[bam_seqi(seq, i)];
		}
		work_buffer[l_qseq] = '\0';
		work_buffer += l_qseq + 1;
	} else {
		strcpy(work_buffer, "*\0");
		work_buffer += 2;
	}

	return ret;
}


char *
bam_qual_str(const bam1_t *row, char *work_buffer)
{
	char *ret = work_buffer;
	int32_t l_qseq = row->core.l_qseq;
	uint8_t *qual = bam_get_qual(row);
	if (qual[0] == 0xff || l_qseq <= 0) {
		/* No quality data, indicate with a '*' */
		strcpy(work_buffer, "*\0");
		work_buffer += 2;
	} else {
		for (int i = 0; i < l_qseq; ++i) {
			/* ASCII of base QUALity plus 33 */
			work_buffer[i] = qual[i] + 33;
		}
		work_buffer[l_qseq] = '\0';
		work_buffer += l_qseq + 1;
	}

	return ret;
}


char *
bam_str_key(const bam1_t *row, const char* key, char *work_buffer)
{
	uint8_t *aux;
	char *ret = work_buffer;
	uint8_t *bx_pos = 0;

	aux = bam_get_aux(row);

	if (strlen(key) == 2) {
		while (aux+4 <= row->data + row->l_data) {
			/* Format for string keys is **Z, Z being a format signifer */
			if (aux[0] == key[0] && aux[1] == key[1] && aux[2] == 'Z') {
				bx_pos = aux + 3;
				break;
			}
			aux++;
		}
	} else {
		fprintf(stderr, "Attempting to access a key (%s) that is not 2 characters long\n", key);
		return ret;
	}

	if (bx_pos != NULL) {
		ret = work_buffer;
		while (bx_pos < row->data + row->l_data && *bx_pos) {
			sprintf(work_buffer, "%c", *bx_pos++);
			work_buffer++;
		}
	} else {
		work_buffer[0] = '*';
		work_buffer[1] = '\0';
		work_buffer += 2;
	}

	return ret;
}


static void
populate_aux_tags(aux_list_t *list, const bam1_t *row)
{
	// XXX: do we need a working buffer?
	/* Tags are stored as TAGTYPEVALUE
	 * TAG is two characters
	 * TYPE is a single character
	 * Example: QTZAAFFFKKK which indicates a
	 * string tag of QT with a value of AAFFFKKK */
	uint8_t *aux;
	uint8_t type = 0, sub_type = 0;
	size_t aux_bytes = 0;
	uint32_t arr_size;
	char *dummy = 0;

	list->n_tags = 0;
	list->head = NULL;
	list->tail = NULL;

	aux = bam_get_aux(row);
	while (aux+4 <= row->data + row->l_data) {
		aux_elm_t *new_aux = calloc(1, sizeof(aux_elm_t));

		new_aux->key[0] = aux[0];
		new_aux->key[1] = aux[1];
		new_aux->type = aux[2];
		aux += 3;

		/* TODO: add error handling for values that don't conform to type */
		switch(new_aux->type) {
			case 'A': /* Printable character */
				new_aux->val = malloc(sizeof(char));
				memcpy(new_aux->val, aux, 1);
				aux++;
				break;
			case 'C': /* Unsigned 8 bit integer */
				new_aux->val = malloc(sizeof(uint8_t));
				memcpy(new_aux->val, aux, 1);
				aux++;
				break;
			case 'c': /* Signed 8 bit integer */
				new_aux->val = malloc(sizeof(int8_t));
				memcpy(new_aux->val, aux, 1);
				aux++;
				break;
			case 'S': /* Unsigned 16 bit integer */
				new_aux->val = malloc(sizeof(uint16_t));
				memcpy(new_aux->val, aux, 2);
				aux += 2;
				break;
			case 's': /* Signed 16 bit integer */
				new_aux->val = malloc(sizeof(int16_t));
				memcpy(new_aux->val, aux, 2);
				aux += 2;
				break;
			case 'I': /* Unsigned 32 bit integer */
				new_aux->val = malloc(sizeof(uint32_t));
				memcpy(new_aux->val, aux, 4);
				aux += 4;
				break;
			case 'i': /* Signed 32 bit integer */
				new_aux->val = malloc(sizeof(int32_t));
				memcpy(new_aux->val, aux, 4);
				aux += 4;
				break;
			case 'f': /* Single precision floating point */
				new_aux->val = malloc(sizeof(float));
				memcpy(new_aux->val, aux, 4);
				aux += 4;
				break;
			case 'd':
				/* Double precision floating point. This does appear to be in the BAM spec,
				 * I'm copying from samtools which does provide for this */
				new_aux->val = malloc(sizeof(float));
				memcpy(new_aux->val, aux, 4);
				aux += 4;
				break;
			case 'Z': /* Printable string */
			case 'H': /* Byte array */
				aux_bytes = 0;
				/* Determine size of value */
				while (aux < row->data + row->l_data && *(aux + aux_bytes)) {
					aux_bytes++;
				}
				new_aux->val = malloc(aux_bytes);
				memcpy(new_aux->val, aux, aux_bytes);
				aux += aux_bytes + 1; // XXX: padding byte?
				break;
// TODO: Implement support for array types 
#if 0
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
#endif
		}

		if (list->head == NULL) {
		    list->head = new_aux;
		}

		if (list->tail != NULL) {
		    list->tail->next = new_aux;
		}
		list->tail = new_aux;

		list->n_tags++;
	}

}


bam_sequence_row_t *
deserialize_bam_row(const bam1_t *row, const bam_hdr_t *header)
{
	bam_sequence_row_t *r = malloc(sizeof(bam_sequence_row_t));
	char *temp = malloc(WORK_BUFFER_SIZE);
	char *work_buffer = temp;

	r->qname = strdup(bam_get_qname(row));
	r->flag = row->core.flag;
	r->rname = strdup(bam_get_rname(row, header));
	r->pos = row->core.pos;
	r->mapq = row->core.qual;
	r->cigar = strdup(bam_cigar_str(row, work_buffer));
	work_buffer = temp;
	r->rnext = strdup(bam_get_rnext(row, header));
	r->pnext = row->core.mpos + 1;
	r->tlen = row->core.isize;
	r->seq = strdup(bam_seq_str(row, work_buffer));
	work_buffer = temp;
	r->qual = strdup(bam_qual_str(row, work_buffer));
	populate_aux_tags(&r->aux_list, row);

	return r;
}


bam_sequence_row_t *
get_bam_row(int64_t offset, samFile *input_file, bam_hdr_t *header)
{
	bam1_t *bam_row = bam_init1();
	int64_t src = 0;
	int r = 0;
	bam_sequence_row_t *ret = NULL;

	src = bgzf_seek(input_file->fp.bgzf, offset, SEEK_SET);
	r = sam_read1(input_file, header, bam_row);
	ret = deserialize_bam_row(bam_row, header);

	bam_destroy1(bam_row);
	return ret;
}


static void
print_bam_tags()
{


}


void
print_sequence_row(bam_sequence_row_t *row)
{
	aux_elm_t *aux = row->aux_list.head;

	printf("%s", row->qname);
	printf("\t%d", row->flag);
	printf("\t%s", row->rname);
	printf("\t%d", row->pos);
	printf("\t%d", row->mapq);
	printf("\t%s", row->cigar);
	printf("\t%s", row->rnext);
	printf("\t%d", row->pnext);
	printf("\t%d", row->tlen);
	printf("\t%s", row->seq);
	printf("\t%s", row->qual);

	while (aux != NULL) {
		printf("\t%c%c:", aux->key[0], aux->key[1]);

		switch(aux->type) {
			case 'A':
				printf("A:%c", *(char*)aux->val);
				break;
			case 'C':
				printf("i:%d", *(int*)aux->val);
				break;
			case 'c':
				printf("i:%" PRId8, *(int8_t*)aux->val);
				break;
			case 'S':
				printf("i:%" PRIu16, *(uint16_t*)aux->val);
				break;
			case 's':
				printf("i:%" PRId16, *(int16_t*)aux->val);
				break;
			case 'I':
				printf("i:%" PRIu32, *(uint32_t*)aux->val);
				break;
			case 'i':
				printf("i:%" PRId32, *(int32_t*)aux->val);
				break;
			case 'f':
				printf("f:%g", *(float*)aux->val);
				break;
			case 'd':
				printf("d:%g", *(float*)aux->val);
				break;
			case 'Z':
			case 'H':
				printf("%c:%s", aux->type, (char*)aux->val);
				break;
		}

		aux = aux->next;
	}

	printf("\n");
}


void
destroy_bam_sequence_row(bam_sequence_row_t *row)
{
	free(row->qname);
	free(row->rname);
	free(row->cigar);
	free(row->rnext);
	free(row);
}


void
free_row_set(bam_row_set_t *row_set)
{
	for (int i = 0; i < row_set->n_entries; ++i) {
		free(row_set->rows[i]);
	}
	free(row_set);
}
