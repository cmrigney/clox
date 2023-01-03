.PHONY: build run clean build-wasm run-wasm build-pico wasm-opt

DIR := ${CURDIR}

# If the first argument starts with "run"...
ifneq (,$(findstring run,$(firstword $(MAKECMDGOALS))))
  # use the rest as arguments for "run"
  RUN_ARGS := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
  # ...and turn them into do-nothing targets
  $(eval $(RUN_ARGS):;@:)
endif

prepare-stdlib:
	mkdir -p autogen && xxd -i stdlib/lib.lox > autogen/stdlib_lox.h

prepare-bundle:
ifdef bundle
	mkdir -p autogen && xxd -i -n exec_bundle ${bundle} > autogen/bundle_lox.h
else
	rm -f autogen/bundle_lox.h
endif

build: prepare-stdlib prepare-bundle
ifdef bundle
	mkdir -p build && cd build && BUNDLE=true cmake -DCMAKE_BUILD_TYPE=Debug .. && make -j4
else
	mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Debug .. && make -j4
endif

build-release: prepare-stdlib prepare-bundle
ifdef bundle
	mkdir -p build && cd build && BUNDLE=true cmake -DCMAKE_BUILD_TYPE=Release .. && make -j4
else
	mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j4
endif

build-pico:
	MODULES=pico make build-release

build-wasm: prepare-stdlib prepare-bundle
ifdef bundle
	docker buildx build --platform linux/amd64 --build-arg bundle=true -f Dockerfile.wasm.build -t clox-wasm-builder . && docker run --rm --platform linux/amd64 clox-wasm-builder > clox.wasm
else
	docker buildx build --platform linux/amd64 -f Dockerfile.wasm.build -t clox-wasm-builder . && docker run --rm --platform linux/amd64 clox-wasm-builder > clox.wasm
endif

build-modules:
	cd modules/filesystem && make build
	cd modules/os && make build

run:
	./build/clox $(RUN_ARGS)

wasm-opt:
	docker run -it --rm --platform linux/amd64 -v $(DIR):/app -w /app wasmedge/slim:0.11.2 wasmedgec /app/clox.wasm /app/clox_aot.wasm && rm -f clox.wasm && mv clox_aot.wasm clox.wasm

run-wasm:
	docker run -it --rm --platform linux/amd64 -v $(DIR):/app -w /app wasmedge/slim:0.11.2 wasmedge --dir .:. /app/clox.wasm $(RUN_ARGS)

clean:
	rm -rf build
	rm -f clox.wasm