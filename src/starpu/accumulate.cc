/*! @copyright (c) 2022-present Skolkovo Institute of Science and Technology
 *                              (Skoltech), Russia. All rights reserved.
 *                 2023-present Artificial Intelligence Research Institute
 *                              (AIRI), Russia. All rights reserved.
 *
 * NNTile is software framework for fast training of big neural networks on
 * distributed-memory heterogeneous systems based on StarPU runtime system.
 *
 * @file src/starpu/accumulate.cc
 * Accumulate one StarPU buffers into another
 *
 * @version 1.0.0
 * */

#include "nntile/starpu/accumulate.hh"
#include "nntile/kernel/add.hh"
#include <cstdlib>

//! StarPU wrappers for accumulate operation
namespace nntile::starpu::accumulate
{

//! Apply accumulate operation for StarPU buffers in CPU
template<typename T>
void cpu(void *buffers[], void *cl_args)
    noexcept
{
    // Get interfaces
    auto interfaces = reinterpret_cast<VariableInterface **>(buffers);
    Index nelems = interfaces[0]->elemsize / sizeof(T);
    T *dst = interfaces[0]->get_ptr<T>();
    const T *src = interfaces[1]->get_ptr<T>();
    // Launch kernel
    kernel::add::cpu<T>(nelems, 1.0, src, 1.0, dst);
}

#ifdef NNTILE_USE_CUDA
//! Apply accumulate for StarPU buffers on CUDA
template<typename T>
void cuda(void *buffers[], void *cl_args)
    noexcept
{
    // Get interfaces
    auto interfaces = reinterpret_cast<VariableInterface **>(buffers);
    Index nelems = interfaces[0]->elemsize / sizeof(T);
    T *dst = interfaces[0]->get_ptr<T>();
    const T *src = interfaces[1]->get_ptr<T>();
    // Get CUDA stream
    cudaStream_t stream = starpu_cuda_get_local_stream();
    // Launch kernel
    kernel::add::cuda<T>(stream, nelems, 1.0, src, 1.0, dst);
}
#endif // NNTILE_USE_CUDA

Codelet codelet_fp32, codelet_fp64;

void init()
{
    codelet_fp32.init("nntile_accumulate_fp32",
            nullptr,
            {cpu<fp32_t>},
#ifdef NNTILE_USE_CUDA
            {cuda<fp32_t>}
#else // NNTILE_USE_CUDA
            {}
#endif // NNTILE_USE_CUDA
            );
    codelet_fp32.nbuffers = 2;
    codelet_fp32.modes[0] = static_cast<starpu_data_access_mode>(
            //STARPU_RW | STARPU_COMMUTE);
            STARPU_RW);
    codelet_fp32.modes[1] = STARPU_R;
    codelet_fp64.init("nntile_accumulate_fp64",
            nullptr,
            {cpu<fp64_t>},
#ifdef NNTILE_USE_CUDA
            {cuda<fp64_t>}
#else // NNTILE_USE_CUDA
            {}
#endif // NNTILE_USE_CUDA
            );
    codelet_fp64.nbuffers = 2;
    codelet_fp64.modes[0] = static_cast<starpu_data_access_mode>(
            //STARPU_RW | STARPU_COMMUTE);
            STARPU_RW);
    codelet_fp64.modes[1] = STARPU_R;
}

void restrict_where(uint32_t where)
{
    codelet_fp32.restrict_where(where);
    codelet_fp64.restrict_where(where);
}

void restore_where()
{
    codelet_fp32.restore_where();
    codelet_fp64.restore_where();
}

template<typename T>
void submit(Handle src, Handle dst)
//! Insert accumulate task into StarPU pool of tasks
/*! No argument checking is performed. All the inputs are packed and passed to
 * starpu_task_insert() function. If task submission fails, this routines
 * throws an std::runtime_error() exception.
 * */
{
    // Submit task
    int ret = starpu_task_insert(codelet<T>(),
            //STARPU_RW|STARPU_COMMUTE, static_cast<starpu_data_handle_t>(dst),
            STARPU_RW, static_cast<starpu_data_handle_t>(dst),
            STARPU_R, static_cast<starpu_data_handle_t>(src),
            // STARPU_FLOPS, nflops,
            0);
    // Check submission
    if(ret != 0)
    {
        throw std::runtime_error("Error in accumulate task submission");
    }
}

// Explicit instantiation
template
void submit<fp32_t>(Handle src, Handle dst);

template
void submit<fp64_t>(Handle src, Handle dst);

} // namespace nntile::starpu::accumulate

