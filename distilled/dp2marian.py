
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

parser = argparse.ArgumentParser(description='Convert distilled model weights to Marian weight file.')
parser.add_argument('--model', help='Distilled model path', required=True)
parser.add_argument('--output', help='Marian output path', required=True)
args = parser.parse_args()

dpModel = torch.load(args.model,map_location=torch.device('cpu') )

marianModel = {}
marianModel[ 'embeddings_txt_src'] = dpModel['_text_field_embedder_src.token_embedder_tokens.weight']
marianModel[ 'encoder_s2s_text_src_bi_W'] = dpModel['seq2seq_encoder_src._module.weight_ih_l0'].T
marianModel[ 'encoder_s2s_text_src_bi_U'] = dpModel['seq2seq_encoder_src._module.weight_hh_l0'].T # + w_sum.T
marianModel[ 'encoder_s2s_text_src_bi_b'] = dpModel['seq2seq_encoder_src._module.bias_ih_l0']
marianModel[ 'encoder_s2s_text_src_bi_bu'] = dpModel['seq2seq_encoder_src._module.bias_hh_l0']

marianModel[ 'encoder_s2s_text_src_bi_r_W'] = dpModel['seq2seq_encoder_src._module.weight_ih_l0_reverse'].T
marianModel[ 'encoder_s2s_text_src_bi_r_U'] = dpModel['seq2seq_encoder_src._module.weight_hh_l0_reverse'].T # + w_reverse_sum.T
marianModel[ 'encoder_s2s_text_src_bi_r_b'] = dpModel['seq2seq_encoder_src._module.bias_ih_l0_reverse']
marianModel[ 'encoder_s2s_text_src_bi_r_bu'] = dpModel['seq2seq_encoder_src._module.bias_hh_l0_reverse']

numpy.savez(args.output, **marianModel)
