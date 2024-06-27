/*! @copyright (c) 2022-present Skolkovo Institute of Science and Technology
 *                              (Skoltech), Russia. All rights reserved.
 *                 2023-present Artificial Intelligence Research Institute
 *                              (AIRI), Russia. All rights reserved.
 *
 * NNTile is software framework for fast training of big neural networks on
 * distributed-memory heterogeneous systems based on StarPU runtime system.
 *
 * @file src/kernel/adamw_step/cuda.cu
 * AdamW step with buffers on CUDA
 *
 * @version 1.0.0
 * */

#include "nntile/kernel/adamw_step/cuda.hh"
#include "nntile/kernel/cuda.hh"

namespace nntile::kernel::adamw_step
{

template<typename T>
static __global__
void cuda_kernel(Index num_iter, Index num_elems, T beta_1, T beta_2, T eps,
        T lr, T weight_decay, const T *grad, T *first_moment, T *second_moment,
        T *p, T alpha, T beta)
{
    int i = threadIdx.x + blockIdx.x*blockDim.x;
    if(i < num_elems)
    {
        // Read values (param+grad) from RAM only once
        T p_val = p[i], grad_val = grad[i];
        if (weight_decay != 0)
        {
            p_val *= 1 - lr*weight_decay;
        }
        // Read values (first+second moments) from RAM no more than once and
        // update them in the RAM immediately
        T f_val, s_val;
        if(num_iter == 1)
        {
            f_val = (1-beta_1) * grad_val;
            first_moment[i] = f_val;
            s_val = ::sqrt(1-beta_2) * ::fabs(grad_val);
            second_moment[i] = s_val;
        }
        else
        {
            f_val = first_moment[i];
            s_val = second_moment[i];
            f_val = beta_1*f_val + (1-beta_1)*grad_val;
            first_moment[i] = f_val;
            s_val = ::hypot(::sqrt(beta_2)*s_val, ::sqrt(1-beta_2)*grad_val);
            second_moment[i] = s_val;
        }
        // Update parameters using only data in registers
        T denom = s_val*beta + eps;
        p[i] = p_val - alpha*f_val/denom;
    }
}

template<typename T>
void cuda(cudaStream_t stream, Index num_iter, Index num_elems, scal_t beta_1,
        scal_t beta_2, scal_t eps, scal_t lr, scal_t weight_decay,
        const T *grad_, T *first_moment_, T *second_moment_, T *p_)
    noexcept
//! Fused AdamW step operation of buffers
/*!
 *
* @param[in] num_iters: current iteration number
* @param[in] num_elems: Number of elements in buffers
* @param[in] beta_1: parameter for moving average of first moments
* @param[in] beta_2: parameter for moving average of second moments
* @param[in] eps: small scalar to avoid division by zero
* @param[in] lr: learning rate
* @param[in] grad_: Input buffer stored gradient
* @param[in] first_moment_: Input buffer stored first moments
* @param[in] second_moment_: Input buffer stored second moments
* @param[inout] p_: Input buffers with parameter that are updated in the end
 * */
{
    dim3 blocks((num_elems+255)/256), threads(256);
    using Y = typename CUDAComputeType<T>::value;
    const Y alpha = lr / (Y{1.0} - std::pow(beta_1, num_iter));
    const Y beta = Y{1.0} / std::sqrt(Y{1.0} - std::pow(beta_2, num_iter));
    auto grad = reinterpret_cast<const Y *>(grad_);
    auto first_moment = reinterpret_cast<Y *>(first_moment_);
    auto second_moment = reinterpret_cast<Y *>(second_moment_);
    auto p = reinterpret_cast<Y *>(p_T);
    (cuda_kernel<Y>)<<<blocks, threads, 0, stream>>>(num_iter, num_elems,
            Y{beta_1}, Y{beta_2}, Y{eps}, Y{lr}, Y{weight_decay}, grad,
            first_moment, second_moment, p, alpha, beta);
}

// Explicit instantiation
template
void cuda<fp32_t>(cudaStream_t stream, Index num_iter, Index num_elems,
        scal_t beta_1, scal_t beta_2, scal_t eps, scal_t lr,
        scal_t weight_decay, const fp32_t *grad, fp32_t *first_moment,
        fp32_t *second_moment, fp32_t *p)
    noexcept;

template
void cuda<fp64_t>(cudaStream_t stream, Index num_iter, Index num_elems,
        scal_t beta_1, scal_t beta_2, scal_t eps, scal_t lr,
        scal_t weight_decay, const fp64_t *grad, fp64_t *first_moment,
        fp64_t *second_moment, fp64_t *p)
    noexcept;

} // namespace nntile::kernel::adamw_step
