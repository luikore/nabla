PREFIX=/opt/local

libccut.a: ccut.c include/ccut.h
	cc -std=c11 -Iinclude -c ccut.c
	ar rcs lib/libccut.a ccut.o

test: test.c libccut.a
	cc -std=c11 -Iinclude -Llib -g -rdynamic test.c -L. -lccut
	./a.out
	c++ -Iinclude -Llib -g -rdynamic test.c -L. -lccut
	./a.out

clean:
	rm -rf *.o *.out lib/*.a *.dSYM

install: libccut.a
	sudo mkdir -p $(PREFIX)/include
	sudo cp -pR include/*.h $(PREFIX)/include
	sudo mkdir -p $(PREFIX)/lib
	sudo cp -pR lib/*.a $(PREFIX)/lib
