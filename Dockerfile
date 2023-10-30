FROM gcc:latest

WORKDIR /app

COPY . .

RUN apt-get update && \
    apt-get install -y cmake && \
    mkdir build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make

CMD ["./build/clox"]