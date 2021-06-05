#include "shared.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

char *
mmap_file(const char *file, size_t *bytes)
{
  int fd;
  char *addr;
  struct stat st = {0};

  if ((fd = open(file, O_RDONLY) < 0)) {
    fprintf(stderr, "Unable to open '%s': %m\n", file);
    return NULL;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(stderr, "fstat failed on '%s': %m\n", file);
    return NULL;
  }

  *bytes = (size_t)st.st_size;
  addr   = mmap(NULL, *bytes, PROT_READ, MAP_PRIVATE, fd, 0);
  if (addr == MAP_FAILED) {
    fprintf(stderr, "mmap failed on '%s': %m\n", file);
    return NULL;
  }

  return (char *)addr;
}
