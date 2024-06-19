/*! @copyright (c) 2022-present Skolkovo Institute of Science and Technology
 *                              (Skoltech), Russia. All rights reserved.
 *                 2023-present Artificial Intelligence Research Institute
 *                              (AIRI), Russia. All rights reserved.
 *
 * NNTile is software framework for fast training of big neural networks on
 * distributed-memory heterogeneous systems based on StarPU runtime system.
 *
 * @file tests/starpu/fill.cc
 * Fill operation on a StarPU buffer
 *
 * @version 1.0.0
 * */

#include "nntile/starpu/fill.hh"
#include "nntile/kernel/fill.hh"
#include "../testing.hh"
#ifdef NNTILE_USE_CUDA
#   include <cuda_runtime.h>
#endif // NNTILE_USE_CUDA
#include <vector>
#include <stdexcept>
#include <iostream>

using namespace nntile;
using namespace nntile::starpu;

template<typename T>
void validate_cpu(Index nelems)
{
    // Init all the data
    T val = -0.5;
    std::vector<T> data(nelems);
    for(Index i = 0; i < nelems; ++i)
    {
        data[i] = T(i+1);
    }
    // Create copies of destination
    std::vector<T> data2(data);
    // Launch low-level kernel
    std::cout << "Run kernel::fill::cpu<T>\n";
    kernel::fill::cpu<T>(nelems, val, &data[0]);
    // Check by actually submitting a task
    VariableHandle data2_handle(&data2[0], sizeof(T)*nelems, STARPU_RW);
    fill::restrict_where(STARPU_CPU);
    std::cout << "Run starpu::fill::submit<T> restricted to CPU\n";
    fill::submit<T>(nelems, val, data2_handle);
    starpu_task_wait_for_all();
    data2_handle.unregister();
    // Check result
    for(Index i = 0; i < nelems; ++i)
    {
        TEST_ASSERT(data[i] == data2[i]);
    }
    std::cout << "OK: starpu::fill::submit<T> restricted to CPU\n";
}

#ifdef NNTILE_USE_CUDA
template<typename T>
void validate_cuda(Index nelems)
{
    // Get a StarPU CUDA worker (to perform computations on the same device)
    int cuda_worker_id = starpu_worker_get_by_type(STARPU_CUDA_WORKER, 0);
    // Choose worker CUDA device
    int dev_id = starpu_worker_get_devid(cuda_worker_id);
    cudaError_t cuda_err = cudaSetDevice(dev_id);
    TEST_ASSERT(cuda_err == cudaSuccess);
    // Create CUDA stream
    cudaStream_t stream;
    cuda_err = cudaStreamCreate(&stream);
    TEST_ASSERT(cuda_err == cudaSuccess);
    // Init all the data
    T val = -0.5;
    std::vector<T> data(nelems);
    for(Index i = 0; i < nelems; ++i)
    {
        data[i] = T(i+1);
    }
    // Create copies of destination
    std::vector<T> data2(data);
    // Launch low-level kernel
    T *dev_data;
    cuda_err = cudaMalloc(&dev_data, sizeof(T)*nelems);
    TEST_ASSERT(cuda_err == cudaSuccess);
    cuda_err = cudaMemcpy(dev_data, &data[0], sizeof(T)*nelems,
            cudaMemcpyHostToDevice);
    TEST_ASSERT(cuda_err == cudaSuccess);
    std::cout << "Run kernel::fill::cuda<T>\n";
    kernel::fill::cuda<T>(stream, nelems, val, dev_data);
    // Wait for result and destroy stream
    cuda_err = cudaStreamSynchronize(stream);
    TEST_ASSERT(cuda_err == cudaSuccess);
    cuda_err = cudaStreamDestroy(stream);
    TEST_ASSERT(cuda_err == cudaSuccess);
    // Copy result back to CPU
    cuda_err = cudaMemcpy(&data[0], dev_data, sizeof(T)*nelems,
            cudaMemcpyDeviceToHost);
    TEST_ASSERT(cuda_err == cudaSuccess);
    // Deallocate CUDA memory
    cuda_err = cudaFree(dev_data);
    TEST_ASSERT(cuda_err == cudaSuccess);
    // Check by actually submitting a task
    VariableHandle data2_handle(&data2[0], sizeof(T)*nelems, STARPU_RW);
    fill::restrict_where(STARPU_CUDA);
    std::cout << "Run starpu::fill::submit<T> restricted to CUDA\n";
    fill::submit<T>(nelems, val, data2_handle);
    starpu_task_wait_for_all();
    data2_handle.unregister();
    // Check result
    for(Index i = 0; i < nelems; ++i)
    {
        TEST_ASSERT(data[i] == data2[i]);
    }
    std::cout << "OK: starpu::fill::submit<T> restricted to CUDA\n";
}
#endif // NNTILE_USE_CUDA

int main(int argc, char **argv)
{
    // Init StarPU for testing
    Config starpu(1, 1, 0);
    // Init codelet
    fill::init();
    // Launch all tests
    validate_cpu<fp32_t>(1);
    validate_cpu<fp32_t>(10000);
    validate_cpu<fp64_t>(1);
    validate_cpu<fp64_t>(10000);
#ifdef NNTILE_USE_CUDA
    validate_cuda<fp32_t>(1);
    validate_cuda<fp32_t>(10000);
    validate_cuda<fp64_t>(1);
    validate_cuda<fp64_t>(10000);
#endif // NNTILE_USE_CUDA
    return 0;
}

