FROM debian:bookworm AS build

RUN apt-get update && apt-get install -y \
  coreutils \
  build-essential \
  clang \
  llvm \
  cmake \
  git \
  wget


WORKDIR /build

# download latest version of xxd for -n option
RUN git clone https://github.com/vim/vim.git && \
  cd vim && \
  git checkout 9a9432d3a223f7fbd902a0346030422ae0a97f0e
WORKDIR /build/vim/src/xxd
RUN make

RUN cp /build/vim/src/xxd/xxd /usr/bin/

COPY . /clox
WORKDIR /clox

RUN mkdir -p build && \
  cd build && \
  cmake -DCMAKE_BUILD_TYPE=Release .. && \
  make -j`nproc`

# docker build -t clox-mcp --target mcp .
FROM node:22-bookworm-slim AS mcp

COPY ./mcp /mcp
WORKDIR /mcp

RUN npm install
RUN npm run build

COPY --from=build /clox/build/clox /clox

ENV CLOX_EXECUTABLE=/clox

ENTRYPOINT ["node", "/mcp/dist/index.js"]

FROM debian:bookworm-slim AS final

COPY --from=build /clox/build/clox /clox

ENTRYPOINT ["/clox"]

