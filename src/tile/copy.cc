/*! @copyright (c) 2022-2022 Skolkovo Institute of Science and Technology
 *                           (Skoltech). All rights reserved.
 *
 * NNTile is software framework for fast training of big neural networks on
 * distributed-memory heterogeneous systems based on StarPU runtime system.
 *
 * @file src/tile/copy.cc
 * Copy operation for Tile<T>
 *
 * @version 1.0.0
 * @author Aleksandr Mikhalev
 * @date 2022-04-22
 * */

#include "nntile/tile/copy.hh"

namespace nntile
{

template<typename T>
static
void cpu_copy_intersection_ndim0(void *buffers[], void *cl_args)
    noexcept
{
    const T *src = reinterpret_cast<T *>(STARPU_VARIABLE_GET_PTR(buffers[0]));
    T *dst = reinterpret_cast<T *>(STARPU_VARIABLE_GET_PTR(buffers[1]));
    *dst = *src;
}

template<typename T>
void copy_intersection_work_ndim0(const Tile<T> &src, const Tile<T> &dst)
{
    static struct starpu_codelet codelet_copy_ndim0 =
    {
        .cpu_funcs = {cpu_copy_intersection_ndim0<T>},
        .nbuffers = 2,
        .modes = {STARPU_R, STARPU_W}
    };
    constexpr double zero_flops = 0;
    int ret = starpu_task_insert(&codelet_copy_ndim0,
            STARPU_R, static_cast<starpu_data_handle_t>(src),
            STARPU_W, static_cast<starpu_data_handle_t>(dst),
            STARPU_FLOPS, zero_flops, // No floating point operations
            0);
    if(ret != 0)
    {
        throw std::runtime_error("ret != 0");
    }
}

template
void copy_intersection_work_ndim0(const Tile<fp32_t> &src,
        const Tile<fp32_t> &dst);

template
void copy_intersection_work_ndim0(const Tile<fp64_t> &src,
        const Tile<fp64_t> &dst);

template<typename T>
static
void cpu_copy_intersection(void *buffers[], void *cl_args)
    noexcept
{
    const Index *ndim_ptr, *src_start, *src_stride, *copy_shape, *dst_start,
          *dst_stride;
    // Read arguments
    Starpu::unpack_args_ptr(cl_args, ndim_ptr, src_start, src_stride,
            copy_shape, dst_start, dst_stride);
    Index ndim = *ndim_ptr;
    const T *src = reinterpret_cast<T *>(STARPU_VARIABLE_GET_PTR(buffers[0]));
    T *dst = reinterpret_cast<T *>(STARPU_VARIABLE_GET_PTR(buffers[1]));
    Index *src_index = reinterpret_cast<Index *>(
            STARPU_VARIABLE_GET_PTR(buffers[2]));
    Index *dst_index = src_index + ndim;
    Index nelems = 1;
    for(Index i = 0; i < ndim; ++i)
    {
        nelems *= copy_shape[i];
        src_index[i] = src_start[i];
        dst_index[i] = dst_start[i];
    }
    Index src_offset = src_start[0]; // src_stride[0] = 1
    Index dst_offset = dst_start[0]; // src_stride[0] = 1
    for(Index i = 1; i < ndim; ++i)
    {
        src_offset += src_start[i] * src_stride[i];
        dst_offset += dst_start[i] * dst_stride[i];
    }
    dst[dst_offset] = src[src_offset];
    ++src_offset;
    ++dst_offset;
    for(Index i = 1; i < nelems; ++i)
    {
        ++src_index[0];
        ++dst_index[0];
        Index j = 0;
        while(src_index[j] == src_start[j]+copy_shape[j])
        {
            src_index[j] = src_start[j];
            ++j;
            ++src_index[j];
            src_offset += src_stride[j] - copy_shape[j-1]*src_stride[j-1];
        }
        j = 0;
        while(dst_index[j] == dst_start[j]+copy_shape[j])
        {
            dst_index[j] = dst_start[j];
            ++j;
            ++dst_index[j];
            dst_offset += dst_stride[j] - copy_shape[j-1]*dst_stride[j-1];
        }
        dst[dst_offset] = src[src_offset];
        ++src_offset;
        ++dst_offset;
    }
}

template<typename T>
void copy_intersection_work(const Tile<T> &src,
        const std::vector<Index> &src_start, const Tile<T> &dst,
        const std::vector<Index> &dst_start,
        const std::vector<Index> &copy_shape,
        const StarpuVariableHandle &scratch,
        enum starpu_data_access_mode mode)
{
    static struct starpu_codelet codelet_copy_w =
    {
        .cpu_funcs = {cpu_copy_intersection<T>},
        .nbuffers = 3,
        .modes = {STARPU_R, STARPU_W, STARPU_SCRATCH}
    };
    static struct starpu_codelet codelet_copy_rw =
    {
        .cpu_funcs = {cpu_copy_intersection<T>},
        .nbuffers = 3,
        .modes = {STARPU_R, STARPU_RW, STARPU_SCRATCH}
    };
    static struct starpu_codelet codelet_copy_rw_commute =
    {
        .cpu_funcs = {cpu_copy_intersection<T>},
        .nbuffers = 3,
        .modes = {STARPU_R, Starpu::STARPU_RW_COMMUTE, STARPU_SCRATCH}
    };
    struct starpu_codelet *current_codelet;
    switch(mode)
    {
        case STARPU_W:
            current_codelet = &codelet_copy_w;
            break;
        case STARPU_RW:
            current_codelet = &codelet_copy_rw;
            break;
        case Starpu::STARPU_RW_COMMUTE:
            current_codelet = &codelet_copy_rw_commute;
            break;
        default:
            throw std::runtime_error("Unsupported mode");
    }
    // Launch codelet
    constexpr double zero_flops = 0;
    Index ndim = src.ndim;
    int ret = starpu_task_insert(current_codelet,
            STARPU_VALUE, &(ndim), sizeof(ndim),
            STARPU_VALUE, &(src_start[0]), ndim*sizeof(src_start[0]),
            STARPU_VALUE, &(src.stride[0]), ndim*sizeof(src.stride[0]),
            STARPU_VALUE, &(copy_shape[0]), ndim*sizeof(copy_shape[0]),
            STARPU_VALUE, &(dst_start[0]), ndim*sizeof(dst_start[0]),
            STARPU_VALUE, &(dst.stride[0]), ndim*sizeof(dst.stride[0]),
            STARPU_R, static_cast<starpu_data_handle_t>(src),
            mode, static_cast<starpu_data_handle_t>(dst),
            STARPU_SCRATCH, static_cast<starpu_data_handle_t>(scratch),
            STARPU_FLOPS, zero_flops, // No floating point operations
            0);
    if(ret != 0)
    {
        throw std::runtime_error("ret != 0");
    }
}

template
void copy_intersection_work(const Tile<fp32_t> &src,
        const std::vector<Index> &src_start, const Tile<fp32_t> &dst,
        const std::vector<Index> &dst_start,
        const std::vector<Index> &copy_shape,
        const StarpuVariableHandle &scratch,
        enum starpu_data_access_mode mode);

template
void copy_intersection_work(const Tile<fp64_t> &src,
        const std::vector<Index> &src_start, const Tile<fp64_t> &dst,
        const std::vector<Index> &dst_start,
        const std::vector<Index> &copy_shape,
        const StarpuVariableHandle &scratch,
        enum starpu_data_access_mode mode);

template<typename T>
void copy_intersection_work(const Tile<T> &src,
        const std::vector<Index> &src_offset, const Tile<T> &dst,
        const std::vector<Index> &dst_offset,
        const StarpuVariableHandle &scratch)
{
    // Treat non-zero ndim
    Index ndim = src.ndim;
    // Check if can simply call starpu_data_cpy
    if(src_offset == dst_offset and src.shape == dst.shape)
    {
        starpu_data_cpy(dst, src, 1, nullptr, nullptr);
        return;
    }
    // Otherwise we need to perform smart copy
    std::vector<Index> src_start(ndim), dst_start(ndim), copy_shape(ndim);
    enum starpu_data_access_mode dst_tile_mode = STARPU_W;
    // Obtain starting indices and shape of intersection for copying
    for(Index i = 0; i < ndim; ++i)
    {
        // Do nothing if tiles do not intersect
        if((src_offset[i]+src.shape[i] <= dst_offset[i])
                or (dst_offset[i]+dst.shape[i] <= src_offset[i]))
        {
            return;
        }
        // Copy to the beginning of destination
        if(src_offset[i] < dst_offset[i])
        {
            dst_start[i] = 0;
            src_start[i] = dst_offset[i] - src_offset[i];
            copy_shape[i] = std::min(src.shape[i]-src_start[i],
                    dst.shape[i]);
        }
        // Copy from the beginning of source
        else
        {
            dst_start[i] = src_offset[i] - dst_offset[i];
            src_start[i] = 0;
            copy_shape[i] = std::min(dst.shape[i]-dst_start[i],
                    src.shape[i]);
        }
        // Check if destination is fully inside source
        if(copy_shape[i] != dst.shape[i])
        {
            dst_tile_mode = STARPU_RW;
        }
    }
    // Launch codelet
    copy_intersection_work(src, src_start, dst, dst_start, copy_shape,
            scratch, dst_tile_mode);
}

template
void copy_intersection_work(const Tile<fp32_t> &src,
        const std::vector<Index> &src_offset, const Tile<fp32_t> &dst,
        const std::vector<Index> &dst_offset,
        const StarpuVariableHandle &scratch);

template
void copy_intersection_work(const Tile<fp64_t> &src,
        const std::vector<Index> &src_offset, const Tile<fp64_t> &dst,
        const std::vector<Index> &dst_offset,
        const StarpuVariableHandle &scratch);

} // namespace nntile

