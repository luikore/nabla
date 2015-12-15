target = libsb.a
default: $(target)

files = bootstrap sb compile-* vm-lex vm-peg vm-regexp vm-callback
objects = $(addsuffix .o, $(files))
test_srcs = bootstrap-test.c vm-*-test.c test.c
test_srcs += $(addsuffix .c, $(files))

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

test:
	cc $(CFLAGS_DEBUG) $(LDFLAGS_DEBUG) $(test_srcs) -o sb-test
	./sb-test

clean:
	rm -f *.o *.d *.a sb-test
	rm -rf *.dSYM
