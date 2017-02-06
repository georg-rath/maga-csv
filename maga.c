#include <csv.h>

#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "gawkapi.h"

#define READ_SZ (1024)

/* Boilerplate code: */
int plugin_is_GPL_compatible;

// each field is a an array of array of strings.
typedef char* row_t;
// each queue of
typedef row_t* rows_t;

struct row_queue {
  size_t begin;
  size_t end;
  row_t rows[READ_SZ]; // should this be rows*?
};

struct csv_state {
  struct csv_parser *parser;
  char *read_buffer;
  size_t read_buffer_len;
  struct row_queue *row_queue;
};

static struct row_queue* row_queue_new() {
  struct row_queue* rq = malloc(sizeof (struct row_queue));
  rq->begin = 0;
  rq->end = 0;
  memset(rq->rows, 0, READ_SZ*sizeof(row_t));
  return rq;
}

static void row_queue_destroy(struct row_queue* rq) {
  // TODO: delete stuff inside...
  free(rq);
}

static row_t row_queue_pop_front(struct row_queue* rq) {
  row_t* row = &rq->rows[rq->begin];
  rq->begin++;
  if (rq->begin >= READ_SZ) {
    rq->begin -= READ_SZ;
  }
  return *row;
}

static void row_queue_push_back(struct row_queue* rq, row_t* row) {
  rq->rows[rq->end] = *row;
  rq->end++;
  if (rq->end >= READ_SZ) {
    rq->end -= READ_SZ;
  }
}

static bool row_queue_empty(struct row_queue* rq) {
  return rq->begin == rq->end;
}

struct row_cb_data {
  row_t row; // something here to collect fields so they can be pushed
  struct row_queue rq;
};

void field_collect(void* str, size_t str_len, void* data) {
  fprintf(stderr, "collect\n");
  // TODO: make this grow with *2 or *1.4 so we dont have to reallocate and copy so much.
  struct row_cb_data* rcbd = (struct row_cb_data*)data;
/*  size_t len = str_len + 1 + 1; // separator + null terminator. or do we need null terminator.
  if (rcbd->row != NULL) {
    len += 10;
  }
  row_t new_row = malloc(len);
  snprintf(new_row, len, "%s%c%s", rcbd->row, 31, (char*)str);
  //free(rcbd->row);
  rcbd->row = new_row;
  */
  rcbd->row = "hello\31hello";
}

void row_collect(int c, void* data) {
  fprintf(stderr, "row collect\n");
  struct row_cb_data* rcbd = (struct row_cb_data*)data;
  row_queue_push_back(&rcbd->rq, &rcbd->row);
  fprintf(stderr, "row collect end: %s\n");
}

int csv_get_record(char **out, struct awk_input *iobuf, int *errcode, char **rt_start, size_t *rt_len) {
  fprintf(stderr, "get_record\n");
  struct csv_state *state = (struct csv_state *)iobuf->opaque;
  // maybe keep one buffer in opaque struct...
  if(!row_queue_empty(state->row_queue)) {
    fprintf(stderr, "rq not empty\n");
    row_t row = row_queue_pop_front(state->row_queue);
    fprintf(stderr, "rq popped\n");
    // when do we free row!? maye gawk does it... ?
    *out = row;
    return true;
  }
  // read buffer
  // push fields onto stack
  // on rowcallback, flush row
  // then commit record to awk.
  struct row_cb_data rcbd = {0};
  size_t buflen = 0;
  fprintf(stderr, "before loop\n");
  while((buflen = read(iobuf->fd, state->read_buffer, READ_SZ))  > 0){
    fprintf(stderr, "parse\n");
    csv_parse(state->parser, state->read_buffer, state->read_buffer_len, field_collect, row_collect, &rcbd);
    fprintf(stderr, "after parse\n");
    // if have data, emit; return.
  }
  *rt_start = '\31';
  *rt_len = 1;
  return 11;
}

static const gawk_api_t *api;
static awk_ext_id_t ext_id;
static const char *ext_version = "1.0";

static awk_bool_t
csv_can_take_file(const awk_input_buf_t *iobuf) {
  fprintf(stderr, "csv_can_take_file()\n");

  if (iobuf == NULL)
    return awk_false;

  return (iobuf->fd != INVALID_HANDLE);
}

static awk_bool_t
csv_take_control_of(awk_input_buf_t *iobuf) {
  fprintf(stderr, "opening %s...\n", iobuf->name);
  if(iobuf->fd == INVALID_HANDLE) {
    return 1;
  }

  struct csv_state *state = gawk_malloc(sizeof(struct csv_state));
  // setup parser
  state->parser = gawk_malloc(sizeof(struct csv_parser));
  csv_init(state->parser, CSV_APPEND_NULL);
  // setup buffer
  state->read_buffer = gawk_malloc(READ_SZ);
  state->read_buffer_len = READ_SZ;
  // setup row_queue
  state->row_queue = row_queue_new();
  fprintf(stderr, "after read...\n");
  iobuf->opaque = state;
  iobuf->get_record = csv_get_record;
  fprintf(stderr, "returning...\n");
  fflush(stderr);
  return awk_true;
}

static awk_input_parser_t csv_parser = {
  .name = "csv",
  .can_take_file = csv_can_take_file,
  .take_control_of = csv_take_control_of,
};

  static awk_bool_t
init_csv(void)
{
  register_input_parser(&csv_parser);
  return 1;
}

static awk_bool_t (*init_func)(void) = init_csv;

static awk_ext_func_t func_table[] = { 
  { NULL, NULL, 0, 0, awk_false, NULL }
};

dl_load_func(func_table, readdir, "");
