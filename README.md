# Clox Compiler and VM

WIP

## Building

```
mkdir build
cd build
cmake ..
cmake --build .
```

Run it.

```
./clox
```

### Compiling to Wasm with Emscripten (Run with Node)

Build:

```
docker run --rm -v /Users/codyrigney/Development/clox:/clox -e EMCC=true emscripten/emsdk /bin/bash -c "mkdir -p /clox/wasm-js-build && cd /clox/wasm-js-build && emcmake cmake .. && emmake make"
```

Run:
```
docker run --rm -it -v /Users/codyrigney/Development/clox:/usr/src/app -w /usr/src/app node node wasm-js-build/clox.js /app/examples/classes.lox
```

### Compile to Wasm with Clang/Wasi-SDK (Run with WasmEdge)

Build:

```
docker build -f Dockerfile.wasm.build -t clox-wasm-builder .
docker run --rm clox-wasm-builder > clox.wasm
```

Run:

```
wasmedge --dir .:. wasm-build/clox.wasm ./examples/classes.lox
```

#### Ahead of time compilation with WasmEdge

```
wasmedgec wasm-build/clox.wasm wasm-build/clox.dylib
wasmedge --dir .:. wasm-build/clox.dylib ./examples/classes.lox
```
