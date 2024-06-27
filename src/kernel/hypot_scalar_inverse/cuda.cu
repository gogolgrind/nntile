/*! @copyright (c) 2022-present Skolkovo Institute of Science and Technology
 *                              (Skoltech), Russia. All rights reserved.
 *                 2023-present Artificial Intelligence Research Institute
 *                              (AIRI), Russia. All rights reserved.
 *
 * NNTile is software framework for fast training of big neural networks on
 * distributed-memory heterogeneous systems based on StarPU runtime system.
 *
 * @file src/kernel/hypot_scalar_inverse/cuda.cu
 * Inverse of a hypot operation of a buffer and a scalar on CUDA
 *
 * @version 1.0.0
 * */

#include "nntile/kernel/hypot_scalar_inverse/cuda.hh"
#include "nntile/kernel/cuda.hh"

namespace nntile::kernel::hypot_scalar_inverse
{

template<typename T>
static __global__
void cuda_kernel(Index nelems, T eps, T alpha, T* dst)
//! Inverse of a hypot of a buffer and a scalar on CUDA
/*! Performs the following operation:
 *      dst[i] = 1.0 / hypot(alpha*dst[i], eps),
 * where alpha and eps are non-zero scalars.
 *
 * @param[in] nelems: Size of the dst tensor
 * @param[in] eps: Scalar to be added to the hypot result
 * @param[in] alpha: Scalar multiplier for the dst tensor
 * @param[inout] dst: Destination of the hypot operation
 * */
{
    int i = threadIdx.x + blockIdx.x*blockDim.x;
    if(i < nelems)
    {
        dst[i] = T{1.0} / ::hypot(alpha*dst[i], eps);
    }
}

template<typename T>
void cuda(cudaStream_t stream, Index nelems, scal_t eps, scal_t alpha, T *dst_)
    noexcept
//! Inverse of a hypot of a buffer and a scalar on CUDA
/*! Performs the following operation:
 *      dst[i] = 1.0 / hypot(alpha*dst[i], eps),
 * where alpha and eps are non-zero scalars.
 *
 * @param[in] nelems: Size of the dst tensor
 * @param[in] eps: Scalar to be added to the hypot result
 * @param[in] alpha: Scalar multiplier for the dst tensor
 * @param[inout] dst_: Destination of the hypot operation
 * */
{
    dim3 blocks((nelems+255)/256), threads(256);
    using Y = typename CUDAComputeType<T>::value;
    auto dst = reinterpret_cast<Y *>(dst_);
    (cuda_kernel<T>)<<<blocks, threads, 0, stream>>>(nelems, Y{eps}, Y{alpha},
            dst);
}

// Explicit instantiation
template
void cuda<fp32_t>(cudaStream_t stream, Index nelems, scal_t eps, scal_t alpha,
        fp32_t *dst)
    noexcept;

template
void cuda<fp64_t>(cudaStream_t stream, Index nelems, scal_t eps, scal_t alpha,
        fp64_t *dst)
    noexcept;

} // namespace nntile::kernel::hypot_scalar_inverse
