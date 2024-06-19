/*! @copyright (c) 2022-present Skolkovo Institute of Science and Technology
 *                              (Skoltech), Russia. All rights reserved.
 *                 2023-present Artificial Intelligence Research Institute
 *                              (AIRI), Russia. All rights reserved.
 *
 * NNTile is software framework for fast training of big neural networks on
 * distributed-memory heterogeneous systems based on StarPU runtime system.
 *
 * @file include/nntile/kernel/sumnorm.hh
 * Low-level kernels to compute sum and Euclidean norm along axis
 *
 * @version 1.0.0
 * */

#pragma once

#include <nntile/kernel/sumnorm/cpu.hh>
#include <nntile/defs.h>
#ifdef NNTILE_USE_CUDA
#include <nntile/kernel/sumnorm/cuda.hh>
#endif // NNTILE_USE_CUDA

//! @namespace nntile::kernel::sumnorm
/*! Low-level implementations of computing sum and norm operation
 * */
namespace nntile::kernel::sumnorm
{

} // namespace nntile::kernel::sumnorm

