#include "shared.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
  int dummy;
} type_t;

int
mmap_file(const char *file, struct sp_ts_file *result)
{
  struct stat st = {0};
  memset(&st, 0, sizeof(st));

#if 0
  result->content = ""
                    "typedef enum enum_type {ENUM_X, ENUM_Y } enum_t;\n"
                    "typedef union union_type { int i1; float f2;} union_t;\n"
                    "struct sut {\n"
                    /* "char buf[256];\n" */
                    /* "  const char* string0;\n" */
                    /* "  char* string1;\n" */
                    "  bool bool0;\n"
                    /* "  const long long *ll0;\n" */
                    /* "  unsigned long long ull1;\n" */
                    /* "  long double ld0;\n" */
                    /* "  int int0;\n" */
                    /* "  gint int1;\n" */
                    /* "  const int int2;\n" */
                    /* "  volatile int int3;\n" */
                    /* "  char char0;\n" */
                    "  gchar char1;\n"
                    /* "  const int*const** ppint0;\n" */
                    /* "  float float0;\n" */
/*                     "  double double0;\n" */
                    /* "  struct type type0;\n" */
                    /* "  type_t type1;\n" */
                    /* "  type_t *type2;\n" */
                    "  struct {\n"
                    "    int int0;\n"
                    "  } anon_struct0;\n"
/*                     "  enum { ENUM_1, ENUM_2 } anon_enum0;\n" */
                    /* "  enum enum_type enum0;\n" */
                    /* "  enum_t enum1;\n" */
/*                     "  union { int i1; float f2; } anon_union0;\n" */
/*                     "  union union_type union0;\n" */
/*                     "  union_t union1;\n" */
                    /* "  size_t sz0;\n" */
/*                     "  ssize_t ssz0;\n" */
                    "} instance;\n"
                    "\0";
  result->length = strlen(result->content);
  result->fd     = -1;
  return 0;
#endif
  if ((result->fd = open(file, O_RDONLY)) < 0) {
    fprintf(stderr, "Unable to open '%s': %m\n", file);
    return -1;
  }

  if (fstat(result->fd, &st) < 0) {
    fprintf(stderr, "fstat failed on '%s': %m\n", file);
    goto Lerr;
  }

  if (!S_ISREG(st.st_mode)) {
    fprintf(stderr, "File: '%s' is not a regular file\n", file);
    goto Lerr;
  }

  if (st.st_size == 0) {
    fprintf(stderr, "File: '%s' is empty\n", file);
    goto Lerr;
  }

  result->length  = (size_t)st.st_size;
  result->content = mmap(NULL, result->length, PROT_READ,
                         MAP_PRIVATE | MAP_FILE, result->fd, 0);
  if (result->content == MAP_FAILED) {
    fprintf(stderr, "mmap failed on '%s': %m\n", file);
    goto Lerr;
  }

  return 0;
Lerr:
  if (result->fd >= 0) {
    close(result->fd);
  }
  result->fd = -1;
  return -1;
}

bool
sp_parse_uint32_t(const char *in, uint32_t *out)
{
  char *end          = NULL;
  const char *in_end = in + strlen(in);
  long val;

  val = strtol(in, &end, 10);
  if (end != in_end) {
    return false;
  }

  if (errno == ERANGE) {
    return false;
  }

  if (val < 0) {
    return false;
  }
  if (val >= UINT32_MAX) {
    return false;
  }

  *out = (uint32_t)val;

  return true;
}
