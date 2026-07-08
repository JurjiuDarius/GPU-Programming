# Simple Dockerfile for running the thread library on Linux
# Use this if you're on macOS (ucontext is broken on Apple Silicon)

FROM ubuntu:22.04

# Install build tools
RUN apt-get update && apt-get install -y gcc make && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source files
COPY ult.h ult.c test_threads.c Makefile ./

# Build the test program
RUN make

# Run the test
CMD ["./test_threads"]
