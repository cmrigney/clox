.PHONY: build build-obj build-shared build-static clean

build-obj:
	clang -c -o libcloxfilesystem.o main.c -fPIC

build-shared: build-obj
	clang -shared -w -undefined dynamic_lookup -o libcloxfilesystem.so libcloxfilesystem.o

build-static: build-obj
	ar rcs libcloxfilesystem.a libcloxfilesystem.o

build: build-shared build-static

clean:
	rm -f *.o *.so *.a