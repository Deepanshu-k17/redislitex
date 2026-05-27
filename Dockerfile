FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN rm -rf build && cmake -S . -B build && cmake --build build

EXPOSE 6379

CMD ["./build/redislitex"]