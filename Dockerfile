# Start with Ubuntu
FROM ubuntu:16.04

# Get necessary libraries
RUN apt-get update && apt-get install -y \
    cmake \
    make \
    clang \
    liblmdb-dev \
    libck-dev \
    libhts-dev \
    libz-dev

# Copy bamdb source into container working directory
COPY src/ /bamdb/src/
COPY include/ /bamdb/include/
COPY CMakeLists.txt /bamdb
WORKDIR /bamdb

# Compile bamdb
RUN make

# Run bamdb
ENTRYPOINT ["/bamdb/build/bamdb"]
