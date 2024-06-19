/*! @copyright (c) 2022-present Skolkovo Institute of Science and Technology
 *                              (Skoltech), Russia. All rights reserved.
 *                 2023-present Artificial Intelligence Research Institute
 *                              (AIRI), Russia. All rights reserved.
 *
 * NNTile is software framework for fast training of big neural networks on
 * distributed-memory heterogeneous systems based on StarPU runtime system.
 *
 * @file include/nntile/kernel/logsumexp/cpu.hh
 * Logsumexp of a buffer on CPU
 *
 * @version 1.0.0
 * */

#pragma once

#include <nntile/base_types.hh>

namespace nntile::kernel::logsumexp
{

// Compute logsumexp based on the resut of maxsumexp operation 
template<typename T>
void cpu(Index nelems, const T *maxsumexp, T *logsumexp)
    noexcept;

} // namespace nntile::kernel::logsumexp

