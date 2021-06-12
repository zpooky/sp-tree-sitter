#ifndef SP_TS_SHARED_H
#define SP_TS_SHARED_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

struct sp_ts_file {
  char *content;
  size_t length;
  int fd;
};

int
mmap_file(const char *file, struct sp_ts_file *result);

bool
sp_parse_uint32_t(const char *in, uint32_t *out);

#endif
