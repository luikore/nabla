target = libadt.a
debug_target = libadt-debug.a
default: $(target)
debug: $(debug_target)

c_bases = gens val box array dict sym-table map string cons token struct
bases = $(c_bases)
bases += asm/val-c-call asm/val-c-call2 ../vendor/tinycthread/source/tinycthread
objects = $(addsuffix .o, $(bases))
debug_objects = $(addsuffix -debug.o, $(bases))

test_srcs = test.c asm/val-c-call.S asm/val-c-call2.S map-node-test.c map-cola-test.c
test_srcs += $(addsuffix .c, $(c_bases))
test_srcs += $(addsuffix -test.c, $(c_bases))

-include ../makefile-config
-include *.d asm/*.d

# CFLAGS += -g -UNDEBUG

%-debug.o: %.c
	$(CC) -c $(CFLAGS_DEBUG) $< -o $@

asm/%-debug.o: asm/%.S
	$(CC) -c $(CFLAGS_DEBUG) $< -o $@

asm/%.o: asm/%.S
	$(CC) -c $(CFLAGS) $^ -o $@

$(target): $(objects)
	ar rcs $@ $^

$(debug_target): $(debug_objects)
	ar rcs $@ $^

test:
	$(CC) $(CFLAGS_DEBUG) $(LDFLAGS) $(test_srcs) -o adt-test
	./adt-test

# NOTE:
# XCode 8 ships with an older `llvm-cov gcov` which can't generate html properly
# the data generated can't be recognized by lcov or gcc-gcov either
#
# NOTE:
# make coverage will result in folder up to date
#
# pip install gcovr
cov:
	mkdir -p coverage
	gcovr -r . --html --html-details --sort-uncovered -o coverage/index.html
	open coverage/index.html

clean:
	rm -f {,asm/}*.{o,d,a,gcda,gcno} adt-test
	rm -rf coverage
	rm -rf *.dSYM
