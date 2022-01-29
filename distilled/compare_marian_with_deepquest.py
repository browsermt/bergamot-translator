import numpy

marian_results = numpy.load('distillied_marian_results.npz')
deep_quest_results = numpy.load('distillied_py_results.npz')

assert numpy.allclose(marian_results['embedded_text_src'],
                      deep_quest_results['embedded_text_src'], atol=1e-6)
