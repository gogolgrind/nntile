# @copyright (c) 2022-2023 Skolkovo Institute of Science and Technology
#                           (Skoltech). All rights reserved.
#
# NNTile is software framework for fast training of big neural networks on
# distributed-memory heterogeneous systems based on StarPU runtime system.
#
# Example for torch version of MLP-Mixer model
#
# @version 1.0.0
# @author Gleb Karpov
# @date 2023-09-13

# All necesary imports
import numpy as np
import torch
import torch.nn as nn
import torchvision.datasets as dts 
import torchvision.transforms as trnsfrms
import nntile
from nntile.torch_models.mlp_mixer import MlpMixer, image_patching
from nntile.model.mlp_mixer import MlpMixer as MlpMixerTile


# Set up StarPU configuration and init it
config = nntile.starpu.Config(1, 0, 0)
# Init all NNTile-StarPU codelets
nntile.starpu.init()

init_patch_size = 7
batch_size = 1
channel_size = int(28 * 28 / init_patch_size ** 2)
hidden_dim = 100
num_mixer_layers = 10
num_classes = 10

lr = 1e-2
next_tag = 0

stop_train_iter = 1000

X_shape = [channel_size, batch_size, init_patch_size ** 2]

mlp_mixer_model = MlpMixer(channel_size, init_patch_size ** 2, hidden_dim, num_mixer_layers, num_classes)
optim_torch = torch.optim.SGD(mlp_mixer_model.parameters(), lr=lr)
crit_torch = nn.CrossEntropyLoss(reduction="sum")

trnsform = trnsfrms.Compose([trnsfrms.ToTensor()])

mnisttrainset = dts.MNIST(root='./data', train=True, download=True, transform=trnsform)
trainldr = torch.utils.data.DataLoader(mnisttrainset, batch_size=batch_size, shuffle=True)

training_iteration = 0
for train_batch_sample, true_labels in trainldr:
    train_batch_sample = train_batch_sample.view(-1, 28, 28)
    train_batch_sample = torch.swapaxes(train_batch_sample, 0, 1)
    patched_train_sample = image_patching(train_batch_sample, init_patch_size)
    
    mlp_mixer_model.zero_grad()
    torch_output = mlp_mixer_model(patched_train_sample)
    torch_loss = crit_torch(torch_output, true_labels)
    torch_loss.backward()
    if (training_iteration % 100) == 99:
        print("Intermediate PyTorch loss =", torch_loss.item())
    optim_torch.step()
    if training_iteration == stop_train_iter:
        break
    training_iteration += 1

nntile_mixer_model, next_tag = MlpMixerTile.from_torch(mlp_mixer_model,1,num_classes, next_tag)

data_train_traits = nntile.tensor.TensorTraits(X_shape, X_shape)
data_train_tensor = nntile.tensor.Tensor_fp32(data_train_traits, [0], next_tag)
next_tag = data_train_tensor.next_tag
data_train_tensor.from_array(patched_train_sample)

nntile.tensor.copy_async(data_train_tensor, nntile_mixer_model.activations[0].value)

mlp_mixer_model.zero_grad()
torch_output = mlp_mixer_model(patched_train_sample)
_, pred_labels_torch = torch.max(torch_output, 1)

np_torch_output = np.array(torch_output.detach().numpy(), order="F", dtype=np.float32)

nntile_mixer_model.forward_async()

nntile_last_layer_output = np.zeros(nntile_mixer_model.activations[-1].value.shape, order="F", dtype=np.float32)
nntile_mixer_model.activations[-1].value.to_array(nntile_last_layer_output)
pred_labels_nntile = np.argmax(nntile_last_layer_output, 1)
print("PyTorch predicted label: {}".format(pred_labels_torch))
print("NNTile predicted label: {}".format(pred_labels_nntile))
print("Norm of inference difference: {}".format(np.linalg.norm(nntile_last_layer_output - np_torch_output, 'fro')))

data_train_tensor.unregister()
nntile_mixer_model.unregister()
