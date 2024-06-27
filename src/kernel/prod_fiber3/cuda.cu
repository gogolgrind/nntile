/*! @copyright (c) 2022-present Skolkovo Institute of Science and Technology
 *                              (Skoltech), Russia. All rights reserved.
 *                 2023-present Artificial Intelligence Research Institute
 *                              (AIRI), Russia. All rights reserved.
 *
 * NNTile is software framework for fast training of big neural networks on
 * distributed-memory heterogeneous systems based on StarPU runtime system.
 *
 * @file src/kernel/prod_fiber3/cuda.cu
 * Per-element multiplication of a tensor by a broadcasted fiber on CUDA
 *
 * @version 1.0.0
 * */

#include "nntile/kernel/prod_fiber3/cuda.hh"
#include <algorithm>
#include "nntile/kernel/cuda.hh"

namespace nntile::kernel::prod_fiber3
{

template<typename T>
static __global__
void cuda_kernel(Index m, Index n, Index k, T alpha,
        const T * __restrict__ src1, const T * __restrict__ src2,
        T * __restrict__ dst)
//! Per-element product of a tensor and a broadcasted fiber on CPU
/*! Performs the following operations:
 *      dst[i,l,j] = alpha * src1[l] * src2[i,l,j]
 *
 * @param[in] m: Size of the first mode of dst tensor
 * @param[in] n: Size of the last mode of dst tensor
 * @param[in] k: Size of the middle mode of dst tensor and the only mode of src
 *      tensor
 * @param[in] alpha: Scalar factor
 * @param[in] src1: Input contiguous vector with k elements
 * @param[in] src2: Input contiguous m-by-k-by-n array
 * @param[out] dst: Output contiguous m-by-k-by-n array
 * */
{
    Index i0 = threadIdx.x + blockIdx.x*blockDim.x,
          i1 = threadIdx.y + blockIdx.y*blockDim.y,
          i2 = threadIdx.z + blockIdx.z*blockDim.z;
    if(i0 < m and i1 < n and i2 < k)
    {
        const T src1_val = alpha * src1[i2];
        // Input fiber to be used
        const T *src2_fiber = src2 + (i1*k+i2)*m;
        // Output fiber to be updated
        T *dst_fiber = dst + (i1*k+i2)*m;
        // Update output value
        dst_fiber[i0] = src1_val * src2_fiber[i0];
    }
}

template<typename T>
void cuda(cudaStream_t stream, Index m, Index n, Index k, scal_t alpha,
        const T *src1_, const T *src2_, T *dst_)
    noexcept
//! Per-element product of a tensor and a broadcasted fiber on CPU
//! Per-element product of a tensor and a broadcasted fiber on CPU
/*! Performs the following operations:
 *      dst[i,l,j] = alpha * src1[l] * src2[i,l,j]
 *
 * @param[in] m: Size of the first mode of dst tensor
 * @param[in] n: Size of the last mode of dst tensor
 * @param[in] k: Size of the middle mode of dst tensor and the only mode of src
 *      tensor
 * @param[in] alpha: Scalar factor
 * @param[in] src1_: Input contiguous vector with k elements
 * @param[in] src2_: Input contiguous m-by-k-by-n array
 * @param[out] dst_: Output contiguous m-by-k-by-n array
 * */
{
    // Both source and destination are Fortran-contiguous
    dim3 threads(std::min(int(m), 8), std::min(int(n), 8),
            std::min(int(k), 16));
    dim3 blocks((m+threads.x-1)/threads.x, (n+threads.y-1)/threads.y,
            (k+threads.z-1)/threads.z);
    using Y = typename CUDAComputeType<T>::value;
    auto src1 = reinterpret_cast<const Y *>(src1_);
    auto src2 = reinterpret_cast<const Y *>(src2_);
    auto dst = reinterpret_cast<Y *>(dst_);
    (cuda_kernel<Y>)<<<blocks, threads, 0, stream>>>(m, n, k, Y{alpha}, src1,
            src2, dst);
}

// Explicit instantiation
template
void cuda<fp32_t>(cudaStream_t stream, Index m, Index n, Index k, scal_t alpha,
        const fp32_t *src1, const fp32_t *src2, fp32_t *dst)
    noexcept;

template
void cuda<fp64_t>(cudaStream_t stream, Index m, Index n, Index k, scal_t alpha,
        const fp64_t *src1, const fp64_t *src2, fp64_t *dst)
    noexcept;

} // namespace nntile::kernel::prod_fiber3
