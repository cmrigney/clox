.PHONY: build build-obj build-shared build-static clean

build-obj:
	clang -c -o libcloxos.o main.c -fPIC

build-shared: build-obj
	clang -shared -w -undefined dynamic_lookup -o libcloxos.so libcloxos.o

build-static: build-obj
	ar rcs libcloxos.a libcloxos.o

build: build-shared build-static

clean:
	rm -f *.o *.so *.a