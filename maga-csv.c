/**
 * gawk csv parser
 * (part of the 'make awk great again' project)
 *
 * this extension parses CSV files natively (libcsv) without using FPAT.
 * in our tests this has reduced runtime considerably (approx 20x).
 * currently this extension requires FS to be set to "\31".
 *
 * internally it uses a queue of row buffers to convert the output of
 * libcsv to the gawk format.
 */

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
#include <sys/param.h>

#include "gawkapi.h"

#define READ_SZ (1024 * 1024)
#define ROW_INITIAL_CAPACITY (100)
static /*const*/ char RT_START = '\31';
static const int RT_LEN = 1;

/* Boilerplate code: */
int plugin_is_GPL_compatible;

typedef struct row{
  size_t capacity;
  size_t length;
  char* text;
} row_t;

struct row_queue {
  size_t begin;
  size_t end;
  row_t rows[READ_SZ];
};

struct row_cb_data {
  row_t row;
  struct row_queue *rq;
};

struct csv_state {
  struct csv_parser *parser;
  char *read_buffer;
  struct row_queue *row_queue;
  struct row_cb_data rcbd; /* row callback data */
  char* rt_start; /* row terminator */
  int rt_len; /* row terminator length */
  char *out_to_free; /* text buffer of previous iteration */
};


static const gawk_api_t *api;
static awk_ext_id_t ext_id;
static const char *ext_version = "1.0";

static struct row_queue* row_queue_new() {
  struct row_queue* rq = gawk_malloc(sizeof (struct row_queue));
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
  row_t row = rq->rows[rq->begin];
  rq->begin++;
  assert(rq->begin <= READ_SZ);
  if (rq->begin == READ_SZ) {
    rq->begin = 0;
  }
  return row;
}

static void row_queue_push_back(struct row_queue* rq, row_t* row) {
  //fprintf(stderr, "rq push\n");
  rq->rows[rq->end] = *row;
  rq->end++;
  assert(rq->end <= READ_SZ);
  if (rq->end == READ_SZ) {
    rq->end = 0;
  }
  //fprintf(stderr, "rq push end\n");
}

static bool row_queue_empty(struct row_queue* rq) {
  return rq->begin == rq->end;
}

static row_t row_new(size_t capacity) {
  assert(capacity > 0);
  row_t rb = {
    .capacity = capacity,
    .length = 0,
    .text = gawk_malloc(capacity)
  };
  //fprintf(stderr, "new row: %p\n", rb.text);
  return rb;
}

static void row_append(row_t *rb, char* s, size_t len) {
  if (rb->length + len > rb->capacity) {
    const size_t new_capacity = MAX(len, rb->capacity) * 2;
    rb->text = gawk_realloc(rb->text, new_capacity);
    rb->capacity = new_capacity;
  }
  memcpy(rb->text+ rb->length, s, len);
  rb->length += len;
}

static void field_collect(void* str, size_t str_len, void* data) {
  //fprintf(stderr, "collect: \"%s\"\n", (char*)str);
  struct row_cb_data* rcbd = (struct row_cb_data*)data;
  if (rcbd->row.capacity == 0) {
    rcbd->row = row_new(ROW_INITIAL_CAPACITY);
  }
  if (rcbd->row.length > 0) {
    row_append(&rcbd->row, "\31", 1);
  }
  row_append(&rcbd->row, str, str_len);
}

static void row_collect(int c, void* data) {
  //fprintf(stderr, "row collect\n");
  struct row_cb_data* rcbd = (struct row_cb_data*)data;
  row_queue_push_back(rcbd->rq, &rcbd->row);
  //fprintf(stderr, "row collect end: %d, %s\n", c, rcbd->row.text);
  rcbd->row = row_new(ROW_INITIAL_CAPACITY);
}

static int emit_record(struct csv_state *state, char** out, row_t row, char** rt_start, size_t *rt_len) {
  *rt_start = &RT_START;
  *rt_len = RT_LEN;

  *out = row.text;
  state->out_to_free = *out;
  //fprintf(stderr, "row: %p, %p, %p, %p\n", state->out_to_free, *(state->out_to_free), out, (void *)*out);
  fflush(stderr);
  return row.length;
}

static int csv_get_record(char **out, struct awk_input *iobuf, int *errcode, char **rt_start, size_t *rt_len) {
  //fprintf(stderr, "get_record\n");
  struct csv_state *state = (struct csv_state *)iobuf->opaque;

  /* free row of previous run */
  if(state->out_to_free != NULL) {
    //fprintf(stderr, "freeing row: %p\n", state->out_to_free);
    gawk_free(state->out_to_free);
  }

  // maybe keep one buffer in opaque struct...
  if(!row_queue_empty(state->row_queue)) {
    //fprintf(stderr, "rq not empty\n");
    row_t row = row_queue_pop_front(state->row_queue);
    //fprintf(stderr, "rq popped\n");
    // when do we free row!? maye gawk does it... ?
    return emit_record(state, out, row, rt_start, rt_len);
  }

  size_t buflen = 0;
  //fprintf(stderr, "before loop\n");
  while((buflen = read(iobuf->fd, state->read_buffer, READ_SZ))  > 0){
    //fprintf(stderr, "parse\n");
    csv_parse(state->parser, state->read_buffer, buflen, field_collect, row_collect, &state->rcbd);
    //fprintf(stderr, "after parse\n");
    // if have data, emit; return.
    if(!row_queue_empty(state->row_queue)) {
      //fprintf(stderr, "in loop, popping and emitting record.\n");
      row_t row = row_queue_pop_front(state->row_queue);
      return emit_record(state, out, row, rt_start, rt_len);
    }
  }

  return EOF;
}

static awk_bool_t
csv_can_take_file(const awk_input_buf_t *iobuf) {
  //fprintf(stderr, "csv_can_take_file()\n");

  if (iobuf == NULL)
    return awk_false;

  return (iobuf->fd != INVALID_HANDLE);
}

static void
csv_close(awk_input_buf_t *iobuf) {
  struct csv_state *state = (struct csv_state *)iobuf->opaque;
  gawk_free(state->read_buffer);
  csv_free(state->parser);
  row_queue_destroy(state->row_queue);
  gawk_free(state);
}

static awk_bool_t
csv_take_control_of(awk_input_buf_t *iobuf) {
  // we would set the FS to 31 here, but gawk doesn't allow it. qq
  //fprintf(stderr, "opening %s...\n", iobuf->name);
  if(iobuf->fd == INVALID_HANDLE) {
    return 1;
  }

  struct csv_state *state = gawk_malloc(sizeof(struct csv_state));
  // setup parser
  state->parser = gawk_malloc(sizeof(struct csv_parser));
  memset(state->parser, 0, sizeof(struct csv_parser));
  csv_init(state->parser, CSV_APPEND_NULL);
  // setup buffer
  state->read_buffer = gawk_malloc(READ_SZ);
  // setup row_queue
  state->row_queue = row_queue_new();
  // setup callback structure
  struct row_cb_data rcbd = {{0}};
  rcbd.rq = state->row_queue;
  state->rcbd = rcbd;
  state->out_to_free = NULL;

  //fprintf(stderr, "after read...\n");
  iobuf->opaque = state;
  iobuf->get_record = csv_get_record;
  iobuf->close_func= csv_close;
  //fprintf(stderr, "returning...\n");
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

dl_load_func(func_table, readdir, "")
