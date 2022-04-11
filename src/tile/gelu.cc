#include "nntile/tile/gelu.hh"
#include <cmath>

namespace nntile
{

template<typename T>
static void cpu_gelu(void *buffers[], void *cl_args)
{
    size_t nelems;
    starpu_codelet_unpack_args(cl_args, &nelems);
    T *data = reinterpret_cast<T *>(STARPU_VARIABLE_GET_PTR(buffers[0]));
    constexpr T sqrt2 = std::sqrt(T{2.0});
    constexpr T one = 1.0;
    constexpr T pt5 = 0.5;
    for(size_t i = 0; i < nelems; ++i)
    {
        T tmp = pt5*(std::erf(data[i]/sqrt2)) + pt5;
        data[i] *= tmp;
    }
}

template<typename T>
void gelu_async(const Tile<T> &A)
{
    static struct starpu_codelet codelet_gelu =
    {
        .cpu_funcs = {cpu_gelu<T>},
        .nbuffers = 1,
        .modes = {STARPU_RW}
    };
    size_t nelems = A.nelems;
    starpu_task_insert(&codelet_gelu,
            STARPU_VALUE, &nelems, sizeof(nelems),
            STARPU_RW, static_cast<starpu_data_handle_t>(A),
            0);
}

template
void gelu_async(const Tile<float> &A);

template
void gelu_async(const Tile<double> &A);

} // namespace nntile
