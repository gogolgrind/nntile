/*! @copyright (c) 2022-2022 Skolkovo Institute of Science and Technology
 *                           (Skoltech). All rights reserved.
 *
 * NNTile is software framework for fast training of big neural networks on
 * distributed-memory heterogeneous systems based on StarPU runtime system.
 *
 * @file include/nntile/kernel/cpu/copy.hh
 * Smart copy operation
 *
 * @version 1.0.0
 * @author Aleksandr Mikhalev
 * @date 2022-04-22
 * */

#include <nntile/base_types.hh>

namespace nntile
{

//! Smart copying
template<typename T>
void copy_kernel_cpu(Index ndim, const Index *src_start,
        const Index *src_stride, const Index *copy_shape, const T *src,
        const Index *dst_start, const Index *dst_stride, T *dst,
        Index *tmp_index)
    noexcept;

// Smart copying through StarPU buffers
template<typename T>
void copy_starpu_cpu(void *buffers[], void *cl_args)
    noexcept;

} // namespace nntile
