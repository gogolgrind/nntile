/*! @copyright (c) 2022-present Skolkovo Institute of Science and Technology
 *                              (Skoltech), Russia. All rights reserved.
 *                 2023-present Artificial Intelligence Research Institute
 *                              (AIRI), Russia. All rights reserved.
 *
 * NNTile is software framework for fast training of big neural networks on
 * distributed-memory heterogeneous systems based on StarPU runtime system.
 *
 * @file tests/tile/dgelutanh.cc
 * Derivative of approximate GeLU operation on Tile<T>
 *
 * @version 1.0.0
 * */

#include "nntile/tile/dgelutanh.hh"
#include "nntile/starpu/dgelutanh.hh"
#include "../testing.hh"

using namespace nntile;
using namespace nntile::tile;

template<typename T>
void validate()
{
    Tile<T> tile1({}), tile1_copy({}), tile2({2, 3, 4}), tile2_copy({2, 3, 4});
    auto tile1_local = tile1.acquire(STARPU_W);
    tile1_local[0] = T{-1};
    tile1_local.release();
    auto tile1_copy_local = tile1_copy.acquire(STARPU_W);
    tile1_copy_local[0] = T{-1};
    tile1_copy_local.release();
    auto tile2_local = tile2.acquire(STARPU_W);
    auto tile2_copy_local = tile2_copy.acquire(STARPU_W);
    for(Index i = 0; i < tile2.nelems; ++i)
    {
        tile2_local[i] = T(i+1);
        tile2_copy_local[i] = T(i+1);
    }
    tile2_local.release();
    tile2_copy_local.release();
    starpu::dgelutanh::submit<T>(1, tile1);
    dgelutanh<T>(tile1_copy);
    tile1_local.acquire(STARPU_R);
    tile1_copy_local.acquire(STARPU_R);
    TEST_ASSERT(tile1_local[0] == tile1_copy_local[0]);
    tile1_local.release();
    tile1_copy_local.release();
    starpu::dgelutanh::submit<T>(tile2.nelems, tile2);
    dgelutanh<T>(tile2_copy);
    tile2_local.acquire(STARPU_R);
    tile2_copy_local.acquire(STARPU_R);
    for(Index i = 0; i < tile2.nelems; ++i)
    {
        TEST_ASSERT(tile2_local[i] == tile2_copy_local[i]);
    }
    tile2_local.release();
    tile2_copy_local.release();
}

int main(int argc, char **argv)
{
    // Init StarPU for testing on CPU only
    starpu::Config starpu(1, 0, 0);
    // Init codelet
    starpu::dgelutanh::init();
    starpu::dgelutanh::restrict_where(STARPU_CPU);
    // Launch all tests
    validate<fp32_t>();
    validate<fp64_t>();
    return 0;
}

