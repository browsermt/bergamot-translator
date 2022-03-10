
"""
This script converts the distilled model weights trained in python(torch) to a marian weights file (npz)
The distilled model files is listed on: 
  - https://github.com/sheffieldnlp/deepQuest-py/tree/main/examples/knowledge_distillation#trained-student-models

usage:
  python3 dp2marian.py --model weights.th  --output distilled.npz
"""

import torch
import numpy
import argparse

def as_rowmajor(a):
  return numpy.array(a, order='C', dtype=numpy.float32)

parser = argparse.ArgumentParser(description='Convert distilled model weights to Marian weight file.')
parser.add_argument('--model', help='Distilled model path', required=True)
parser.add_argument('--output', help='Marian output path', required=True)
args = parser.parse_args()

dpModel = torch.load(args.model,map_location=torch.device('cpu') )

marianModel = {}

# Embedding
marianModel[ 'embeddings_txt_src'] = as_rowmajor(dpModel['_text_field_embedder_src.token_embedder_tokens.weight'])

# Seq2Seq source Encoder
marianModel[ 'encoder_s2s_text_src_bi_W'] = as_rowmajor(dpModel['seq2seq_encoder_src._module.weight_ih_l0'].T)
marianModel[ 'encoder_s2s_text_src_bi_b'] = as_rowmajor(dpModel['seq2seq_encoder_src._module.bias_ih_l0'].T)

marianModel[ 'encoder_s2s_text_src_bi_U'] = as_rowmajor(dpModel['seq2seq_encoder_src._module.weight_hh_l0'].T)
marianModel[ 'encoder_s2s_text_src_bi_bu'] = as_rowmajor(dpModel['seq2seq_encoder_src._module.bias_hh_l0'].T)

marianModel[ 'encoder_s2s_text_src_bi_r_W'] = as_rowmajor(dpModel['seq2seq_encoder_src._module.weight_ih_l0_reverse'].T)
marianModel[ 'encoder_s2s_text_src_bi_r_U'] = as_rowmajor(dpModel['seq2seq_encoder_src._module.weight_hh_l0_reverse'].T)
marianModel[ 'encoder_s2s_text_src_bi_r_b'] = as_rowmajor(dpModel['seq2seq_encoder_src._module.bias_ih_l0_reverse'].T)
marianModel[ 'encoder_s2s_text_src_bi_r_bu'] = as_rowmajor(dpModel['seq2seq_encoder_src._module.bias_hh_l0_reverse'].T)

numpy.savez(args.output, **marianModel)
