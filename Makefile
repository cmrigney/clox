.PHONY: build run clean

# If the first argument is "run"...
ifeq (run,$(firstword $(MAKECMDGOALS)))
  # use the rest as arguments for "run"
  RUN_ARGS := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
  # ...and turn them into do-nothing targets
  $(eval $(RUN_ARGS):;@:)
endif


build:
	mkdir -p build && cd build && cmake .. && make -j4

run: build
	./build/clox $(RUN_ARGS)

clean:
	rm -rf build