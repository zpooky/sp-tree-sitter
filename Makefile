# https://spin.atomicobject.com/2016/08/26/makefile-c-projects/
SOURCES = main.c
# SOURCES = $(shell find . -iname "*.c" | grep -v '.ccls-cache' | xargs)
# SOURCES = $(wildcard *.c)

# BUILD_DIR = .
# BUILD_DIR = build
# OBJECTS = $(SOURCES:%.c=$(BUILD_DIR)/%.o)
OBJECTS = $(SOURCES:.c=.o)

DEPENDS = $(OBJECTS:.o=.d)

LDFLAGS = -fno-omit-frame-pointer -fstack-protector -fsanitize=address
#-fsanitize=undefined
#-fsanitize=thread

# LDFLAGS += $(shell pkg-config --libs libsystemd glib-2.0)
LDLIBS = -Ltree-sitter -l:libtree-sitter.a -Lbuild -l:languages.so
LDFLAGS = -Wl,-rpath,build # write rpath to executable for where to find languages.so

PROG = parse

# default
# CC = gcc
# CC = clang
# CC = musl-gcc -static

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
all: $(PROG)

$(PROG): $(OBJECTS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

-include $(DEPENDS)
%.o: %.c
	$(CC) $(CFLAGS) -MMD -c $< -o $@

.PHONEY: clean
clean:
	$(RM) $(OBJECTS)
	$(RM) $(PROG)
	$(RM) $(DEPENDS)
