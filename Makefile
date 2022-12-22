.PHONY: build run clean build-wasm run-wasm

DIR := ${CURDIR}

# If the first argument starts with "run"...
ifneq (,$(findstring run,$(firstword $(MAKECMDGOALS))))
  # use the rest as arguments for "run"
  RUN_ARGS := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
  # ...and turn them into do-nothing targets
  $(eval $(RUN_ARGS):;@:)
endif


build:
	mkdir -p build && cd build && cmake .. && make -j4

build-wasm:
	docker buildx build --platform linux/amd64 -f Dockerfile.wasm.build -t clox-wasm-builder . && docker run --rm --platform linux/amd64 clox-wasm-builder > clox.wasm

run: build
	./build/clox $(RUN_ARGS)

run-wasm: build-wasm
	docker run -it --rm --platform linux/amd64 -v $(DIR):/app -w /app wasmedge/slim:0.11.2 wasmedge --dir .:. /app/clox.wasm $(RUN_ARGS)

clean:
	rm -rf build