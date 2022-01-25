import numpy

model = numpy.load('distilled.npz')
marian_results = numpy.load('distillied_marian_results.npz')
embedded_text_src =  marian_results['embedded_text_src']
encoder_s2s_text_src_bi_W =  model['encoder_s2s_text_src_bi_W']
print(numpy.dot(embedded_text_src,encoder_s2s_text_src_bi_W))

