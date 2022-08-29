/*! @copyright (c) 2022-2022 Skolkovo Institute of Science and Technology
 *                           (Skoltech). All rights reserved.
 *
 * NNTile is software framework for fast training of big neural networks on
 * distributed-memory heterogeneous systems based on StarPU runtime system.
 *
 * @file src/kernel/cpu/gelutanh.cc
 * Approximate GeLU operation on CPU based on tanh function
 *
 * @version 1.0.0
 * @author Aleksandr Mikhalev
 * @date 2022-08-23
 * */

#include "nntile/kernel/cpu/gelutanh.hh"
#include <cmath>

namespace nntile
{
namespace kernel
{
namespace cpu
{

template<typename T>
void gelutanh(Index nelems, T *data)
    noexcept
//! Approximate GeLU operation
/*! Applies the following approximation of the GeLU function:
 * GeLU(z) \approx 0.5z(1+tanh(sqrt(2/pi)(z+0.044715z^3))),
 * which is actually implemented as
 * GeLU(z) \approx z/(1+exp(-2sqrt(2/pi)z(1+0.044715z^2)))
 *
 * @params[in] nelems: Number of elements in a buffer
 * @params[inout] data: Buffer to apply GeLU
 * */
{
    // Constants
    constexpr T pi = 3.141592653589793238462643383279502884L,
        one = 1, pt5 = 0.5, f1 = T{0.044715};
    // Square root is not constexpr by standard, proceed with a static const
    static const T sqrt_pi = std::sqrt(pi), sqrt_2 = std::sqrt(T{2}),
        f2 = sqrt_2/sqrt_pi, f3 = -T{2}*f2, f4 = f3*f1;
    for(Index i = 0; i < nelems; ++i)
    {
        T z = data[i];
        T y = z * (f3 + f4*z*z);
        data[i] = z / (one+std::exp(y));
    }
}

// Explicit instantiation
template
void gelutanh<fp32_t>(Index nelems, fp32_t *data)
    noexcept;

template
void gelutanh<fp64_t>(Index nelems, fp64_t *data)
    noexcept;

} // namespace cpu
} // namespace kernel
} // namespace nntile
