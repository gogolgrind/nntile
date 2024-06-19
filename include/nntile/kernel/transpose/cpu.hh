/*! @copyright (c) 2022-present Skolkovo Institute of Science and Technology
 *                              (Skoltech), Russia. All rights reserved.
 *                 2023-present Artificial Intelligence Research Institute
 *                              (AIRI), Russia. All rights reserved.
 *
 * NNTile is software framework for fast training of big neural networks on
 * distributed-memory heterogeneous systems based on StarPU runtime system.
 *
 * @file include/nntile/kernel/transpose/cpu.hh
 * Transpose operation on buffers on CPU
 *
 * @version 1.0.0
 * */

#pragma once

#include <nntile/base_types.hh>

namespace nntile::kernel::transpose
{

// Apply transpose for buffers on CPU
template<typename T>
void cpu(Index m, Index n, T alpha, const T* src, T* dst)
    noexcept;

} // namespace nntile::kernel::transpose

