/*! @copyright (c) 2022-2022 Skolkovo Institute of Science and Technology
 *                           (Skoltech). All rights reserved.
 *
 * NNTile is software framework for fast training of big neural networks on
 * distributed-memory heterogeneous systems based on StarPU runtime system.
 *
 * @file src/kernel/cpu/copy.cc
 * Smart copy operation on CPU
 *
 * @version 1.0.0
 * @author Aleksandr Mikhalev
 * @date 2022-08-02
 * */

#include "nntile/kernel/cpu/copy.hh"
#include "nntile/starpu.hh"

namespace nntile
{

//! Smart copying of one buffer into another
//
// @param[in] ndim: Dimensionality of underlying buffers
// @param[in] src_start: Start element to copy from source buffer
// @param[in] src_stride: Strides of the source buffer
// @param[in] copy_shape: Shape of buffer to copy
// @param[in] src: Pointer to input data
// @param[in] dst_start: Start element to copy to destination buffer
// @param[in] dst_stride: Strides of the destination buffer
// @param[inout] dst: Pointer to output data
// @param[out] tmp_index: Temporary buffer for indexing
template<typename T>
void copy_kernel_cpu(Index ndim, const Index *src_start,
        const Index *src_stride, const Index *copy_shape, const T *src,
        const Index *dst_start, const Index *dst_stride, T *dst,
        Index *tmp_index)
    noexcept
{
    // Map temporary buffer into source index and destination index
    Index *src_index = tmp_index;
    Index *dst_index = tmp_index + ndim;
    // Get number of elements to copy and init source and target indexes for
    // the first element to copy
    Index nelems = 1;
    for(Index i = 0; i < ndim; ++i)
    {
        nelems *= copy_shape[i];
        src_index[i] = src_start[i];
        dst_index[i] = dst_start[i];
    }
    // Get offsets for both source and target elements
    Index src_offset = src_start[0]; // src_stride[0] = 1
    Index dst_offset = dst_start[0]; // src_stride[0] = 1
    for(Index i = 1; i < ndim; ++i)
    {
        src_offset += src_start[i] * src_stride[i];
        dst_offset += dst_start[i] * dst_stride[i];
    }
    // Copy source into destination
    dst[dst_offset] = src[src_offset];
    // Get source and target offsets for the next element to copy
    ++src_offset;
    ++dst_offset;
    for(Index i = 1; i < nelems; ++i)
    {
        // Update indexes of source and target positions
        ++src_index[0];
        ++dst_index[0];
        // Get index and offset of the next source
        Index j = 0;
        while(src_index[j] == src_start[j]+copy_shape[j])
        {
            src_index[j] = src_start[j];
            ++j;
            ++src_index[j];
            src_offset += src_stride[j] - copy_shape[j-1]*src_stride[j-1];
        }
        // Get index and offset of the next target
        j = 0;
        while(dst_index[j] == dst_start[j]+copy_shape[j])
        {
            dst_index[j] = dst_start[j];
            ++j;
            ++dst_index[j];
            dst_offset += dst_stride[j] - copy_shape[j-1]*dst_stride[j-1];
        }
        // Copy source into destination
        dst[dst_offset] = src[src_offset];
        // Update offsets for the next copy
        ++src_offset;
        ++dst_offset;
    }
}

//! Smart copying through StarPU buffers
template<typename T>
void copy_starpu_cpu(void *buffers[], void *cl_args)
    noexcept
{
    // Get arguments
    const Index *ndim_ptr, *src_start, *src_stride, *copy_shape, *dst_start,
          *dst_stride;
    Starpu::unpack_args_ptr(cl_args, ndim_ptr, src_start, src_stride,
            copy_shape, dst_start, dst_stride);
    Index ndim = *ndim_ptr;
    // Get interfaces
    auto interfaces = reinterpret_cast<StarpuVariableInterface **>(buffers);
    // Launch kernel
    const T *src = interfaces[0]->get_ptr<T>();
    T *dst = interfaces[1]->get_ptr<T>();
    Index *tmp_index = interfaces[2]->get_ptr<Index>();
    copy_kernel_cpu<T>(ndim, src_start, src_stride, copy_shape, src, dst_start,
            dst_stride, dst, tmp_index);
}

// Explicit instantiation
template
void copy_starpu_cpu<fp32_t>(void *buffers[], void *cl_args)
    noexcept;

template
void copy_starpu_cpu<fp64_t>(void *buffers[], void *cl_args)
    noexcept;

} // namespace nntile

