
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

numpy.savez(args.output, **marianModel)
