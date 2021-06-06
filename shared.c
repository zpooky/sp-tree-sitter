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

  result->content = ""
                    "typedef enum enum_type {ENUM_X, ENUM_Y } enum_t;\n"
                    "struct type {\n"
                    "  int int0;\n"
                    "  char char0;\n"
                    "  const char* string0;\n"
                    "  float float0;\n"
                    "  double double0;\n"
                    "};\n"
                    "typedef struct {\n"
                    "  int int0;\n"
                    "  char char0;\n"
                    "  const char* string0;\n"
                    "  float float0;\n"
                    "  double double0;\n"
                    "} type_t;\n"
                    "struct sut {\n"
                    "  int int0;\n"
                    "  char char0;\n"
                    "  const char* string0;\n"
                    "  float float0;\n"
                    "  double double0;\n"
                    "  struct type type0;\n"
                    "  type_t type1;\n"
                    "  struct {\n"
                    "    int int0;\n"
                    "  } anon_struct0;\n"
                    "  enum { ENUM_1, ENUM_2 } anon_enum0;\n"
                    "  enum enum_type enum0;\n"
                    "  enum_t enum1;\n"
                    "} instance;\n"
                    "\0";
  result->length = strlen(result->content);
  result->fd     = -1;
  return 0;
  if ((result->fd = open(file, O_NONBLOCK | O_RDONLY | O_CLOEXEC) < 0)) {
    fprintf(stderr, "Unable to open '%s': %m\n", file);
    return -1;
  }

  if (fstat(result->fd, &st) < 0) {
    fprintf(stderr, "fstat failed on '%s': %m\n", file);
    return -1;
  }

  if (S_ISBLK(st.st_mode)) {
    printf("S_ISBLK\n");
  }
  if (S_ISCHR(st.st_mode)) {
    printf("S_ISCHR\n");
  }
  if (S_ISDIR(st.st_mode)) {
    printf("S_ISDIR\n");
  }
  if (S_ISFIFO(st.st_mode)) {
    printf("S_ISFIFO\n");
  }
  if (S_ISLNK(st.st_mode)) {
    printf("S_ISLNK\n");
  }
  if (S_ISREG(st.st_mode)) {
    printf("S_ISREG\n");
  }
  if (S_ISSOCK(st.st_mode)) {
    printf("S_ISSOCK\n");
  }
  if (!S_ISREG(st.st_mode)) {
    fprintf(stderr, "File: '%s' is not a regular file\n", file);
    return -1;
  }

  if (st.st_size == 0) {
    fprintf(stderr, "File: '%s' is empty\n", file);
    return -1;
  }

  result->length = (size_t)st.st_size;
  printf("bytes:%jd\n", st.st_size);
  result->content =
    mmap(NULL, result->length, PROT_READ, MAP_PRIVATE, result->fd, 0);
  if (result->content == MAP_FAILED) {
    fprintf(stderr, "mmap failed on '%s': %m\n", file);
    return -1;
  }

  return 0;
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
