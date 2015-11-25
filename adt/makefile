target = libadt.a
debug_target = libadt-debug.a
default: $(target)
debug: $(debug_target)

c_bases = val box map array dict sym-table string
bases = $(c_bases)
bases += asm/val-c-call
objects = $(addsuffix .o, $(bases))
debug_objects = $(addsuffix -debug.o, $(bases))

test_srcs = test.c asm/*.S
test_srcs += $(addsuffix .c, $(c_bases))
test_srcs += $(addsuffix -test.c, $(c_bases))

-include ../makefile-config
-include *.d asm/*.d

# CFLAGS += -g -UNDEBUG

%-debug.o: %.c
	$(CC) -c $(CFLAGS_DEBUG) $+ -o $@

asm/%-debug.o: asm/%.S
	$(CC) -c $(CFLAGS_DEBUG) $+ -o $@

asm/%.o: asm/%.S
	$(CC) -c $(CFLAGS) $+ -o $@

$(target): $(objects)
	ar rcs $@ $+

$(debug_target): $(debug_objects)
	ar rcs $@ $+

test:
	$(CC) $(CFLAGS_DEBUG) $(LDFLAGS) $(test_srcs) -o adt-test
	./adt-test

clean:
	rm -f {,asm/}*.{o,d,a} adt-test
	rm -rf *.dSYM
