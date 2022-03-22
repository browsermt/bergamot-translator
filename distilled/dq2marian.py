"""
This script converts the distilled model weights trained in python(torch) to a marian weights file (npz)
The distilled model files is listed on: 
  - https://github.com/sheffieldnlp/deepQuest-py/tree/main/examples/knowledge_distillation#trained-student-models

usage:
  python3 dp2marian.py --model weights.th  --output distilled.npz
"""

from typing import List
from typing import Tuple
import torch
import numpy
import argparse


def as_rowmajor(a):
    return numpy.array(a, order="C", dtype=numpy.float32)


def seq2seq_weight_conversion(
    dqModel,
    dqParam: str,
    marianModel: dict,
    marianParam: str,
    dimEmb: int
):
    mW, mWx = torch.split(dqModel[dqParam].T, [2 * dimEmb, dimEmb], len(dqModel[dqParam].shape) - 1)
    marianModel[marianParam] = as_rowmajor(mW)
    marianModel[f"{marianParam}x"] = as_rowmajor(mWx)


def seq2seq_weights_conversion(
    dqModel, marianModel: dict, params: List[Tuple], dimEmb: int
):
    for dqParam, marianParam in params:
        seq2seq_weight_conversion(
            dqModel, dqParam, marianModel, marianParam, dimEmb
        )


parser = argparse.ArgumentParser(
    description="Convert distilled model weights to Marian weight file."
)
parser.add_argument("--model", help="Distilled model path", required=True)
parser.add_argument("--output", help="Marian output path", required=True)
args = parser.parse_args()

dqModel = torch.load(args.model, map_location=torch.device("cpu"))

marianModel = {}

# Embedding
marianModel["embeddings_txt_src"] = as_rowmajor(
    dqModel["_text_field_embedder_src.token_embedder_tokens.weight"]
)

dimEmb = marianModel["embeddings_txt_src"].shape[1]

# Seq2Seq source Encoder
seq2seq_weights_conversion(
    dqModel,
    marianModel,
    [
      ("seq2seq_encoder_src._module.weight_ih_l0", "encoder_s2s_text_src_bi_W"),
      ("seq2seq_encoder_src._module.weight_hh_l0", "encoder_s2s_text_src_bi_U"),
      ("seq2seq_encoder_src._module.bias_ih_l0", "encoder_s2s_text_src_bi_b"),
      ("seq2seq_encoder_src._module.bias_hh_l0", "encoder_s2s_text_src_bi_bu"),
      ("seq2seq_encoder_src._module.weight_ih_l0_reverse", "encoder_s2s_text_src_bi_r_W"),
      ("seq2seq_encoder_src._module.weight_hh_l0_reverse", "encoder_s2s_text_src_bi_r_U"),
      ("seq2seq_encoder_src._module.bias_ih_l0_reverse", "encoder_s2s_text_src_bi_r_b"),
      ("seq2seq_encoder_src._module.bias_hh_l0_reverse", "encoder_s2s_text_src_bi_r_bu"),
    ],
    dimEmb,
)

# marianModel['encoder_s2s_text_src_bi_r_bu'] = as_rowmajor(dqModel['seq2seq_encoder_src._module.bias_hh_l0_reverse'].T)

numpy.savez(args.output, **marianModel)
