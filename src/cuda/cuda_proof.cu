// Utilization Observatory : real RTX 5090 / CUDA hardware proof primitives.
// Real allocation, real H2D/D2H, real kernels, real events/synchronization.
// Copyright 2026 Summon Software Labs. Apache-2.0.
#include <cuda_runtime.h>
#include <chrono>
#include <cstdio>

__global__ void uo_scale_kernel(float* out, const float* in, long n) {
    long i = static_cast<long>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (i < n) out[i] = in[i] * 2.0f;
}

static const char* last_err = nullptr;

extern "C" int uo_cuda_device_count() {
    int n = 0;
    if (cudaGetDeviceCount(&n) != cudaSuccess) return 0;
    return n;
}

extern "C" const char* uo_cuda_device_name(int index) {
    static char name[256];
    cudaDeviceProp prop;
    if (cudaGetDeviceProperties(&prop, index) != cudaSuccess) {
        std::snprintf(name, sizeof(name), "unknown");
        return name;
    }
    std::snprintf(name, sizeof(name), "%s (sm_%d, %d SM, %.1f MiB)",
                  prop.name, prop.major * 10 + prop.minor, prop.multiProcessorCount,
                  static_cast<double>(prop.totalGlobalMem) / (1024.0 * 1024.0));
    return name;
}

extern "C" bool uo_cuda_alloc(void** ptr, size_t bytes) {
    cudaError_t e = cudaMalloc(ptr, bytes);
    if (e != cudaSuccess) { last_err = cudaGetErrorString(e); return false; }
    return true;
}

extern "C" void uo_cuda_free(void* ptr) { (void)cudaFree(ptr); }

extern "C" bool uo_cuda_h2d(void* dst, const void* src, size_t bytes) {
    cudaError_t e = cudaMemcpy(dst, src, bytes, cudaMemcpyHostToDevice);
    if (e != cudaSuccess) { last_err = cudaGetErrorString(e); return false; }
    return true;
}

extern "C" bool uo_cuda_d2h(void* dst, const void* src, size_t bytes) {
    cudaError_t e = cudaMemcpy(dst, src, bytes, cudaMemcpyDeviceToHost);
    if (e != cudaSuccess) { last_err = cudaGetErrorString(e); return false; }
    return true;
}

extern "C" bool uo_cuda_sync() {
    cudaError_t e = cudaDeviceSynchronize();
    if (e != cudaSuccess) { last_err = cudaGetErrorString(e); return false; }
    return true;
}

extern "C" const char* uo_cuda_last_error() { return last_err ? last_err : "none"; }

extern "C" double uo_cuda_run_busy_ms(int ms) {
    const long n = 4L * 1024 * 1024;
    float* d_out = nullptr;
    float* d_in = nullptr;
    if (cudaMalloc(&d_out, n * sizeof(float)) != cudaSuccess) return -1.0;
    if (cudaMalloc(&d_in, n * sizeof(float)) != cudaSuccess) { cudaFree(d_out); return -1.0; }
    cudaMemset(d_in, 0, n * sizeof(float));

    cudaEvent_t e0, e1;
    cudaEventCreate(&e0);
    cudaEventCreate(&e1);
    cudaEventRecord(e0);

    auto start = std::chrono::steady_clock::now();
    double elapsed_ms = 0.0;
    while (elapsed_ms < static_cast<double>(ms)) {
        uo_scale_kernel<<<(n + 255) / 256, 256>>>(d_out, d_in, n);
        elapsed_ms = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start).count() / 1000.0;
    }
    cudaEventRecord(e1);
    cudaEventSynchronize(e1);

    float elapsed = 0.0f;
    cudaEventElapsedTime(&elapsed, e0, e1);
    cudaFree(d_out);
    cudaFree(d_in);
    cudaEventDestroy(e0);
    cudaEventDestroy(e1);
    return static_cast<double>(elapsed);
}

extern "C" double uo_cuda_measure_h2d(size_t bytes, void* out_ptr) {
    void* d = nullptr;
    if (cudaMalloc(&d, bytes) != cudaSuccess) return -1.0;
    float* h = static_cast<float*>(out_ptr);
    for (size_t i = 0; i < bytes / sizeof(float); ++i) h[i] = static_cast<float>(i % 1024);
    cudaEvent_t e0, e1;
    cudaEventCreate(&e0);
    cudaEventCreate(&e1);
    cudaEventRecord(e0);
    cudaMemcpy(d, h, bytes, cudaMemcpyHostToDevice);
    cudaEventRecord(e1);
    cudaEventSynchronize(e1);
    float elapsed = 0.0f;
    cudaEventElapsedTime(&elapsed, e0, e1);
    cudaFree(d);
    cudaEventDestroy(e0);
    cudaEventDestroy(e1);
    return static_cast<double>(elapsed);
}

extern "C" double uo_cuda_measure_d2h(size_t bytes, void* out_ptr) {
    void* d = nullptr;
    if (cudaMalloc(&d, bytes) != cudaSuccess) return -1.0;
    float* h = static_cast<float*>(out_ptr);
    cudaMemcpy(d, h, bytes, cudaMemcpyHostToDevice);
    cudaEvent_t e0, e1;
    cudaEventCreate(&e0);
    cudaEventCreate(&e1);
    cudaEventRecord(e0);
    cudaMemcpy(h, d, bytes, cudaMemcpyDeviceToHost);
    cudaEventRecord(e1);
    cudaEventSynchronize(e1);
    float elapsed = 0.0f;
    cudaEventElapsedTime(&elapsed, e0, e1);
    cudaFree(d);
    cudaEventDestroy(e0);
    cudaEventDestroy(e1);
    return static_cast<double>(elapsed);
}

extern "C" bool uo_cuda_parity(const float* buf, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        float expected = static_cast<float>(i % 1024) * 2.0f;
        if (buf[i] != expected) return false;
    }
    return true;
}

// Real useful-work path: allocate, H2D, run kernels, D2H, return elapsed (ms).
// The caller verifies CPU parity of the D2H buffer.
extern "C" double uo_cuda_useful_work(int ms, float* host_out, size_t n) {
    float* d_out = nullptr;
    float* d_in = nullptr;
    if (cudaMalloc(&d_out, n * sizeof(float)) != cudaSuccess) return -1.0;
    if (cudaMalloc(&d_in, n * sizeof(float)) != cudaSuccess) { cudaFree(d_out); return -1.0; }
    for (size_t i = 0; i < n; ++i) host_out[i] = static_cast<float>(i % 1024);
    if (cudaMemcpy(d_in, host_out, n * sizeof(float), cudaMemcpyHostToDevice) != cudaSuccess) {
        cudaFree(d_out); cudaFree(d_in); return -1.0;
    }
    cudaEvent_t e0, e1;
    cudaEventCreate(&e0);
    cudaEventCreate(&e1);
    cudaEventRecord(e0);
    auto start = std::chrono::steady_clock::now();
    double elapsed_ms = 0.0;
    while (elapsed_ms < static_cast<double>(ms)) {
        uo_scale_kernel<<<(n + 255) / 256, 256>>>(d_out, d_in, n);
        elapsed_ms = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start).count() / 1000.0;
    }
    if (cudaMemcpy(host_out, d_out, n * sizeof(float), cudaMemcpyDeviceToHost) != cudaSuccess) {
        cudaFree(d_out); cudaFree(d_in);
        cudaEventDestroy(e0); cudaEventDestroy(e1);
        return -1.0;
    }
    cudaEventRecord(e1);
    cudaEventSynchronize(e1);
    float elapsed = 0.0f;
    cudaEventElapsedTime(&elapsed, e0, e1);
    cudaFree(d_out);
    cudaFree(d_in);
    cudaEventDestroy(e0);
    cudaEventDestroy(e1);
    return static_cast<double>(elapsed);
}

