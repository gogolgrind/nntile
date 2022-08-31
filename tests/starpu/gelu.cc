/*! @copyright (c) 2022-2022 Skolkovo Institute of Science and Technology
 *                           (Skoltech). All rights reserved.
 *
 * NNTile is software framework for fast training of big neural networks on
 * distributed-memory heterogeneous systems based on StarPU runtime system.
 *
 * @file tests/starpu/gelu.cc
 * GeLU operation on a StarPU buffer
 *
 * @version 1.0.0
 * @author Aleksandr Mikhalev
 * @date 2022-08-31
 * */

#include "nntile/starpu/gelu.hh"
#include "nntile/kernel/gelu/cpu.hh"
#ifdef NNTILE_USE_CUDA
#   include "nntile/kernel/gelu/cuda.hh"
#   include <cuda_runtime.h>
#endif // NNTILE_USE_CUDA
#include "common.hh"
#include <vector>
#include <stdexcept>
#include <iostream>

using namespace nntile;

template<typename T>
void validate_cpu(Index nelems)
{
    // Init all the data
    std::vector<T> data(nelems);
    for(Index i = 0; i < nelems; ++i)
    {
        data[i] = T(i+1);
    }
    // Create copies of destination
    std::vector<T> data2(data);
    // Launch low-level kernel
    std::cout << "Run kernel::gelu::cpu<T>\n";
    kernel::gelu::cpu<T>(nelems, &data[0]);
    // Check by actually submitting a task
    StarpuVariableHandle data2_handle(&data2[0], sizeof(T)*nelems);
    starpu::gelu::restrict_where(STARPU_CPU);
    starpu_resume();
    std::cout << "Run starpu::gelu::submit<T> restricted to CPU\n";
    starpu::gelu::submit<T>(nelems, data2_handle);
    starpu_task_wait_for_all();
    data2_handle.unregister();
    starpu_pause();
    // Check result
    for(Index i = 0; i < nelems; ++i)
    {
        if(data[i] != data2[i])
        {
            throw std::runtime_error("StarPU submission wrong result");
        }
    }
    std::cout << "OK: starpu::gelu::submit<T> restricted to CPU\n";
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
    if(cuda_err != cudaSuccess)
    {
        throw std::runtime_error("CUDA error");
    }
    // Create CUDA stream
    cudaStream_t stream;
    cuda_err = cudaStreamCreate(&stream);
    if(cuda_err != cudaSuccess)
    {
        throw std::runtime_error("CUDA error");
    }
    // Init all the data
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
    if(cuda_err != cudaSuccess)
    {
        throw std::runtime_error("CUDA error");
    }
    cuda_err = cudaMemcpy(dev_data, &data[0], sizeof(T)*nelems,
            cudaMemcpyHostToDevice);
    if(cuda_err != cudaSuccess)
    {
        throw std::runtime_error("CUDA error");
    }
    std::cout << "Run kernel::gelu::cuda<T>\n";
    kernel::gelu::cuda<T>(stream, nelems, dev_data);
    // Wait for result and destroy stream
    cuda_err = cudaStreamSynchronize(stream);
    if(cuda_err != cudaSuccess)
    {
        throw std::runtime_error("CUDA error");
    }
    cuda_err = cudaStreamDestroy(stream);
    if(cuda_err != cudaSuccess)
    {
        throw std::runtime_error("CUDA error");
    }
    // Copy result back to CPU
    cuda_err = cudaMemcpy(&data[0], dev_data, sizeof(T)*nelems,
            cudaMemcpyDeviceToHost);
    if(cuda_err != cudaSuccess)
    {
        throw std::runtime_error("CUDA error");
    }
    // Deallocate CUDA memory
    cuda_err = cudaFree(dev_data);
    if(cuda_err != cudaSuccess)
    {
        throw std::runtime_error("CUDA error");
    }
    // Check by actually submitting a task
    StarpuVariableHandle data2_handle(&data2[0], sizeof(T)*nelems, STARPU_RW);
    starpu::gelu::restrict_where(STARPU_CUDA);
    starpu_resume();
    std::cout << "Run starpu::gelu::submit<T> restricted to CUDA\n";
    starpu::gelu<T>(nelems, data2_handle);
    starpu_task_wait_for_all();
    data2_handle.unregister();
    starpu_pause();
    // Check result
    for(Index i = 0; i < nelems; ++i)
    {
        if(data[i] != data2[i])
        {
            throw std::runtime_error("StarPU submission wrong result");
        }
    }
    std::cout << "OK: starpu::gelu::submit<T> restricted to CUDA\n";
}
#endif // NNTILE_USE_CUDA

int main(int argc, char **argv)
{
    // Init StarPU for testing
    StarpuTest starpu;
    // Init codelet
    starpu::gelu::init();
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

