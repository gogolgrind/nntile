# @copyright (c) 2022-2023 Skolkovo Institute of Science and Technology
#                           (Skoltech). All rights reserved.
#
# NNTile is software framework for fast training of big neural networks on
# distributed-memory heterogeneous systems based on StarPU runtime system.
#
# @file wrappers/python/nntile/layer/linear.py
# Linear layer of NNTile Python package
#
# @version 1.0.0
# @author Aleksandr Mikhalev
# @date 2023-05-06

import nntile
from nntile.tensor import TensorTraits, Tensor, TensorOrNone, TensorMoments, \
        TransOp, trans, notrans, copy_async, gemm_async, gemm_ex_async, \
        randn_async
from nntile.layer.base_layer import BaseLayer
import numpy as np
from typing import List, Optional

class Linear(BaseLayer):
    side: str
    trans_x: TransOp
    x: TensorMoments
    y: TensorMoments
    w: TensorMoments
    ndim: int
    #b: TensorMoments
    #b_axis: int
    fp32_fast_fp16: bool
    fp32_convert_fp16: bool
    x_fp16: TensorMoments
    y_fp16: TensorMoments
    w_fp16: TensorMoments

    # Construct linear layer with all the provided data
    def __init__(self, side: str, trans_x: TransOp, x: TensorMoments, \
            y: TensorMoments, w: TensorMoments, ndim: int, \
            #b: TensorMoments, b_axis: int, # No bias as of now \
            fp32_fast_fp16: bool = False, \
            fp32_convert_fp16: bool = False, \
            x_fp16: Optional[TensorMoments], \
            w_fp16: Optional[TensorMoments], \
            y_fp16: Optional[TensorMoments]):
        # Check parameter side
        if side != 'L' and side != 'R':
            raise ValueError("side must be either 'L' or 'R'")
        # Check parameter ndim
        if ndim <= 0:
            raise ValueError("ndim must be positive integer")
        # Redirect to BaseClass initialization
        super().__init__([x], [y], [w], [x_fp16, w_fp16, y_fp16])
        # Set up local named parameters
        self.side = side
        self.trans_x = trans_x
        self.ndim = ndim
        self.x = x
        self.y = y
        self.w = w
        self.x_fp16 = x_fp16
        self.w_fp16 = w_fp16
        self.y_fp16 = y_fp16
        #self.b = b
        #self.b_axis = b_axis
        self.fp32_fast_fp16 = fp32_fast_fp16
        self.fp32_convert_fp16 = fp32_convert_fp16

    # Simple generator for the linear layer
    @staticmethod
    def generate_simple_mpiroot(x: TensorMoments, side: str, \
            trans_x: TransOp, ndim: int, add_shape: List[int], \
            add_basetile_shape: List[int], next_tag: int, \
            fp32_fast_fp16: bool = False, fp32_convert_fp16: bool = False):
        # Define shapes
        if side == 'L':
            if trans_x == notrans:
                w_shape = x.value.shape[-ndim:] + add_shape
                w_tile = x.value.basetile_shape[-ndim:] + add_basetile_shape
                y_shape = x.value.shape[:-ndim] + add_shape
                y_tile = x.value.basetile_shape[:-ndim] + add_basetile_shape
            else:
                w_shape = x.value.shape[:ndim] + add_shape
                w_tile = x.value.basetile_shape[:ndim] + add_basetile_shape
                y_shape = x.value.shape[ndim:] + add_shape
                y_tile = x.value.basetile_shape[ndim:] + add_basetile_shape
        else:
            if trans_x == notrans:
                w_shape = add_shape + x.value.shape[:ndim]
                w_tile = add_basetile_shape + x.value.basetile_shape[:ndim]
                y_shape = add_shape + x.value.shape[ndim:]
                y_tile = add_basetile_shape + x.value.basetile_shape[ndim:]
            else:
                w_shape = add_shape + x.value.shape[-ndim:]
                w_tile = add_basetile_shape + x.value.basetile_shape[-ndim:]
                y_shape = add_shape + x.value.shape[:-ndim]
                y_tile = add_basetile_shape + x.value.basetile_shape[:-ndim]
        # Define W
        w_traits = TensorTraits(w_shape, w_tile)
        # TODO change distribution
        w_distr = [0] * w_traits.grid.nelems
        w_value = type(x.value)(w_traits, w_distr, next_tag)
        next_tag = w_value.next_tag
        # Create gradient of W with the same traits and distribution as W
        w_grad = type(x.value)(w_traits, w_distr, next_tag)
        next_tag = w_grad.next_tag
        # Define W as TensorMoments
        w = TensorMoments(w_value, w_grad, True)
        # Define Y
        y_traits = TensorTraits(y_shape, y_tile)
        # TODO change distribution
        y_distr = [0] * y_traits.grid.nelems
        y_value = type(x.value)(y_traits, y_distr, next_tag)
        next_tag = y_value.next_tag
        # Create gradient of Y with the same traits and distribution as Y
        y_grad = type(x.value)(y_traits, y_distr, next_tag)
        next_tag = y_grad.next_tag
        # Define Y as TensorMoments
        y = TensorMoments(y_value, y_grad, True)
        # Bias is ignored for now
        #b = TensorMoments(None, None, False)
        # Create linear layer with all the provided data
        if type(x.value) is not nntile.tensor.Tensor_fp32:
            fp32_fast_fp16 = False
            fp32_convert_fp16 = False
        if fp32_fast_fp16:
            fp32_convert_fp16 = False
        if fp32_convert_fp16:
            x_fp16_value = Tensor_fp16(x_traits, x_distr, next_tag)
            next_tag = w_fp16_value.next_tag
            x_fp16_grad = Tensor_fp16(x_traits, x_distr, next_tag)
            next_tag = x_fp16_grad.next_tag
            x_fp16 = TensorMoments(x_fp16_value, x_fp16_grad, True)
            w_fp16_value = Tensor_fp16(w_traits, w_distr, next_tag)
            next_tag = w_fp16_value.next_tag
            w_fp16_grad = Tensor_fp16(w_traits, w_distr, next_tag)
            next_tag = w_fp16_grad.next_tag
            w_fp16 = TensorMoments(w_fp16_value, w_fp16_grad, True)
            y_fp16_value = Tensor_fp16(y_traits, y_distr, next_tag)
            next_tag = y_fp16_value.next_tag
            y_fp16_grad = Tensor_fp16(y_traits, y_distr, next_tag)
            next_tag = y_fp16_grad.next_tag
            y_fp16 = TensorMoments(y_fp16_value, y_fp16_grad, True)
            layer = Linear(side, trans_x, x, y, w, ndim, fp32_fast_fp16, \
                    fp32_convert_fp16, x_fp16, w_fp16, y_fp16)
        else:
            layer = Linear(side, trans_x, x, y, w, ndim, fp32_fast_fp16)
        # Return layer and next tag to be used
        return (layer, next_tag)

    # Forward propagation of the linear layer
    def forward_async(self):
        # Convert fp32 to fp16 if needed
        if self.fp32_convert_fp16:
            fp32_to_fp16_async(self.x.value, self.x_fp16.value)
            fp32_to_fp16_async(self.w.value, self.w_fp16.value)
        # Perform actual gemm
        if self.side == 'L':
            # Y = einsum('ij,jk->ik', op(X), W)
            # 'i' is a multi-index of dimension X.ndim-ndim
            # 'j' is a multi-index of dimension ndim
            # 'k' is a multi-index of dimension W.ndim-ndim
            if self.fp32_fast_fp16:
                gemm_ex_async(1.0, self.trans_x, self.x.value, notrans, \
                        self.w.value, 0.0, self.y.value, self.ndim, 0)
            elif self.fp32_convert_fp16:
                gemm_async(1.0, self.trans_x, self.x_fp16.value, notrans, \
                        self.w_fp16.value, 0.0, self.y_fp16.value, \
                        self.ndim, 0)
            else:
                gemm_async(1.0, self.trans_x, self.x.value, notrans, \
                        self.w.value, 0.0, self.y.value, self.ndim, 0)
        else:
            # Y = einsum('ij,jk->ik', W, op(X))
            # 'i' is a multi-index of dimension W.ndim-ndim
            # 'j' is a multi-index of dimension ndim
            # 'k' is a multi-index of dimension X.ndim-ndim
            if self.fp32_fast_fp16:
                gemm_ex_async(1.0, notrans, self.w.value, self.trans_x, \
                        self.x.value, 0.0, self.y.value, self.ndim, 0)
            elif self.fp32_convert_fp16:
                gemm_async(1.0, notrans, self.w_fp16.value, self.trans_x, \
                        self.x_fp16.value, 0.0, self.y_fp16.value, \
                        self.ndim, 0)
            else:
                gemm_async(1.0, notrans, self.w.value, self.trans_x, \
                        self.x.value, 0.0, self.y.value, self.ndim, 0)
        # Convert fp16 to fp32 if needed
        if self.fp32_convert_fp16:
            fp16_to_fp32(self.y_fp16.value, self.y.value)
        # Hint for StarPU that W tensor will
        # not be used soon and it is advised to offload data from GPU
        #self.w.value.wont_use()

    # Backward propagation of the linear layer
    def backward_async(self):
        # Convert fp32 to fp16 if needed
        if self.fp32_convert_fp16:
            fp32_to_fp16_async(self.y.grad, self.y_fp16.grad)
        # Gradient over W (weights)
        if self.w.grad_required:
            # Convert fp32 to fp16 if needed
            if self.fp32_convert_fp16:
                fp32_to_fp16_async(self.w.grad, self.w_fp16.grad)
            gemm_ndim = self.x.value.ndim - self.ndim
            if self.side == 'L':
                # Backward for Y = einsum('ij,jk->ik', op(X), W)
                # dW = einsum('ij,ik->jk', op(X), dY)
                # 'i' is a multi-index of dimension X.ndim-ndim
                # 'j' is a multi-index of dimension ndim
                # 'k' is a multi-index of dimension W.ndim-ndim
                if self.trans_x == notrans:
                    if self.fp32_fast_fp16:
                        gemm_ex_async(1.0, trans, self.x.value, notrans, \
                                self.y.grad, 1.0, self.w.grad, gemm_ndim, 0)
                    elif self.fp32_convert_fp16:
                        gemm_async(1.0, trans, self.x_fp16.value, notrans, \
                                self.y_fp16.grad, 1.0, self.w_fp16.grad, \
                                gemm_ndim, 0)
                    else:
                        gemm_async(1.0, trans, self.x.value, notrans, \
                                self.y.grad, 1.0, self.w.grad, gemm_ndim, 0)
                else:
                    if self.fp32_fast_fp16:
                        gemm_ex_async(1.0, notrans, self.x.value, notrans, \
                                self.y.grad, 1.0, self.w.grad, gemm_ndim, 0)
                    elif self.fp32_convert_fp16:
                        gemm_async(1.0, notrans, self.x_fp16.value, notrans, \
                                self.y_fp16.grad, 1.0, self.w_fp16.grad, \
                                gemm_ndim, 0)
                    else:
                        gemm_async(1.0, notrans, self.x.value, notrans, \
                                self.y.grad, 1.0, self.w.grad, gemm_ndim, 0)
            else:
                # Backward for Y = einsum('ij,jk->ik', W, op(X))
                # dW = einsum('ik,jk->ij', dY, op(X))
                # 'i' is a multi-index of dimension W.ndim-ndim
                # 'j' is a multi-index of dimension ndim
                # 'k' is a multi-index of dimension X.ndim-ndim
                if self.trans_x == notrans:
                    if self.fp32_fast_fp16:
                        gemm_ex_async(1.0, notrans, self.y.grad, trans, \
                                self.x.value, 1.0, self.w.grad, gemm_ndim, 0)
                    elif self.fp32_convert_fp16:
                        gemm_async(1.0, notrans, self.y_fp16.grad, trans, \
                                self.x_fp16.value, 1.0, self.w_fp16.grad, \
                                gemm_ndim, 0)
                    else:
                        gemm_async(1.0, notrans, self.y.grad, trans, \
                                self.x.value, 1.0, self.w.grad, gemm_ndim, 0)
                else:
                    if self.fp32_fast_fp16:
                        gemm_ex_async(1.0, notrans, self.y.grad, notrans, \
                                self.x.value, 1.0, self.w.grad, gemm_ndim, 0)
                    elif self.fp32_convert_fp16:
                        gemm_async(1.0, notrans, self.y_fp16.grad, notrans, \
                                self.x_fp16.value, 1.0, self.w_fp16.grad, \
                                gemm_ndim, 0)
                    else:
                        gemm_async(1.0, notrans, self.y.grad, notrans, \
                                self.x.value, 1.0, self.w.grad, gemm_ndim, 0)
            # Convert fp16 to fp32 if needed
            if self.fp32_convert_fp16:
                fp16_to_fp32_async(self.w_fp16.grad, self.w.grad)
            # Hint StarPU to offload gradient over W if needed
            #self.w.grad.wont_use()
        # Gradient over X (input)
        if self.x.grad_required:
            # Convert fp32 to fp16 if needed
            if self.fp32_convert_fp16:
                fp32_to_fp16_async(self.x.grad, self.x_fp16.grad)
            gemm_ndim = self.w.value.ndim - self.ndim
            if self.side == 'L':
                # Backward for Y = einsum('ij,jk->ik', op(X), W)
                # d op(X) = einsum('ik,jk->ij', dY, W)
                # 'i' is a multi-index of dimension X.ndim-ndim
                # 'j' is a multi-index of dimension ndim
                # 'k' is a multi-index of dimension W.ndim-ndim
                if self.trans_x == notrans:
                    # dX = einsum('ik,jk->ij', dY, W)
                    if self.fp32_fast_fp16:
                        gemm_ex_async(1.0, notrans, self.y.grad, trans, \
                                self.w.value, 1.0, self.x.grad, gemm_ndim, 0)
                    elif self.fp32_convert_fp16:
                        gemm_async(1.0, notrans, self.y_fp16.grad, trans, \
                                self.w_fp16.value, 1.0, self.x_fp16.grad, \
                                gemm_ndim, 0)
                    else:
                        gemm_async(1.0, notrans, self.y.grad, trans, \
                                self.w.value, 1.0, self.x.grad, gemm_ndim, 0)
                else:
                    # dX = einsum('ik,jk->ij', W, dY)
                    if self.fp32_fast_fp16:
                        gemm_ex_async(1.0, notrans, self.w.value, trans, \
                                self.y.grad, 1.0, self.x.grad, gemm_ndim, 0)
                    elif self.fp32_convert_fp16:
                        gemm_async(1.0, notrans, self.w_fp16.value, trans, \
                                self.y_fp16.grad, 1.0, self.x_fp16.grad, \
                                gemm_ndim, 0)
                    else:
                        gemm_async(1.0, notrans, self.w.value, trans, \
                                self.y.grad, 1.0, self.x.grad, gemm_ndim, 0)
            else:
                # Backward for Y = einsum('ij,jk->ik', W, op(X))
                # d op(X) = einsum('ij,ik->jk', W, dY)
                # 'i' is a multi-index of dimension W.ndim-ndim
                # 'j' is a multi-index of dimension ndim
                # 'k' is a multi-index of dimension X.ndim-ndim
                if self.trans_x == notrans:
                    # dX = einsum('ij,ik->jk', W, dY)
                    if self.fp32_fast_fp16:
                        gemm_ex_async(1.0, trans, self.w.value, notrans, \
                                self.y.grad, 1.0, self.x.grad, gemm_ndim, 0)
                    elif self.fp32_convert_fp16:
                        gemm_async(1.0, trans, self.w_fp16.value, notrans, \
                                self.y_fp16.grad, 1.0, self.x_fp16.grad, \
                                gemm_ndim, 0)
                    else:
                        gemm_async(1.0, trans, self.w.value, notrans, \
                                self.y.grad, 1.0, self.x.grad, gemm_ndim, 0)
                else:
                    # dX = einsum('ij,ik->jk', dY, W)
                    if self.fp32_fast_fp16:
                        gemm_ex_async(1.0, trans, self.y.grad, notrans, \
                                self.w.value, 1.0, self.x.grad, gemm_ndim, 0)
                    elif self.fp32_convert_fp16:
                        gemm_async(1.0, trans, self.y_fp16.grad, notrans, \
                                self.w_fp16.value, 1.0, self.x_fp16.grad, \
                                gemm_ndim, 0)
                    else:
                        gemm_async(1.0, trans, self.y.grad, notrans, \
                                self.w.value, 1.0, self.x.grad, gemm_ndim, 0)
            # Convert fp16 to fp32 if needed
            if self.fp32_convert_fp16:
                fp16_to_fp32_async(self.x_fp16.grad, self.x.grad)
            # Hint StarPU to offload certain buffers
            #self.w.value.wont_use()

