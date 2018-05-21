#define _GNU_SOURCE

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bam_api.h"
#include "bam_lmdb.h"
#include "bamdb.h"

// Return number of characters an unsigned int takes when represented in base 10
#define get_int_chars(i) ((i == 0) ? 1 : floor(log10(i)) + 1)

static void get_bam_tags(const bam1_t *row, char *buffer) {
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

  aux = bam_get_aux(row);
  while (aux + 4 <= row->data + row->l_data) {
    key[0] = aux[0];
    key[1] = aux[1];
    sprintf(buffer + buffer_pos, "%c%c:", key[0], key[1]);
    buffer_pos += 3;

    type = aux[2];
    aux += 3;

    /* TODO: add error handling for values that don't conform to type */
    switch (type) {
      case 'A': /* Printable character */
        sprintf(buffer + buffer_pos, "A:%c", *aux);
        buffer_pos += 3;
        aux++;
        break;
      case 'C': /* Signed integer */
        sprintf(buffer + buffer_pos, "i:%d", *aux);
        buffer_pos += 2 + get_int_chars(*aux);
        aux++;
        break;
      case 'c':
        sprintf(buffer + buffer_pos, "i:%" PRId8, *(int8_t *)aux);
        buffer_pos += 2 + get_int_chars(*aux);
        aux++;
        break;
      case 'S':
        sprintf(buffer + buffer_pos, "i:%" PRIu16, *(uint16_t *)aux);
        buffer_pos += 2 + get_int_chars(*aux);
        aux += 2;
        break;
      case 's':
        sprintf(buffer + buffer_pos, "i:%" PRId16, *(int16_t *)aux);
        buffer_pos += 2 + get_int_chars(*aux);
        aux += 2;
        break;
      case 'I':
        sprintf(buffer + buffer_pos, "i:%" PRIu32, *(uint32_t *)aux);
        buffer_pos += 2 + get_int_chars(*aux);
        aux += 4;
        break;
      case 'i':
        sprintf(buffer + buffer_pos, "i:%" PRId32, *(int32_t *)aux);
        buffer_pos += 2 + get_int_chars(*aux);
        aux += 4;
        break;
      case 'f': /* Single precision floating point */
        sprintf(buffer + buffer_pos, "f:%g", *(float *)aux);
        /* Figure out how many chars the fp takes as a string */
        buffer_pos += 2 + snprintf(dummy, 0, "%g", *(float *)aux);
        aux += 4;
        break;
      case 'd':
        /* Double precision floating point. This does appear to be in the BAM
         * spec,
         * I'm copying from samtools which does provide for this */
        sprintf(buffer + buffer_pos, "d:%g", *(float *)aux);
        /* Figure out how many chars the fp takes as a string */
        buffer_pos += 2 + snprintf(dummy, 0, "%g", *(float *)aux);
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
              sprintf(buffer + buffer_pos, "%" PRId8, *(int8_t *)aux);
              buffer_pos += get_int_chars(*aux);
              aux++;
              break;
            case 'S':
              sprintf(buffer + buffer_pos, "%" PRIu16, *(uint16_t *)aux);
              buffer_pos += get_int_chars(*aux);
              aux += 2;
              break;
            case 's':
              sprintf(buffer + buffer_pos, "%" PRId16, *(int16_t *)aux);
              buffer_pos += get_int_chars(*aux);
              aux += 2;
              break;
            case 'I':
              sprintf(buffer + buffer_pos, "i:%" PRIu32, *(uint32_t *)aux);
              buffer_pos += 2 + get_int_chars(*aux);
              aux += 4;
              break;
            case 'i':
              sprintf(buffer + buffer_pos, "i:%" PRId32, *(int32_t *)aux);
              buffer_pos += 2 + get_int_chars(*aux);
              aux += 4;
              break;
            case 'f': /* Single precision floating point */
              sprintf(buffer + buffer_pos, "f:%g", *(float *)aux);
              /* Figure out how many chars the fp takes as a string */
              buffer_pos += 2 + snprintf(dummy, 0, "%g", *(float *)aux);
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

static int print_bam_row(const bam1_t *row, const bam_hdr_t *header,
                         char *work_buffer) {
  static uint32_t rows = 0;
  char *temp;

  printf("Row %u:\n", rows);
  printf("\tQNAME: %s\n", bam_get_qname(row));
  printf("\tFLAG: %u\n", row->core.flag);
  printf("\tRNAME: %s\n", bam_get_rname(row, header));
  printf("\tPOS: %d\n", row->core.pos);
  printf("\tMAPQ: %u\n", row->core.qual);

  temp = work_buffer;
  printf("\tCIGAR: %s\n", bam_cigar_str(row, work_buffer));
  work_buffer = temp;

  printf("\tRNEXT: %s\n", bam_get_rnext(row, header));
  printf("\tPNEXT: %d\n", row->core.mpos + 1);
  printf("\tTLEN: %d\n", row->core.isize);

  printf("\tSEQ: %s\n", bam_seq_str(row, work_buffer));
  work_buffer = temp;

  printf("\tQUAL: %s\n", bam_qual_str(row, work_buffer));
  work_buffer = temp;

  printf("\tBX: %s\n", bam_str_key(row, "BX", work_buffer));
  work_buffer = temp;

  /* TAGs */
  get_bam_tags(row, work_buffer);
  printf("\tTAGs: %s\n", work_buffer);

  rows++;
  return 0;
}

static int read_file(samFile *input_file, offset_list_t *offset_list) {
  bam_hdr_t *header = NULL;
  bam1_t *bam_row;
  char *work_buffer = NULL;
  int r = 0;
  int rc = 0;
  int64_t src = 0;
  offset_node_t *offset_node;

  header = sam_hdr_read(input_file);
  if (header == NULL) {
    fprintf(stderr, "Unable to read the header from %s\n", input_file->fn);
    rc = 1;
    return 1;
  }

  bam_row = bam_init1();
  work_buffer = malloc(WORK_BUFFER_SIZE);
  if (offset_list != NULL) {
    offset_node = offset_list->head;
    while (offset_node != NULL) {
      src = bgzf_seek(input_file->fp.bgzf, offset_node->offset, SEEK_SET);
      if (src != 0) {
        fprintf(stderr, "Error seeking to file offset\n");
        rc = 1;
        goto exit;
      }

      r = sam_read1(input_file, header, bam_row);
      print_bam_row(bam_row, header, work_buffer);
      offset_node = offset_node->next;
    }
  } else {
    while ((r = sam_read1(input_file, header, bam_row)) >=
           0) {  // read one alignment from `in'
      print_bam_row(bam_row, header, work_buffer);
    }
  }
  if (r < -1) {
    fprintf(stderr, "Attempting to process truncated file.\n");
    rc = 1;
    goto exit;
  }

exit:
  free(work_buffer);
  bam_destroy1(bam_row);
  return rc;
}

int write_row_subset(char *input_file_name, offset_list_t *offset_list,
                     char *out_filename) {
  int rc = 0;
  int64_t src = 0;
  bam1_t *bam_row;
  offset_node_t *offset_node;
  bam_hdr_t *new_header = NULL;
  bam_hdr_t *header = NULL;
  BGZF *fp = bgzf_open(out_filename, "w");
  samFile *input_file = 0;

  if ((input_file = sam_open(input_file_name, "r")) == 0) {
    fprintf(stderr, "Unable to open file %s\n", input_file_name);
    return 1;
  }

  header = sam_hdr_read(input_file);
  if (header == NULL) {
    fprintf(stderr, "Unable to read the header from %s\n", input_file->fn);
    rc = 1;
    return 1;
  }

  new_header = bam_hdr_dup(header);
  rc = bam_hdr_write(fp, new_header);
  if (rc != 0) {
    fprintf(stderr, "Unable to write header for %s\n", out_filename);
    return rc;
  }

  bam_row = bam_init1();
  if (offset_list != NULL) {
    offset_node = offset_list->head;
    while (offset_node != NULL) {
      src = bgzf_seek(input_file->fp.bgzf, offset_node->offset, SEEK_SET);
      if (src != 0) {
        fprintf(stderr, "Error seeking to file offset\n");
        return 1;
      }

      rc = sam_read1(input_file, header, bam_row);
      rc = bam_write1(fp, bam_row);
      if (rc < 0) {
        fprintf(stderr, "Error writing row to file %s\n", out_filename);
        return rc;
      }

      offset_node = offset_node->next;
    }
  }

  bgzf_close(fp);
  return rc;
}

int generate_index_file(char *input_file_name, char *output_file_name) {
  samFile *input_file = 0;
  indices_t target_indices = {.includes_qname = true,
                              .num_key_indices = 1,
                              .key_indices = malloc(sizeof(char *))};

  target_indices.key_indices[0] = malloc(3);
  strncpy(target_indices.key_indices[0], "BX", 2);

  if ((input_file = sam_open(input_file_name, "r")) == 0) {
    fprintf(stderr, "Unable to open file %s\n", input_file_name);
    return 1;
  }

  return convert_to_lmdb(input_file, output_file_name, &target_indices);
}

int main(int argc, char *argv[]) {
  int rc = 0;
  int c;
  bam_args_t bam_args;
  int max_rows = 0;

  bam_args.index_file_name = NULL;
  bam_args.bx = NULL;
  bam_args.output_file_name = NULL;
  bam_args.convert_to = BAMDB_CONVERT_TO_TEXT;
  while ((c = getopt(argc, argv, "t:f:n:i:b:o:")) != -1) {
    switch (c) {
      case 't':
        if (strcmp(optarg, "lmdb") == 0) {
          bam_args.convert_to = BAMDB_CONVERT_TO_LMDB;
        } else if (strcmp(optarg, "text") == 0) {
          bam_args.convert_to = BAMDB_CONVERT_TO_TEXT;
        } else {
          fprintf(stderr, "Invalid output format %s\n", optarg);
          return 1;
        }
        break;
      case 'f':
        strcpy(bam_args.input_file_name, optarg);
        break;
      case 'n':
        max_rows = atoi(optarg);
        break;
      case 'i':
        bam_args.index_file_name = strdup(optarg);
        break;
      case 'b':
        bam_args.bx = strdup(optarg);
        break;
      case 'o':
        bam_args.output_file_name = strdup(optarg);
        break;
      default:
        fprintf(stderr, "Unknown argument\n");
        return 1;
    }
  }

  /* Get filename from first non option argument */
  if (optind < argc) {
    strcpy(bam_args.input_file_name, argv[optind]);
  }

  if (bam_args.bx != NULL && bam_args.index_file_name != NULL) {
    if (bam_args.output_file_name != NULL) {
      /* Write resulting rows to file */
      offset_list_t *offset_list = calloc(1, sizeof(offset_list_t));

      rc = get_offsets_lmdb(offset_list, bam_args.index_file_name, "BX",
                            bam_args.bx);
      rc = write_row_subset(bam_args.input_file_name, offset_list,
                            bam_args.output_file_name);
      free(offset_list);
    } else {
      /* Print rows in tab delim format */
      bam_row_set_t *row_set = NULL;
      rc = get_bam_rows(&row_set, bam_args.input_file_name,
                        bam_args.index_file_name, "BX", bam_args.bx);

      if (row_set != NULL) {
        for (size_t j = 0; j < row_set->num_entries; ++j) {
          print_sequence_row(row_set->rows[j]);
        }
        free_bamdb_row_set(row_set);
      }
    }
  }
}

void print_bx_rows(char **input_file_name, char **db_path, char **bx) {
  int rc = 0;
  bam_row_set_t *row_set = NULL;
  rc = get_bam_rows(&row_set, *input_file_name, *db_path, "BX", *bx);

  if (row_set != NULL) {
    for (size_t j = 0; j < row_set->num_entries; ++j) {
      print_sequence_row(row_set->rows[j]);
    }
  }
}
