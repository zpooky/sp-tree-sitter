# https://spin.atomicobject.com/2016/08/26/makefile-c-projects/
PARSE_SOURCES = main.c
STRUCT_SOURCES = struct.c
SHARED_SOURCES = shared.c sp_util.c sp_str.c lang/tree-sitter-c/src/parser.c
# SOURCES = $(shell find . -iname "*.c" | grep -v '.ccls-cache' | xargs)
# SOURCES = $(wildcard *.c)

# BUILD_DIR = .
# BUILD_DIR = build
# SHARED_OBJECTS = $(SOURCES:%.c=$(BUILD_DIR)/%.o)
PARSE_OBJECTS = $(PARSE_SOURCES:.c=.o)
STRUCT_OBJECTS = $(STRUCT_SOURCES:.c=.o)
SHARED_OBJECTS = $(SHARED_SOURCES:.c=.o)
ALL_OBJECTS = $(PARSE_OBJECTS) $(STRUCT_OBJECTS) $(SHARED_OBJECTS)

DEPENDS = $(ALL_OBJECTS:.o=.d)

LDFLAGS = -fno-omit-frame-pointer -fstack-protector -fsanitize=address
#-fsanitize=undefined
#-fsanitize=thread

# LDFLAGS += $(shell pkg-config --libs libsystemd glib-2.0)
LDLIBS = -Ltree-sitter -l:libtree-sitter.a #-Lbuild -l:languages.so
LDFLAGS = -Wl,-rpath,build # write rpath to executable for where to find languages.so
LDFLAGS =

PROG = parse
STRUCT = sp_struct_to_string

# default
# CC = gcc
# CC = clang
CC = musl-gcc -static

# https://github.com/lefticus/cppbestpractices/blob/master/02-Use_the_Tools_Available.md#gcc%E2%80%94clang
CFLAGS += -std=gnu11
CFLAGS += -Itree-sitter/lib/include
# CFLAGS += $(shell pkg-config --cflags libsystemd glib-2.0)
CFLAGS += -Wall -Wextra -Wpointer-arith -Wconversion -Wshadow
CFLAGS += -Wnull-dereference -Wdouble-promotion
CFLAGS += -Wreturn-type -Wcast-align -Wcast-qual -Wuninitialized -Winit-self
CFLAGS += -Wformat=2 -Wformat-security -Wmissing-include-dirs
CFLAGS += -Wstrict-prototypes
CFLAGS += -ggdb -O0


ifeq ($(CC), gcc)
CFLAGS += -Wpedantic -Wduplicated-cond -Wlogical-op
endif

ifeq ($(CC), clang)
CFLAGS += -Wformat -Wformat-signedness
endif

.PHONEY: all
all: $(PROG) $(STRUCT)

$(PROG): $(PARSE_OBJECTS) $(SHARED_OBJECTS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(STRUCT): $(STRUCT_OBJECTS) $(SHARED_OBJECTS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

-include $(DEPENDS)
%.o: %.c
	$(CC) $(CFLAGS) -MMD -c $< -o $@

.PHONEY: clean
clean:
	$(RM) $(ALL_OBJECTS)
	$(RM) $(PROG) $(STRUCT)
	$(RM) $(DEPENDS)
