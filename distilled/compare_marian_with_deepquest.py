import numpy
import torch

marian_results = numpy.load('distillied_marian_results.npz')
deep_quest_results = numpy.load('distillied_py_results.npz')

assert numpy.allclose(marian_results['embedded_text_src'],
                      deep_quest_results['embedded_text_src'], atol=1e-6)


torch.set_printoptions(precision=5,sci_mode=False)
numpy.set_printoptions(precision=5,suppress=True)

print('Marian')
print(marian_results['encoded_text_src'][0][0])
print('DeepQuest')
print(deep_quest_results['encoded_text_src'][0][0])

assert numpy.allclose(marian_results['encoded_text_src'][0][0],
                      deep_quest_results['encoded_text_src'][0][0], atol=1e-4)
