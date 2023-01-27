.PHONY: build run clean build-wasm run-wasm build-pico wasm-opt

DIR := ${CURDIR}

# If the first argument starts with "run"...
ifneq (,$(findstring run,$(firstword $(MAKECMDGOALS))))
  # use the rest as arguments for "run"
  RUN_ARGS := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
  # ...and turn them into do-nothing targets
  $(eval $(RUN_ARGS):;@:)
endif

build:
ifdef bundle
	mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Debug -DLOX_BUNDLE=${bundle} .. && make -j4
else
	mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Debug .. && make -j4
endif

build-release:
ifdef bundle
	mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Release -DLOX_BUNDLE=${bundle} .. && make -j4
else
	mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j4
endif

build-pico:
	PICO_SDK_PATH=./vendor/pico-sdk MODULES=pico,lora make build

build-picow:
	PICO_BOARD=pico_w USE_PICO_W=true make build-pico

build-wasm:
ifdef bundle
	docker buildx build --platform linux/amd64 --build-arg BUNDLE=${bundle} --build-arg HAS_BUNDLE=true -f Dockerfile.wasm.build -t clox-wasm-builder . && docker run --rm --platform linux/amd64 clox-wasm-builder > clox.wasm
else
	docker buildx build --platform linux/amd64 --build-arg HAS_BUNDLE=false -f Dockerfile.wasm.build -t clox-wasm-builder . && docker run --rm --platform linux/amd64 clox-wasm-builder > clox.wasm
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