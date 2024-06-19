/*! @copyright (c) 2022-present Skolkovo Institute of Science and Technology
 *                              (Skoltech), Russia. All rights reserved.
 *                 2023-present Artificial Intelligence Research Institute
 *                              (AIRI), Russia. All rights reserved.
 *
 * NNTile is software framework for fast training of big neural networks on
 * distributed-memory heterogeneous systems based on StarPU runtime system.
 *
 * @file src/starpu/fp16_to_fp32.cc
 * Convert fp16_t array into fp32_t array
 *
 * @version 1.0.0
 * */

#include "nntile/starpu/fp16_to_fp32.hh"
#include "nntile/kernel/fp16_to_fp32.hh"

namespace nntile::starpu::fp16_to_fp32
{

#ifdef NNTILE_USE_CUDA
//! StarPU wrapper for kernel::fp16_to_fp32::cpu<T>
void cpu(void *buffers[], void *cl_args)
    noexcept
{
    // Get arguments
    Index nelems = reinterpret_cast<Index *>(cl_args)[0];
    // Get interfaces
    auto interfaces = reinterpret_cast<VariableInterface **>(buffers);
    const fp16_t *src = interfaces[0]->get_ptr<fp16_t>();
    fp32_t *dst = interfaces[1]->get_ptr<fp32_t>();
    // Launch kernel
    kernel::fp16_to_fp32::cpu(nelems, src, dst);
}

//! StarPU wrapper for kernel::fp16_to_fp32::cuda<T>
void cuda(void *buffers[], void *cl_args)
    noexcept
{
    // Get arguments
    Index nelems = reinterpret_cast<Index *>(cl_args)[0];
    // Get interfaces
    auto interfaces = reinterpret_cast<VariableInterface **>(buffers);
    const fp16_t *src = interfaces[0]->get_ptr<fp16_t>();
    fp32_t *dst = interfaces[1]->get_ptr<fp32_t>();
    // Get CUDA stream
    cudaStream_t stream = starpu_cuda_get_local_stream();
    // Launch kernel
    kernel::fp16_to_fp32::cuda(stream, nelems, src, dst);
}
#endif // NNTILE_USE_CUDA

Codelet codelet;

void init()
{
    codelet.init("nntile_fp16_to_fp32",
            nullptr,
#ifdef NNTILE_USE_CUDA
            {cpu},
            {cuda}
#else // NNTILE_USE_CUDA
            {},
            {}
#endif // NNTILE_USE_CUDA
            );
}

void restrict_where(uint32_t where)
{
    codelet.restrict_where(where);
}

void restore_where()
{
    codelet.restore_where();
}

void submit(Index nelems, Handle src, Handle dst)
{
    Index *nelems_ = new Index{nelems};
    //fp64_t nflops = 5 * nelems;
    int ret = starpu_task_insert(&codelet,
            STARPU_R, static_cast<starpu_data_handle_t>(src),
            STARPU_W, static_cast<starpu_data_handle_t>(dst),
            STARPU_CL_ARGS, nelems_, sizeof(*nelems_),
            //STARPU_FLOPS, nflops,
            0);
    // Check submission
    if(ret != 0)
    {
        throw std::runtime_error("Error in fp16_to_fp32 task submission");
    }
}

} // namespace nntile::starpu::fp16_to_fp32

