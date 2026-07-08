# GPU & Parallel Programming

Two self-contained systems written close to the metal for my MSc parallel/GPU programming coursework — one on the GPU with CUDA, one in the guts of a C runtime.

![C](https://img.shields.io/badge/C-00599C?logo=c&logoColor=white)
![CUDA](https://img.shields.io/badge/CUDA-76B900?logo=nvidia&logoColor=white)
![Docker](https://img.shields.io/badge/Docker-2496ED?logo=docker&logoColor=white)

## 1 · CUDA Nonce Finder — [`cuda_nonce_finder/`](cuda_nonce_finder/)

A GPU brute-force search for a nonce whose `SHA-1(data + nonce)` ends in a target suffix — the proof-of-work primitive behind blockchains, implemented as a CUDA kernel.

- SHA-1 implemented from scratch in CUDA (`sha1.cu` / `sha1.cuh`).
- Massively parallel nonce search across GPU threads (`nonce_finder.cu`).
- A runnable, Colab-ready walkthrough with benchmarks: `nonce_finder_colab.ipynb`.

## 2 · User-Level Thread Library — [`parallel_library/`](parallel_library/)

A **user-level threading library in C** — threads, scheduling, and synchronization built entirely in user space, without `pthreads`.

- `ult.c` / `ult.h`: thread create/join, a timer-driven preemptive scheduler, mutexes (`ult_mutex_t`), and reader-writer locks (`ult_rwlock_t`).
- `test_threads.c`: a harness exercising threads, mutual exclusion, and rw-locks.
- Reproducible build via `Makefile` and `Dockerfile` (`./run.sh`).

## Build & run

```bash
# CUDA nonce finder — needs an NVIDIA GPU + nvcc (or just open the Colab notebook)
cd cuda_nonce_finder && nvcc nonce_finder.cu sha1.cu -o nonce_finder && ./nonce_finder

# User-level threads
cd parallel_library && make && ./run.sh      # or build/run via the Dockerfile
```

## Tech stack

C · CUDA · SHA-1 · POSIX signals & timers · Make · Docker

— Darius Jurjiu
