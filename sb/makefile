target = libsb.a
default: $(target)

files = bootstrap sb compile* vm-lex vm-lex-compile vm-regexp vm-regexp-compile vm-peg vm-peg-compile
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

sb-test: $(test_srcs)
	cc $(CFLAGS_DEBUG) $(LDFLAGS_DEBUG) $(test_srcs) -o sb-test

test: sb-test
	./sb-test

clean:
	rm -f *.o *.d *.a sb-test
	rm -rf *.dSYM
