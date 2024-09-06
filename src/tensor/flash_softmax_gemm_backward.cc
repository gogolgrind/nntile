/*! @copyright (c) 2022-present Skolkovo Institute of Science and Technology
 *                              (Skoltech), Russia. All rights reserved.
 *                 2023-present Artificial Intelligence Research Institute
 *                              (AIRI), Russia. All rights reserved.
 *
 * NNTile is software framework for fast training of big neural networks on
 * distributed-memory heterogeneous systems based on StarPU runtime system.
 *
 * @file src/tensor/flash_softmax_gemm_backward.cc
 * Fast backward of softmax and gemm operations
 *
 * @version 1.1.0
 * */

#include "nntile/tensor/flash_softmax_gemm_backward.hh"
#include "nntile/starpu/flash_softmax_gemm_backward_sumprod_slice.hh"
#include "nntile/starpu/flash_softmax_gemm_backward_dq_dk.hh"
#include <cmath>
#include <limits>

namespace nntile::tensor
{

template<typename T>
void flash_softmax_gemm_backward_async(const Tensor<T> &Q, const Tensor<T> &dQ,
        const Tensor<T> &K, const Tensor<T> &dK, const Tensor<T> &V,
        const Tensor<T> &dV, const Tensor<bool_t> &mask,
        const Tensor<T> &maxsumexp, const Tensor<T> &dst_grad,
        const Tensor<T> &tmp, const Tensor<T> &tmp_grad,
        const Tensor<T> &tmp_sumprod_slice, int redux)
{
//    // Check dimensions
//    if(src.ndim != dst.ndim)
//    {
//        throw std::runtime_error("src.ndim != dst.ndim");
//    }
//    // Treat special case of src.ndim=0
//    if(src.ndim == 0)
//    {
//        throw std::runtime_error("Scalar input makes no sense");
//    }
//    // Check axis
//    if(axis < 0)
//    {
//        throw std::runtime_error("axis < 0");
//    }
//    if(axis >= src.ndim)
//    {
//        throw std::runtime_error("axis >= src.ndim");
//    }
//    // Check shapes of src and dst
//    if(dst.shape[0] != 2)
//    {
//        throw std::runtime_error("dst.shape[0] != 2");
//    }
//    if(dst.basetile_shape[0] != 2)
//    {
//        throw std::runtime_error("dst.basetile_shape[0] != 2");
//    }
//    for(Index i = 0; i < axis; ++i)
//    {
//        if(src.shape[i] != dst.shape[i+1])
//        {
//            throw std::runtime_error("src.shape[i] != dst.shape[i+1]");
//        }
//        if(src.basetile_shape[i] != dst.basetile_shape[i+1])
//        {
//            throw std::runtime_error("src.basetile_shape[i] != "
//                    "dst.basetile_shape[i+1]");
//        }
//    }
//    for(Index i = axis+1; i < src.ndim; ++i)
//    {
//        if(src.shape[i] != dst.shape[i])
//        {
//            throw std::runtime_error("src.shape[i] != dst.shape[i]");
//        }
//        if(src.basetile_shape[i] != dst.basetile_shape[i])
//        {
//            throw std::runtime_error("src.basetile_shape[i] != "
//                    "dst.basetile_shape[i]");
//        }
//    }
    // Do actual calculations
    int ret;
    Index head_size = Q.shape[0];
    Index n_seq_tile = Q.basetile_shape[1];
    Index n_batch_tile = Q.basetile_shape[2];
    Index n_head_tile;
    // Support both GPT2 and Llama attention inputs, that differ by shape of
    // inputs:
    // GPT2: Q is  (head_size, n_seq, n_batch, n_head)
    // Llama: Q is (head_size, n_seq, n_batch, kv_group_size, n_head_kv)
    if(Q.ndim == 4)
    {
        // GPT2 case
        n_head_tile = Q.basetile_shape[3];
    }
    else
    {
        // Llama case
        n_head_tile = Q.basetile_shape[3] * Q.basetile_shape[4];
    }
    // Cycle for all tiles of dV tensor
    for(Index i = 0; i < dV.grid.nelems; ++i)
    {
        auto dQ_tile_handle = dQ.get_tile_handle(i);
        auto dK_tile_handle = dK.get_tile_handle(i);
        auto dV_tile_handle = dV.get_tile_handle(i);
        auto dV_tile_index = dV.grid.linear_to_index(i);
        // Indices of all required tensors
        std::vector<Index> tmp_tile_index(dV_tile_index),
            q_tile_index(dV_tile_index), dq_tile_index(dV_tile_index),
            k_tile_index(dV_tile_index), dk_tile_index(dV_tile_index),
            v_tile_index(dV_tile_index), dst_grad_tile_index(dV_tile_index),
            mask_tile_index(2), maxsumexp_tile_index(dV_tile_index),
            tmp_grad_tile_index(dV_tile_index),
            tmp_sumprod_slice_tile_index(Q.ndim-1);
        auto k_tile_handle = K.get_tile_handle(k_tile_index);
        auto v_tile_handle = V.get_tile_handle(v_tile_index);
        tmp_tile_index[0] = dV_tile_index[1];
        tmp_grad_tile_index[0] = dV_tile_index[1];
        for(Index j = 1; j < Q.ndim-1; ++j)
        {
            tmp_sumprod_slice_tile_index[j] = dV_tile_index[j+1];
        }
        mask_tile_index[0] = dV_tile_index[1];
        // Clear destination buffers at first
        starpu::clear::submit(dQ_tile_handle);
        starpu::clear::submit(dK_tile_handle);
        starpu::clear::submit(dV_tile_handle);
        // Launch kernel for each appropriate tile of K and V to accumulate
        // result into destination tensor
        for(Index j = 0; j < Q.grid.shape[1]; ++j)
        {
            tmp_tile_index[1] = j;
            tmp_grad_tile_index[1] = j;
            q_tile_index[1] = j;
            dst_grad_tile_index[1] = j;
            mask_tile_index[1] = j;
            maxsumexp_tile_index[1] = j;
            tmp_sumprod_slice_tile_index[0] = j;
            auto tmp_tile_handle = tmp.get_tile_handle(tmp_tile_index);
            auto tmp_grad_tile_handle = tmp_grad.get_tile_handle(
                    tmp_grad_tile_index);
            auto tmp_sumprod_slice_tile_handle = tmp_sumprod_slice
                .get_tile_handle(tmp_sumprod_slice_tile_index);
            auto q_tile_handle = Q.get_tile_handle(q_tile_index);
            auto dst_grad_tile_handle = dst_grad.get_tile_handle(
                    dst_grad_tile_index);
            auto mask_tile_handle = mask.get_tile_handle(mask_tile_index);
            auto maxsumexp_tile_handle = maxsumexp.get_tile_handle(
                    maxsumexp_tile_index);
            // Insert a fused task
            starpu::flash_softmax_gemm_backward_sumprod_slice::submit<T>(
                    n_seq_tile, head_size, n_batch_tile*n_head_tile,
                    k_tile_handle, q_tile_handle, mask_tile_handle,
                    maxsumexp_tile_handle, dst_grad_tile_handle, v_tile_handle,
                    dV_tile_handle, tmp_sumprod_slice_tile_handle,
                    tmp_tile_handle, tmp_grad_tile_handle, redux=0);
        }
    }
    // Cycle for all tiles of dK/dV tensor
    for(Index i = 0; i < dV.grid.nelems; ++i)
    {
        auto dK_tile_handle = dK.get_tile_handle(i);
        auto dV_tile_index = dV.grid.linear_to_index(i);
        // Indices of all required tensors
        std::vector<Index> tmp_tile_index(dV_tile_index),
            q_tile_index(dV_tile_index), dq_tile_index(dV_tile_index),
            k_tile_index(dV_tile_index), dk_tile_index(dV_tile_index),
            v_tile_index(dV_tile_index), dst_grad_tile_index(dV_tile_index),
            mask_tile_index(2), maxsumexp_tile_index(dV_tile_index),
            tmp_grad_tile_index(dV_tile_index),
            tmp_sumprod_slice_tile_index(Q.ndim-1);
        auto k_tile_handle = K.get_tile_handle(k_tile_index);
        auto v_tile_handle = V.get_tile_handle(v_tile_index);
        tmp_tile_index[0] = dV_tile_index[1];
        tmp_grad_tile_index[0] = dV_tile_index[1];
        for(Index j = 1; j < Q.ndim-1; ++j)
        {
            tmp_sumprod_slice_tile_index[j] = dV_tile_index[j+1];
        }
        mask_tile_index[0] = dV_tile_index[1];
        for(Index j = 0; j < dQ.grid.shape[1]; ++j)
        {
            tmp_tile_index[1] = j;
            tmp_grad_tile_index[1] = j;
            q_tile_index[1] = j;
            dst_grad_tile_index[1] = j;
            mask_tile_index[1] = j;
            maxsumexp_tile_index[1] = j;
            tmp_sumprod_slice_tile_index[0] = j;
            dq_tile_index[1] = j;
            auto tmp_tile_handle = tmp.get_tile_handle(tmp_tile_index);
            auto tmp_grad_tile_handle = tmp_grad.get_tile_handle(
                    tmp_grad_tile_index);
            auto tmp_sumprod_slice_tile_handle = tmp_sumprod_slice
                .get_tile_handle(tmp_sumprod_slice_tile_index);
            auto q_tile_handle = Q.get_tile_handle(q_tile_index);
            auto dst_grad_tile_handle = dst_grad.get_tile_handle(
                    dst_grad_tile_index);
            auto mask_tile_handle = mask.get_tile_handle(mask_tile_index);
            auto maxsumexp_tile_handle = maxsumexp.get_tile_handle(
                    maxsumexp_tile_index);
            auto dQ_tile_handle = dQ.get_tile_handle(dq_tile_index);
            // Insert a fused task
            starpu::flash_softmax_gemm_backward_dq_dk::submit<T>(
                    n_seq_tile, head_size, n_batch_tile*n_head_tile,
                    k_tile_handle, q_tile_handle, mask_tile_handle,
                    maxsumexp_tile_handle, dst_grad_tile_handle, v_tile_handle,
                    tmp_sumprod_slice_tile_handle, dQ_tile_handle, dK_tile_handle,
                    tmp_tile_handle, tmp_grad_tile_handle, redux=0);
        }
    }
}

template<typename T>
void flash_softmax_gemm_backward(const Tensor<T> &Q, const Tensor<T> &dQ,
        const Tensor<T> &K, const Tensor<T> &dK, const Tensor<T> &V,
        const Tensor<T> &dV, const Tensor<bool_t> &mask,
        const Tensor<T> &maxsumexp, const Tensor<T> &dst_grad,
        const Tensor<T> &tmp, const Tensor<T> &tmp_grad,
        const Tensor<T> &tmp_sumprod_slice, int redux)
{
    flash_softmax_gemm_backward_async<T>(Q, dQ, K, dK, V, dV, mask, maxsumexp,
            dst_grad, tmp, tmp_grad, tmp_sumprod_slice, redux);
    starpu_task_wait_for_all();
    starpu_mpi_wait_for_all(MPI_COMM_WORLD);
}

// Explicit instantiation
template
void flash_softmax_gemm_backward_async(const Tensor<fp32_t> &Q, const Tensor<fp32_t> &dQ,
        const Tensor<fp32_t> &K, const Tensor<fp32_t> &dK, const Tensor<fp32_t> &V,
        const Tensor<fp32_t> &dV, const Tensor<bool_t> &mask,
        const Tensor<fp32_t> &maxsumexp, const Tensor<fp32_t> &dst_grad,
        const Tensor<fp32_t> &tmp, const Tensor<fp32_t> &tmp_grad,
        const Tensor<fp32_t> &tmp_sumprod_slice, int redux);

template
void flash_softmax_gemm_backward_async(const Tensor<fp32_fast_tf32_t> &Q, const Tensor<fp32_fast_tf32_t> &dQ,
        const Tensor<fp32_fast_tf32_t> &K, const Tensor<fp32_fast_tf32_t> &dK, const Tensor<fp32_fast_tf32_t> &V,
        const Tensor<fp32_fast_tf32_t> &dV, const Tensor<bool_t> &mask,
        const Tensor<fp32_fast_tf32_t> &maxsumexp, const Tensor<fp32_fast_tf32_t> &dst_grad,
        const Tensor<fp32_fast_tf32_t> &tmp, const Tensor<fp32_fast_tf32_t> &tmp_grad,
        const Tensor<fp32_fast_tf32_t> &tmp_sumprod_slice, int redux);

template
void flash_softmax_gemm_backward_async(const Tensor<fp32_fast_fp16_t> &Q, const Tensor<fp32_fast_fp16_t> &dQ,
        const Tensor<fp32_fast_fp16_t> &K, const Tensor<fp32_fast_fp16_t> &dK, const Tensor<fp32_fast_fp16_t> &V,
        const Tensor<fp32_fast_fp16_t> &dV, const Tensor<bool_t> &mask,
        const Tensor<fp32_fast_fp16_t> &maxsumexp, const Tensor<fp32_fast_fp16_t> &dst_grad,
        const Tensor<fp32_fast_fp16_t> &tmp, const Tensor<fp32_fast_fp16_t> &tmp_grad,
        const Tensor<fp32_fast_fp16_t> &tmp_sumprod_slice, int redux);

template
void flash_softmax_gemm_backward_async(const Tensor<fp64_t> &Q, const Tensor<fp64_t> &dQ,
        const Tensor<fp64_t> &K, const Tensor<fp64_t> &dK, const Tensor<fp64_t> &V,
        const Tensor<fp64_t> &dV, const Tensor<bool_t> &mask,
        const Tensor<fp64_t> &maxsumexp, const Tensor<fp64_t> &dst_grad,
        const Tensor<fp64_t> &tmp, const Tensor<fp64_t> &tmp_grad,
        const Tensor<fp64_t> &tmp_sumprod_slice, int redux);

template
void flash_softmax_gemm_backward_async(const Tensor<bf16_t> &Q, const Tensor<bf16_t> &dQ,
        const Tensor<bf16_t> &K, const Tensor<bf16_t> &dK, const Tensor<bf16_t> &V,
        const Tensor<bf16_t> &dV, const Tensor<bool_t> &mask,
        const Tensor<bf16_t> &maxsumexp, const Tensor<bf16_t> &dst_grad,
        const Tensor<bf16_t> &tmp, const Tensor<bf16_t> &tmp_grad,
        const Tensor<bf16_t> &tmp_sumprod_slice, int redux);

// Explicit instantiation
template
void flash_softmax_gemm_backward(const Tensor<fp32_t> &Q, const Tensor<fp32_t> &dQ,
        const Tensor<fp32_t> &K, const Tensor<fp32_t> &dK, const Tensor<fp32_t> &V,
        const Tensor<fp32_t> &dV, const Tensor<bool_t> &mask,
        const Tensor<fp32_t> &maxsumexp, const Tensor<fp32_t> &dst_grad,
        const Tensor<fp32_t> &tmp, const Tensor<fp32_t> &tmp_grad,
        const Tensor<fp32_t> &tmp_sumprod_slice, int redux);

template
void flash_softmax_gemm_backward(const Tensor<fp32_fast_tf32_t> &Q, const Tensor<fp32_fast_tf32_t> &dQ,
        const Tensor<fp32_fast_tf32_t> &K, const Tensor<fp32_fast_tf32_t> &dK, const Tensor<fp32_fast_tf32_t> &V,
        const Tensor<fp32_fast_tf32_t> &dV, const Tensor<bool_t> &mask,
        const Tensor<fp32_fast_tf32_t> &maxsumexp, const Tensor<fp32_fast_tf32_t> &dst_grad,
        const Tensor<fp32_fast_tf32_t> &tmp, const Tensor<fp32_fast_tf32_t> &tmp_grad,
        const Tensor<fp32_fast_tf32_t> &tmp_sumprod_slice, int redux);

template
void flash_softmax_gemm_backward(const Tensor<fp32_fast_fp16_t> &Q, const Tensor<fp32_fast_fp16_t> &dQ,
        const Tensor<fp32_fast_fp16_t> &K, const Tensor<fp32_fast_fp16_t> &dK, const Tensor<fp32_fast_fp16_t> &V,
        const Tensor<fp32_fast_fp16_t> &dV, const Tensor<bool_t> &mask,
        const Tensor<fp32_fast_fp16_t> &maxsumexp, const Tensor<fp32_fast_fp16_t> &dst_grad,
        const Tensor<fp32_fast_fp16_t> &tmp, const Tensor<fp32_fast_fp16_t> &tmp_grad,
        const Tensor<fp32_fast_fp16_t> &tmp_sumprod_slice, int redux);

template
void flash_softmax_gemm_backward(const Tensor<fp64_t> &Q, const Tensor<fp64_t> &dQ,
        const Tensor<fp64_t> &K, const Tensor<fp64_t> &dK, const Tensor<fp64_t> &V,
        const Tensor<fp64_t> &dV, const Tensor<bool_t> &mask,
        const Tensor<fp64_t> &maxsumexp, const Tensor<fp64_t> &dst_grad,
        const Tensor<fp64_t> &tmp, const Tensor<fp64_t> &tmp_grad,
        const Tensor<fp64_t> &tmp_sumprod_slice, int redux);

template
void flash_softmax_gemm_backward(const Tensor<bf16_t> &Q, const Tensor<bf16_t> &dQ,
        const Tensor<bf16_t> &K, const Tensor<bf16_t> &dK, const Tensor<bf16_t> &V,
        const Tensor<bf16_t> &dV, const Tensor<bool_t> &mask,
        const Tensor<bf16_t> &maxsumexp, const Tensor<bf16_t> &dst_grad,
        const Tensor<bf16_t> &tmp, const Tensor<bf16_t> &tmp_grad,
        const Tensor<bf16_t> &tmp_sumprod_slice, int redux);

} // namespace nntile::tensor
