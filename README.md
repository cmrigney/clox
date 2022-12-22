# Clox Compiler and VM

## Building

```
make build
```

Run it.

```
make run examples/classes.lox
```

# Running as WebAssembly

## Compile to Wasm with Clang/Wasi-SDK (Run with WasmEdge)

### Build

```
make build-wasm
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
