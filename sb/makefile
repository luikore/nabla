target = libsb.a
default: $(target)

# NOTE makefile doesn't expand '*', it may work but may generate file names containing '*'

srcs = bootstrap.c sb.c $(shell ls -1 compile*.c) $(shell ls -1 vm-*.c | grep -v test.c)
src_bases = $(subst .c,,$(srcs))
objects = $(addsuffix .o, $(src_bases))
test_srcs = bootstrap-test.c $(shell ls -1 vm-*-test.c) test.c
test_srcs += $(addsuffix .c, $(src_bases))

-include ../makefile-config
-include *.d

# CFLAGS += -g -UNDEBUG
CFLAGS += -I..
CFLAGS_DEBUG += -I..
LDFLAGS += -L../adt -ladt

# XXX to overwrite -ladt in two directions...
LDFLAGS_DEBUG = -ladt-debug $(LDFLAGS) -ladt-debug

$(target): $(objects)
	ar rcs libsb.a $(objects)

sb-test: $(test_srcs)
	cc $(CFLAGS_DEBUG) $(LDFLAGS_DEBUG) $(test_srcs) -o sb-test

test: sb-test
	./sb-test

cov:
	mkdir -p coverage
	gcovr -r . --html --html-details --sort-uncovered -o coverage/index.html
	open coverage/index.html

infer:
	infer -- $(CC) $(CFLAGS_DEBUG) $(LDFLAGS) -c $(test_srcs)

clean:
	rm -f *.{o,d,a,gcda,gcno} sb-test
	rm -rf *.dSYM
