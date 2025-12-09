// CUDA Nonce Finder - finds nonce where SHA1(data + nonce) ends with target suffix
// SHA1 from: https://github.com/mochimodev/cuda-hashing-algos (Public Domain)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <cuda_runtime.h>

#include "sha1.cu"

#define MAX_DATA_SIZE 256
#define MAX_SUFFIX_SIZE 8

__constant__ uint8_t const_data[MAX_DATA_SIZE];
__constant__ int const_data_len;
__constant__ uint8_t const_suffix[MAX_SUFFIX_SIZE];
__constant__ int const_suffix_len;

__device__ bool hash_ends_with_suffix(const BYTE* hash) {
    for (int i = 0; i < const_suffix_len; i++) {
        if (hash[SHA1_BLOCK_SIZE - const_suffix_len + i] != const_suffix[i])
            return false;
    }
    return true;
}

__global__ void find_nonce_constant_memory(uint64_t start_nonce, int nonces_per_thread,
                                           int* found_flag, uint64_t* found_nonce) {
    uint64_t thread_id = blockIdx.x * blockDim.x + threadIdx.x;
    uint64_t total_threads = gridDim.x * blockDim.x;

    for (int i = 0; i < nonces_per_thread; i++) {
        if (*found_flag) return;

        uint64_t nonce = start_nonce + thread_id + i * total_threads;

        BYTE message[MAX_DATA_SIZE + 8];
        for (int j = 0; j < const_data_len; j++)
            message[j] = const_data[j];

        for (int j = 0; j < 8; j++)
            message[const_data_len + j] = (nonce >> (j * 8)) & 0xFF;

        BYTE hash[SHA1_BLOCK_SIZE];
        CUDA_SHA1_CTX ctx;
        cuda_sha1_init(&ctx);
        cuda_sha1_update(&ctx, message, const_data_len + 8);
        cuda_sha1_final(&ctx, hash);

        if (hash_ends_with_suffix(hash)) {
            if (atomicExch(found_flag, 1) == 0)
                *found_nonce = nonce;
            return;
        }
    }
}

__global__ void find_nonce_shared_memory(uint64_t start_nonce, int nonces_per_thread,
                                         int* found_flag, uint64_t* found_nonce) {
    __shared__ BYTE shared_data[MAX_DATA_SIZE];
    __shared__ int shared_data_len;

    if (threadIdx.x == 0)
        shared_data_len = const_data_len;

    for (int i = threadIdx.x; i < const_data_len; i += blockDim.x)
        shared_data[i] = const_data[i];

    __syncthreads();

    uint64_t thread_id = blockIdx.x * blockDim.x + threadIdx.x;
    uint64_t total_threads = gridDim.x * blockDim.x;

    for (int i = 0; i < nonces_per_thread; i++) {
        if (*found_flag) return;

        uint64_t nonce = start_nonce + thread_id + i * total_threads;

        BYTE message[MAX_DATA_SIZE + 8];
        for (int j = 0; j < shared_data_len; j++)
            message[j] = shared_data[j];

        for (int j = 0; j < 8; j++)
            message[shared_data_len + j] = (nonce >> (j * 8)) & 0xFF;

        BYTE hash[SHA1_BLOCK_SIZE];
        CUDA_SHA1_CTX ctx;
        cuda_sha1_init(&ctx);
        cuda_sha1_update(&ctx, message, shared_data_len + 8);
        cuda_sha1_final(&ctx, hash);

        if (hash_ends_with_suffix(hash)) {
            if (atomicExch(found_flag, 1) == 0)
                *found_nonce = nonce;
            return;
        }
    }
}

int main(int argc, char** argv) {
    uint8_t input_data[MAX_DATA_SIZE] = "HELLO";
    int input_data_len = 5;
    uint8_t target_suffix[MAX_SUFFIX_SIZE] = {0xFF};
    int target_suffix_len = 1;
    int num_blocks = 1024;
    int threads_per_block = 256;
    int nonces_per_thread = 100;
    bool use_shared_memory = false;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--data") && i+1 < argc) {
            char* str = argv[++i];
            input_data_len = strlen(str);
            for (int j = 0; j < input_data_len; j++)
                input_data[j] = str[j];
        }
        else if (!strcmp(argv[i], "--suffix") && i+1 < argc) {
            char* str = argv[++i];
            target_suffix_len = strlen(str);
            for (int j = 0; j < target_suffix_len; j++)
                target_suffix[j] = str[j];
        }
        else if (!strcmp(argv[i], "--blocks") && i+1 < argc)
            num_blocks = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--threads") && i+1 < argc)
            threads_per_block = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--per-thread") && i+1 < argc)
            nonces_per_thread = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--shared"))
            use_shared_memory = true;
    }

    cudaDeviceProp gpu_properties;
    cudaGetDeviceProperties(&gpu_properties, 0);
    printf("GPU: %s\n", gpu_properties.name);
    printf("Suffix length: %d bytes\n", target_suffix_len);
    printf("Using %s memory\n\n", use_shared_memory ? "shared" : "constant");

    cudaMemcpyToSymbol(const_data, input_data, input_data_len);
    cudaMemcpyToSymbol(const_data_len, &input_data_len, sizeof(int));
    cudaMemcpyToSymbol(const_suffix, target_suffix, target_suffix_len);
    cudaMemcpyToSymbol(const_suffix_len, &target_suffix_len, sizeof(int));

    int* gpu_found_flag;
    uint64_t* gpu_found_nonce;
    cudaMalloc(&gpu_found_flag, sizeof(int));
    cudaMalloc(&gpu_found_nonce, sizeof(uint64_t));
    cudaMemset(gpu_found_flag, 0, sizeof(int));

    uint64_t current_nonce = 0;
    uint64_t nonces_per_kernel = (uint64_t)num_blocks * threads_per_block * nonces_per_thread;
    uint64_t total_hashes = 0;
    int found = 0;

    cudaEvent_t timer_start, timer_end;
    cudaEventCreate(&timer_start);
    cudaEventCreate(&timer_end);
    cudaEventRecord(timer_start);

    printf("Searching...\n");
    while (!found) {
        if (current_nonce > UINT64_MAX - nonces_per_kernel) {
            printf("Error: Exhausted all possible nonces.\n");
            break;
        }

        if (use_shared_memory)
            find_nonce_shared_memory<<<num_blocks, threads_per_block>>>(
                current_nonce, nonces_per_thread, gpu_found_flag, gpu_found_nonce);
        else
            find_nonce_constant_memory<<<num_blocks, threads_per_block>>>(
                current_nonce, nonces_per_thread, gpu_found_flag, gpu_found_nonce);

        cudaDeviceSynchronize();
        cudaMemcpy(&found, gpu_found_flag, sizeof(int), cudaMemcpyDeviceToHost);

        total_hashes += nonces_per_kernel;
        current_nonce += nonces_per_kernel;

        if (total_hashes % 100000000 < nonces_per_kernel)
            printf("  %lu M hashes...\n", total_hashes / 1000000);
    }

    cudaEventRecord(timer_end);
    cudaEventSynchronize(timer_end);
    float elapsed_ms;
    cudaEventElapsedTime(&elapsed_ms, timer_start, timer_end);

    if (found) {
        uint64_t result_nonce;
        cudaMemcpy(&result_nonce, gpu_found_nonce, sizeof(uint64_t), cudaMemcpyDeviceToHost);

        printf("\nFound nonce: %lu\n", result_nonce);
        printf("Total hashes: %lu\n", total_hashes);
        printf("Time: %.2f sec\n", elapsed_ms / 1000);
        printf("Rate: %.2f MH/s\n", (total_hashes / 1e6) / (elapsed_ms / 1000));
    }

    cudaFree(gpu_found_flag);
    cudaFree(gpu_found_nonce);

    return 0;
}
