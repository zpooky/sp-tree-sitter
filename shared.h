#ifndef SP_TS_SHARED_H
#define SP_TS_SHARED_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include <tree_sitter/api.h>

/* ======================================== */
struct sp_ts_file {
  char *content;
  size_t length;
  int fd;
};

/* ======================================== */
enum sp_ts_SourceDomain {
  DEFAULT_DOMAIN = 0,
  LOG_ERR_DOMAIN,
  SYSLOG_DOMAIN,
  LINUX_KERNEL_DOMAIN,
  F_ERROR_DOMAIN,
  AX_ERROR_DOMAIN,
};

struct sp_ts_Context {
  struct sp_ts_file file;
  TSTree *tree;
  enum sp_ts_SourceDomain domain;
  uint32_t output_line;
};

struct arg_list;
struct arg_list {
  char *format;
  char *variable;
  bool complete;
  char *complex_raw;
  char *type;
  bool complex_printf;
  uint32_t pointer;

  bool function_pointer;

  bool dead;

  bool is_array;
  char *variable_array_length;

  struct arg_list *rec;

  struct arg_list *next;
};

/* ======================================== */
int
mmap_file(const char *file, struct sp_ts_file *result);

/* ======================================== */
bool
sp_parse_uint32_t(const char *in, uint32_t *out);

/* ======================================== */

#endif
