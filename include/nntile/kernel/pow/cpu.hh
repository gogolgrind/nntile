/*! @copyright (c) 2022-present Skolkovo Institute of Science and Technology
 *                              (Skoltech), Russia. All rights reserved.
 *                 2023-present Artificial Intelligence Research Institute
 *                              (AIRI), Russia. All rights reserved.
 *
 * NNTile is software framework for fast training of big neural networks on
 * distributed-memory heterogeneous systems based on StarPU runtime system.
 *
 * @file include/nntile/kernel/pow/cpu.hh
 * Power operation on CPU
 *
 * @version 1.0.0
 * */

#pragma once

#include <nntile/base_types.hh>

namespace nntile::kernel::pow
{

// Power operation on a CPU buffer
template<typename T>
void cpu(Index nelems, T alpha, T exp, T *data)
    noexcept;

} // namespace nntile::kernel::pow

