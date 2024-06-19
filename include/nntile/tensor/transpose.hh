/*! @copyright (c) 2022-present Skolkovo Institute of Science and Technology
 *                              (Skoltech), Russia. All rights reserved.
 *                 2023-present Artificial Intelligence Research Institute
 *                              (AIRI), Russia. All rights reserved.
 *
 * NNTile is software framework for fast training of big neural networks on
 * distributed-memory heterogeneous systems based on StarPU runtime system.
 *
 * @file include/nntile/tensor/transpose.hh
 * Transpose operation for Tensor<T>
 *
 * @version 1.0.0
 * */

#pragma once

#include <nntile/tensor/tensor.hh>

namespace nntile::tensor
{

// Tensor-wise transpose operation
template<typename T>
void transpose_async(T alpha, const Tensor<T> &src, const Tensor<T> &dst,
        Index ndim);

// Tensor-wise transpose operation
template<typename T>
void transpose(T alpha, const Tensor<T> &src, const Tensor<T> &dst,
        Index ndim);

} // namespace nntile::tensor

