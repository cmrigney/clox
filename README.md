# Clox Compiler and VM

## Setup
Clone the repo and checkout the git submodules.
```
git clone git@github.com:cmrigney/clox.git
cd clox
git submodule update --init --recursive
```

## Building

```
make build
```

Run it.

```
make run examples/classes.lox
```

## Bundling a Lox Script into the Build
```
make bundle=examples/classes.lox build
```

Or for Wasm
```
make bundle=examples/classes.lox build-wasm
```

# Running as WebAssembly

## Compile to Wasm with Clang/Wasi-SDK (Run with WasmEdge)

### Build

```
make build-wasm
make wasm-opt # optionally aot compile the module
```

### Running the Wasm module

```
make run-wasm examples/classes.lox
```

#### Ahead of time compilation with WasmEdge

WasmEdge should be installed locally. This assumes a Mac device. Linux is `.so`, Windows is `.dll`.

```
wasmedgec wasm-build/clox.wasm wasm-build/clox.dylib
wasmedge --dir .:. wasm-build/clox.dylib ./examples/classes.lox
```


## Compiling to Wasm with Emscripten (Run with Node)

### Build

```
docker run --rm -v /Users/codyrigney/Development/clox:/clox -e EMCC=true emscripten/emsdk /bin/bash -c "mkdir -p /clox/wasm-js-build && cd /clox/wasm-js-build && emcmake cmake .. && emmake make"
```

### Run
```
docker run --rm -it -v /Users/codyrigney/Development/clox:/usr/src/app -w /usr/src/app node node wasm-js-build/clox.js /app/examples/classes.lox
```

## Compile for RPI Pico

Build:
```
make build-pico
```

Copy the uf2 file:
```
rsync build/clox.uf2 /Volumes/RPI-RP2/
```

Connect to repl via minicom:
```
minicom -b 115200 -o -D /dev/tty.usbmodem21201
```