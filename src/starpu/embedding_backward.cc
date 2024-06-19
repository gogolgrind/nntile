/*! @copyright (c) 2022-present Skolkovo Institute of Science and Technology
 *                              (Skoltech), Russia. All rights reserved.
 *                 2023-present Artificial Intelligence Research Institute
 *                              (AIRI), Russia. All rights reserved.
 *
 * NNTile is software framework for fast training of big neural networks on
 * distributed-memory heterogeneous systems based on StarPU runtime system.
 *
 * @file src/starpu/embedding_backward.cc
 * Embeddings from vocabulary within StarPU buffers
 *
 * @version 1.0.0
 * */

#include "nntile/starpu/embedding_backward.hh"
#include "nntile/kernel/embedding_backward.hh"

namespace nntile::starpu::embedding_backward
{

//! Copy embedding from vocabulary within StarPU buffers on CPU
template<typename T>
void cpu(void *buffers[], void *cl_args)
    noexcept
{
    // Get arguments
    auto args = reinterpret_cast<args_t *>(cl_args);
    // Get interfaces
    auto interfaces = reinterpret_cast<VariableInterface **>(buffers);
    const Index *index = interfaces[0]->get_ptr<Index>();
    const T *embed = interfaces[1]->get_ptr<T>();
    T *vocab = interfaces[2]->get_ptr<T>();
    // Accumulate vocab gradients
    kernel::embedding_backward::cpu<T>(args->m, args->n, args->k,
            args->k_start, args->k_size, index, embed, vocab);
}

#ifdef NNTILE_USE_CUDA
//! Copy embedding from vocabulary within StarPU buffers on CUDA
template<typename T>
void cuda(void *buffers[], void *cl_args)
    noexcept
{
    // Get arguments
    auto args = reinterpret_cast<args_t *>(cl_args);
    // Get interfaces
    auto interfaces = reinterpret_cast<VariableInterface **>(buffers);
    const Index *index = interfaces[0]->get_ptr<Index>();
    const T *embed = interfaces[1]->get_ptr<T>();
    T *vocab = interfaces[2]->get_ptr<T>();
    // Get CUDA stream
    cudaStream_t stream = starpu_cuda_get_local_stream();
    // Accumulate vocab gradients
    kernel::embedding_backward::cuda<T>(stream, args->m, args->n, args->k,
            args->k_start, args->k_size, index, embed, vocab);
}
#endif // NNTILE_USE_CUDA

//! Footprint for embedding tasks that depends only on cl_arg
static
uint32_t footprint(struct starpu_task *task)
{
    // Get arguments
    auto args = reinterpret_cast<args_t *>(task->cl_arg);
    // Apply hash over parameters m, n, k and k_size.
    uint32_t hash = 0;
    hash = starpu_hash_crc32c_be_n(&args->m, sizeof(args->m), hash);
    hash = starpu_hash_crc32c_be_n(&args->n, sizeof(args->n), hash);
    hash = starpu_hash_crc32c_be_n(&args->k, sizeof(args->k), hash);
    hash = starpu_hash_crc32c_be_n(&args->k_size, sizeof(args->k_size), hash);
    return hash;
}

Codelet codelet_fp32, codelet_fp64;

void init()
{
    codelet_fp32.init("nntile_embedding_backward_fp32",
            footprint,
            {cpu<fp32_t>},
#ifdef NNTILE_USE_CUDA
            {cuda<fp32_t>}
#else // NNTILE_USE_CUDA
            {}
#endif // NNTILE_USE_CUDA
            );
    codelet_fp64.init("nntile_embedding_backward_fp64",
            footprint,
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
void submit(Index m, Index n, Index k, Index k_start, Index k_size,
        Handle index, Handle embed, Handle vocab, int redux)
//! Insert embedding_backward task into StarPU pool of tasks
/*! No argument checking is performed. All the inputs are packed and passed to
 * starpu_task_insert() function. If task submission fails, this routines
 * throws an std::runtime_error() exception.
 * */
{
    // Codelet arguments
    args_t *args = (args_t *)std::malloc(sizeof(*args));
    args->m = m;
    args->n = n;
    args->k = k;
    args->k_start = k_start;
    args->k_size = k_size;
    fp64_t nflops = m * n * k_size;
    // Access mode for the output vocab handle
    enum starpu_data_access_mode vocab_mode;
    if(redux != 0)
    {
        vocab_mode = STARPU_REDUX;
        //vocab_mode = Config::STARPU_RW_COMMUTE;
    }
    else
    {
        vocab_mode = Config::STARPU_RW_COMMUTE;
    }
    // Submit task
    int ret = starpu_task_insert(codelet<T>(),
            STARPU_R, static_cast<starpu_data_handle_t>(index),
            STARPU_R, static_cast<starpu_data_handle_t>(embed),
            vocab_mode, static_cast<starpu_data_handle_t>(vocab),
            STARPU_CL_ARGS, args, sizeof(*args),
            STARPU_FLOPS, nflops,
            0);
    // Check submission
    if(ret != 0)
    {
        throw std::runtime_error("Error in embedding_backward task "
                "submission");
    }
}

// Explicit instantiation
template
void submit<fp32_t>(Index m, Index n, Index k, Index k_start, Index k_size,
        Handle index, Handle embed, Handle vocab, int redux);

template
void submit<fp64_t>(Index m, Index n, Index k, Index k_start, Index k_size,
        Handle index, Handle embed, Handle vocab, int redux);

} // namespace nntile::starpu::embedding_backward

