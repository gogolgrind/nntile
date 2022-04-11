#include <nntile.hh>
#include <limits>
using namespace nntile;

int main(int argc, char **argv)
{
    using T = float;
    StarPU starpu;
    {
    std::vector<int> shape({3, 2, 1, 10});
    TileTraits traits(shape);
    std::cout << traits;
    // Traits for different tiles to check operations
    TileTraits A1_traits({3, 2, 1, 10}),
               A1T_traits({10, 3, 2, 1}),
               B1_traits({10, 5, 6}),
               B1T_traits({5, 6, 10}),
               C1_traits({3, 2, 1, 5, 6}),
               C1T_traits({5, 6, 3, 2, 1}),
               A2_traits({3, 4, 5}),
               A2T_traits({4, 5, 3}),
               B2_traits({4, 5, 5, 6}),
               B2T_traits({5, 6, 4, 5}),
               C2_traits({3, 5, 6}),
               C2T_traits({5, 6, 3});
    // Allocate memory for tiles
    auto *A1_ptr = new T[A1_traits.nelems];
    auto *B1_ptr = new T[B1_traits.nelems];
    auto *C1_ptr = new T[C1_traits.nelems];
    auto *A2_ptr = new T[A2_traits.nelems];
    auto *B2_ptr = new T[B2_traits.nelems];
    auto *C2_ptr = new T[C2_traits.nelems];
    // Init StarPU data handles
    TileHandle A1_handle(STARPU_MAIN_RAM, A1_ptr, A1_traits.nelems),
                       B1_handle(STARPU_MAIN_RAM, B1_ptr, B1_traits.nelems),
                       C1_handle(STARPU_MAIN_RAM, C1_ptr, C1_traits.nelems),
                       A2_handle(STARPU_MAIN_RAM, A2_ptr, A2_traits.nelems),
                       B2_handle(STARPU_MAIN_RAM, B2_ptr, B2_traits.nelems),
                       C2_handle(STARPU_MAIN_RAM, C2_ptr, C2_traits.nelems);
    // High-level API for tiles
    Tile<T> A1(A1_traits, A1_handle),
        A1T(A1T_traits, A1_handle),
        B1(B1_traits, B1_handle),
        B1T(B1T_traits, B1_handle),
        C1(C1_traits, C1_handle),
        C1T(C1T_traits, C1_handle),
        A2(A2_traits, A2_handle),
        A2T(A2T_traits, A2_handle),
        B2(B2_traits, B2_handle),
        B2T(B2T_traits, B2_handle),
        C2(C2_traits, C2_handle),
        C2T(C2T_traits, C2_handle);
    T one = 1.0, zero = 0.0;
    // Check if tensors match gemm operation
    gemm_check(TransOp::NoTrans, A1, TransOp::NoTrans, B1, C1);
    gemm_check(TransOp::NoTrans, A1, TransOp::Trans, B1T, C1);
    gemm_check(TransOp::Trans, A1T, TransOp::NoTrans, B1, C1);
    gemm_check(TransOp::Trans, A1T, TransOp::Trans, B1T, C1);
    gemm_check(TransOp::NoTrans, B1T, TransOp::NoTrans, A1T, C1T);
    gemm_check(TransOp::NoTrans, B1T, TransOp::Trans, A1, C1T);
    gemm_check(TransOp::Trans, B1, TransOp::NoTrans, A1T, C1T);
    gemm_check(TransOp::Trans, B1, TransOp::Trans, A1, C1T);
    gemm_check(TransOp::NoTrans, A2, TransOp::NoTrans, B2, C2, 2);
    gemm_check(TransOp::NoTrans, A2, TransOp::Trans, B2T, C2, 2);
    gemm_check(TransOp::Trans, A2T, TransOp::NoTrans, B2, C2, 2);
    gemm_check(TransOp::Trans, A2T, TransOp::Trans, B2T, C2, 2);
    gemm_check(TransOp::NoTrans, B2T, TransOp::NoTrans, A2T, C2T, 2);
    gemm_check(TransOp::NoTrans, B2T, TransOp::Trans, A2, C2T, 2);
    gemm_check(TransOp::Trans, B2, TransOp::NoTrans, A2T, C2T, 2);
    gemm_check(TransOp::Trans, B2, TransOp::Trans, A2, C2T, 2);
    // Check gemm with alpha=one and beta=zero
    gemm(one, TransOp::NoTrans, A1, TransOp::NoTrans, B1, zero, C1, 1);
    gemm(one, TransOp::NoTrans, A1, TransOp::Trans, B1T, zero, C1, 1);
    gemm(one, TransOp::Trans, A1T, TransOp::NoTrans, B1, zero, C1, 1);
    gemm(one, TransOp::Trans, A1T, TransOp::Trans, B1T, zero, C1, 1);
    gemm(one, TransOp::NoTrans, B1T, TransOp::NoTrans, A1T, zero, C1T, 1);
    gemm(one, TransOp::NoTrans, B1T, TransOp::Trans, A1, zero, C1T, 1);
    gemm(one, TransOp::Trans, B1, TransOp::NoTrans, A1T, zero, C1T, 1);
    gemm(one, TransOp::Trans, B1, TransOp::Trans, A1, zero, C1T, 1);
    gemm(one, TransOp::NoTrans, A2, TransOp::NoTrans, B2, zero, C2, 2);
    gemm(one, TransOp::NoTrans, A2, TransOp::Trans, B2T, zero, C2, 2);
    gemm(one, TransOp::Trans, A2T, TransOp::NoTrans, B2, zero, C2, 2);
    gemm(one, TransOp::Trans, A2T, TransOp::Trans, B2T, zero, C2, 2);
    gemm(one, TransOp::NoTrans, B2T, TransOp::NoTrans, A2T, zero, C2T, 2);
    gemm(one, TransOp::NoTrans, B2T, TransOp::Trans, A2, zero, C2T, 2);
    gemm(one, TransOp::Trans, B2, TransOp::NoTrans, A2T, zero, C2T, 2);
    gemm(one, TransOp::Trans, B2, TransOp::Trans, A2, zero, C2T, 2);
    // Check gemm with alpha=one and beta=one
    gemm(one, TransOp::NoTrans, A1, TransOp::NoTrans, B1, one, C1, 1);
    gemm(one, TransOp::NoTrans, A1, TransOp::Trans, B1T, one, C1, 1);
    gemm(one, TransOp::Trans, A1T, TransOp::NoTrans, B1, one, C1, 1);
    gemm(one, TransOp::Trans, A1T, TransOp::Trans, B1T, one, C1, 1);
    gemm(one, TransOp::NoTrans, B1T, TransOp::NoTrans, A1T, one, C1T, 1);
    gemm(one, TransOp::NoTrans, B1T, TransOp::Trans, A1, one, C1T, 1);
    gemm(one, TransOp::Trans, B1, TransOp::NoTrans, A1T, one, C1T, 1);
    gemm(one, TransOp::Trans, B1, TransOp::Trans, A1, one, C1T, 1);
    gemm(one, TransOp::NoTrans, A2, TransOp::NoTrans, B2, one, C2, 2);
    gemm(one, TransOp::NoTrans, A2, TransOp::Trans, B2T, one, C2, 2);
    gemm(one, TransOp::Trans, A2T, TransOp::NoTrans, B2, one, C2, 2);
    gemm(one, TransOp::Trans, A2T, TransOp::Trans, B2T, one, C2, 2);
    gemm(one, TransOp::NoTrans, B2T, TransOp::NoTrans, A2T, one, C2T, 2);
    gemm(one, TransOp::NoTrans, B2T, TransOp::Trans, A2, one, C2T, 2);
    gemm(one, TransOp::Trans, B2, TransOp::NoTrans, A2T, one, C2T, 2);
    gemm(one, TransOp::Trans, B2, TransOp::Trans, A2, one, C2T, 2);
    // Check GELU
    gelu(C1);
    gelu(C2);
    // Traits and memory for bias tiles
    TileTraits A1_bias_traits[4] = {{{2, 1, 10}}, {{3, 1, 10}}, {{3, 2, 10}},
        {{3, 2, 1}}};
    T *A1_bias_ptr[4] = {{new T[A1_bias_traits[0].nelems]},
        {new T[A1_bias_traits[1].nelems]},
        {new T[A1_bias_traits[2].nelems]},
        {new T[A1_bias_traits[3].nelems]}};
    // Init StarPU handles for biases
    TileHandle A1_bias_handle[4] = {
        {STARPU_MAIN_RAM, A1_bias_ptr[0], A1_bias_traits[0].nelems},
        {STARPU_MAIN_RAM, A1_bias_ptr[1], A1_bias_traits[1].nelems},
        {STARPU_MAIN_RAM, A1_bias_ptr[2], A1_bias_traits[2].nelems},
        {STARPU_MAIN_RAM, A1_bias_ptr[3], A1_bias_traits[3].nelems}};
    // High-level API for bias
    Tile<T> A1_bias[4] = {{A1_bias_traits[0], A1_bias_handle[0]},
        {A1_bias_traits[1], A1_bias_handle[1]},
        {A1_bias_traits[2], A1_bias_handle[2]},
        {A1_bias_traits[3], A1_bias_handle[3]}};
    // Check bias code
    for(int i = 0; i < 4; ++i)
    {
        bias(A1, A1_bias[i], i);
    }
    // Free memory
    starpu_task_wait_for_all();
    delete[] A1_ptr;
    delete[] B1_ptr;
    delete[] C1_ptr;
    delete[] A2_ptr;
    delete[] B2_ptr;
    delete[] C2_ptr;
    for(int i = 0; i < 4; ++i)
    {
        delete[] A1_bias_ptr[i];
    }
    }
    return 0;
}
