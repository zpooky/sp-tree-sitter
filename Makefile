# https://spin.atomicobject.com/2016/08/26/makefile-c-projects/
PARSE_SOURCES = main.c
STRUCT_SOURCES = struct.c lang/tree-sitter-cpp/src/parser.c lang/tree-sitter-cpp/src/scanner.c
SHARED_SOURCES = shared.c to_string.c sp_util.c sp_str.c lang/tree-sitter-c/src/parser.c
# SOURCES = $(shell find . -iname "*.c" | grep -v '.ccls-cache' | xargs)
# SOURCES = $(wildcard *.c)

# BUILD_DIR = .
# BUILD_DIR = build
# SHARED_OBJECTS = $(SOURCES:%.c=$(BUILD_DIR)/%.o)
PARSE_OBJECTS = $(PARSE_SOURCES:%=%.o)
STRUCT_OBJECTS = $(STRUCT_SOURCES:%=%.o)
SHARED_OBJECTS = $(SHARED_SOURCES:%=%.o)
ALL_OBJECTS = $(PARSE_OBJECTS) $(STRUCT_OBJECTS) $(SHARED_OBJECTS)

DEPENDS = $(ALL_OBJECTS:.o=.d)

LDFLAGS = -fno-omit-frame-pointer -fstack-protector -fsanitize=address
#-fsanitize=undefined
#-fsanitize=thread

# LDFLAGS += $(shell pkg-config --libs libsystemd glib-2.0)
LDLIBS = -Ltree-sitter -l:libtree-sitter.a $(shell pkg-config --libs jansson)#-Lbuild -l:languages.so
LDFLAGS = -Wl,-rpath,build # write rpath to executable for where to find languages.so
LDFLAGS =

PROG = parse
STRUCT = sp_struct_to_string

# default
# CC = gcc
# CC = clang
# CC = musl-gcc -static

# https://github.com/lefticus/cppbestpractices/blob/master/02-Use_the_Tools_Available.md#gcc%E2%80%94clang
CFLAGS += -std=gnu11
CFLAGS += -Itree-sitter/lib/include -Ilang/tree-sitter-cpp/src
# CFLAGS += $(shell pkg-config --cflags libsystemd glib-2.0)
CFLAGS += -Wall -Wextra -Wpointer-arith -Wconversion -Wshadow
CFLAGS += -Wnull-dereference -Wdouble-promotion
CFLAGS += -Wreturn-type -Wcast-align -Wcast-qual -Wuninitialized -Winit-self
CFLAGS += -Wformat=2 -Wformat-security -Wmissing-include-dirs
CFLAGS += -Wstrict-prototypes
CFLAGS += -ggdb -O0
CFLAGS += $(shell pkg-config --cflags jansson)

CXXFLAGS = $(CFLAGS)


ifeq ($(CC), gcc)
CFLAGS += -Wpedantic -Wduplicated-cond -Wlogical-op
endif

ifeq ($(CC), clang)
CFLAGS += -Wformat
endif

.PHONEY: all
all: $(PROG) $(STRUCT)

tree-sitter/libtree-sitter.a:
	$(MAKE) -C tree-sitter

$(PROG): $(PARSE_OBJECTS) $(SHARED_OBJECTS) tree-sitter/libtree-sitter.a
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(STRUCT): $(STRUCT_OBJECTS) $(SHARED_OBJECTS) tree-sitter/libtree-sitter.a
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

-include $(DEPENDS)
%.c.o: %.c
	$(CC) $(CFLAGS) -MMD -c $< -o $@

%.cc.o: %.cc
	$(CXX) $(CXXFLAGS) -MMD -c $< -o $@

%.cpp.o: %.cpp
	$(CXX) $(CXXFLAGS) -MMD -c $< -o $@

.PHONEY: clean
clean:
	$(RM) $(ALL_OBJECTS)
	$(RM) $(PROG) $(STRUCT)
	$(RM) $(DEPENDS)
	$(MAKE) -C tree-sitter clean

.PHONEY: install
install: all
	install -d $(DESTDIR)$(PREFIX)/bin/
	install $(STRUCT) $(DESTDIR)$(PREFIX)/bin/
