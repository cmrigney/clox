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

### Compiling to Wasm
```
docker run --rm -v /Users/codyrigney/Development/clox:/clox -e EMCC=true emscripten/emsdk /bin/bash -c "mkdir -p /clox/wasm-build && cd /clox/wasm-build && emcmake cmake .. && emmake make && ls -l"
```

```
docker run --rm -it -v /Users/codyrigney/Development/clox:/usr/src/app -w /usr/src/app node node wasm-build/clox.js /app/examples/classes.lox
```