/*! @copyright (c) 2022-present Skolkovo Institute of Science and Technology
 *                              (Skoltech), Russia. All rights reserved.
 *                 2023-present Artificial Intelligence Research Institute
 *                              (AIRI), Russia. All rights reserved.
 *
 * NNTile is software framework for fast training of big neural networks on
 * distributed-memory heterogeneous systems based on StarPU runtime system.
 *
 * @file src/starpu/hypot.cc
 * hypot operation on a StarPU buffers
 *
 * @version 1.0.0
 * */

#include "nntile/starpu/hypot.hh"
#include "nntile/kernel/hypot.hh"
#include "nntile/starpu/scal.hh"
#include "nntile/starpu/clear.hh"
#include "nntile/starpu/scal_inplace.hh"
#include <cstdlib>

//! StarPU wrappers for hypot operation
namespace nntile::starpu::hypot
{

//! Apply hypot operation for StarPU buffers in CPU
template<typename T>
void cpu(void *buffers[], void *cl_args)
    noexcept
{
    // Get arguments
    auto args = reinterpret_cast<args_t<T> *>(cl_args);
    // Get interfaces
    auto interfaces = reinterpret_cast<VariableInterface **>(buffers);
    const T *src = interfaces[0]->get_ptr<T>();
    T *dst = interfaces[1]->get_ptr<T>();
    // Launch kernel
    kernel::hypot::cpu<T>(args->nelems, args->alpha, src, args->beta, dst);
}

#ifdef NNTILE_USE_CUDA
//! Apply hypot for StarPU buffers on CUDA
template<typename T>
void cuda(void *buffers[], void *cl_args)
    noexcept
{
    // Get arguments
    auto args = reinterpret_cast<args_t<T> *>(cl_args);
    // Get interfaces
    auto interfaces = reinterpret_cast<VariableInterface **>(buffers);
    const T *src = interfaces[0]->get_ptr<T>();
    T *dst = interfaces[1]->get_ptr<T>();
    // Get CUDA stream
    cudaStream_t stream = starpu_cuda_get_local_stream();
    // Launch kernel
    kernel::hypot::cuda<T>(stream, args->nelems, args->alpha, src,
            args->beta, dst);
}
#endif // NNTILE_USE_CUDA

Codelet codelet_fp32, codelet_fp64;

void init()
{
    codelet_fp32.init("nntile_hypot_fp32",
            nullptr,
            {cpu<fp32_t>},
#ifdef NNTILE_USE_CUDA
            {cuda<fp32_t>}
#else // NNTILE_USE_CUDA
            {}
#endif // NNTILE_USE_CUDA
            );
    codelet_fp64.init("nntile_hypot_fp64",
            nullptr,
            {cpu<fp64_t>},
#ifdef NNTILE_USE_CUDA
            {cuda<fp64_t>}
#else // NNTILE_USE_CUDA
            {}
#endif // NNTILE_USE_CUDA
            );
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
void submit(Index nelems, T alpha, Handle src, T beta, Handle dst)
//! Insert hypot task into StarPU pool of tasks
/*! No argument checking is performed. All the inputs are packed and passed to
 * starpu_task_insert() function. If task submission fails, this routines
 * throws an std::runtime_error() exception.
 * */
{
    constexpr T zero = 0, one = 1;
    // If beta is zero this function reduces to scal
    //if(beta == zero)
    //{
    //    throw std::runtime_error("modulus(apha*src) is not yet implemented");
    //    //scal::submit<T>(nelems, alpha, src, dst);
    //    return;
    //}
    // If beta is non-zero and alpha is zero then reduce to scal_inplace
    //if(alpha == zero)
    //{
    //    throw std::runtime_error("modulus_inplace(beta*dst) is not yet "
    //            "implemented");
    //    //scal_inplace::submit<T>(nelems, beta, dst);
    //    return;
    //}
    // Access mode for the dst handle
    enum starpu_data_access_mode dst_mode;
    if(beta == one)
    {
        dst_mode = Config::STARPU_RW_COMMUTE;
    }
    else if(beta == zero)
    {
        dst_mode = STARPU_W;
    }
    else
    {
        dst_mode = STARPU_RW;
    }
    // Codelet arguments
    args_t<T> *args = (args_t<T> *)std::malloc(sizeof(*args));
    args->nelems = nelems;
    args->alpha = alpha;
    args->beta = beta;
    // Submit task
    int ret = starpu_task_insert(codelet<T>(),
            STARPU_R, static_cast<starpu_data_handle_t>(src),
            STARPU_CL_ARGS, args, sizeof(*args),
            dst_mode, static_cast<starpu_data_handle_t>(dst), 0);
            // STARPU_FLOPS, nflops);
    // Check submission
    if(ret != 0)
    {
        throw std::runtime_error("Error in hypot task submission");
    }
}

// Explicit instantiation
template
void submit<fp32_t>(Index nelems, fp32_t alpha, Handle src, fp32_t beta,
        Handle dst);

template
void submit<fp64_t>(Index nelems, fp64_t alpha, Handle src, fp64_t beta,
        Handle dst);

} // namespace nntile::starpu::hypot

