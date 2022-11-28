/*! @copyright (c) 2022-2022 Skolkovo Institute of Science and Technology
 *                           (Skoltech). All rights reserved.
 *
 * NNTile is software framework for fast training of big neural networks on
 * distributed-memory heterogeneous systems based on StarPU runtime system.
 *
 * @file include/nntile/kernel/norm/cuda.hh
 * Euclidian norm of a buffer on CUDA
 *
 * @version 1.0.0
 * @author Aleksandr Mikhalev
 * @date 2022-11-28
 * */

#pragma once

#include <nntile/base_types.hh>
#include <cuda_runtime.h>

namespace nntile
{
namespace kernel
{
namespace norm
{

template<typename T>
void cuda(cudaStream_t stream, Index m, Index n, Index k, const T *src,
        T *norm)
    noexcept;

} // namespace norm
} // namespace kernel
} // namespace nntile

