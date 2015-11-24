target = libadt.a
default: $(target)

files = val box map array dict sym-table string
objects = $(addsuffix .o, $(files))
objects += asm/val-c-call.o

test_srcs = test.c
test_srcs += asm/val-c-call.S
test_srcs += $(addsuffix .c, $(files))
test_srcs += $(addsuffix -test.c, $(files))

-include ../makefile-config
-include *.d

# CFLAGS += -g -UNDEBUG

asm/val-c-call.o: asm/val-c-call.S
	cc -c $(CFLAGS) asm/val-c-call.S -o asm/val-c-call.o

$(target): $(objects) asm/val-c-call.o
	ar rcs libadt.a $(objects)
	rm -f $(objects)

debug: $(addsuffix .c, $(files))
	cc -c $(CFLAGS_DEBUG) $(addsuffix .c, $(files))
	ar rcs libadt-debug.a $(objects)
	rm -f $(objects)

test:
	cc $(CFLAGS_DEBUG) $(LDFLAGS) $(test_srcs) -o adt-test
	./adt-test

clean:
	rm -f *.o *.d *.a adt-test
	rm -rf *.dSYM
