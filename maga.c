#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "gawkapi.h"

/* Boilerplate code: */
int plugin_is_GPL_compatible;

static gawk_api_t *const api;
static awk_ext_id_t ext_id;
static const char *ext_version = "1.0";

static awk_bool_t
init_my_extension(void)
{
  return 1;
}

static awk_bool_t (*init_func)(void) = init_my_extension;

static awk_input_parser_t parser = {
  .name = "csv",
  .can_take_file = NULL,
  .take_control_of = NULL,
};

static awk_bool_t
csv_can_take(const awk_input_buf_t *iobuf) {
  return 1;
}

static awk_bool_t
csv_control(awk_input_buf_t *iobuf) {
  return 1;
}
