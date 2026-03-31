FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    cmake \
    g++ \
    git \
    libjsoncpp-dev \
    uuid-dev \
    zlib1g-dev \
    openssl \
    libssl-dev \
    libpq-dev \
    postgresql-client \
    redis-tools \
    libhiredis-dev

# Install Drogon
RUN git clone https://github.com/drogonframework/drogon.git \
 && cd drogon \
 && git submodule update --init \
 && mkdir build \
 && cd build \
 && cmake .. \
 && make -j$(nproc) \
 && make install

# Copy project
WORKDIR /app
COPY shortener/ .

# Build project
RUN mkdir build && cd build && cmake .. && make

# Run server
CMD ["./build/shortener"]
