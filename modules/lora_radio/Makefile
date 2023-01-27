.PHONY: build

build:
	mkdir -p build
	cd build && cmake .. && make -j4
	cp build/libcloxlora_radio.a .

clean:
	rm -f *.a
	rm -rf build