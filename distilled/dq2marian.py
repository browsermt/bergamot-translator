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

for posfix in [ "src", "tgt"]:
    # Embedding
    marianModel[f"embeddings_txt_{posfix}"] = as_rowmajor(
        dqModel[f"_text_field_embedder_{posfix}.token_embedder_tokens.weight"]
    )

    dimEmb = marianModel[f"embeddings_txt_{posfix}"].shape[1]

    # Seq2Seq Encoder
    seq2seq_weights_conversion(
        dqModel,
        marianModel,
        [
        (f"seq2seq_encoder_{posfix}._module.weight_ih_l0", f"encoder_s2s_text_{posfix}_bi_W"),
        (f"seq2seq_encoder_{posfix}._module.weight_hh_l0", f"encoder_s2s_text_{posfix}_bi_U"),
        (f"seq2seq_encoder_{posfix}._module.bias_ih_l0", f"encoder_s2s_text_{posfix}_bi_b"),
        (f"seq2seq_encoder_{posfix}._module.bias_hh_l0", f"encoder_s2s_text_{posfix}_bi_bu"),
        (f"seq2seq_encoder_{posfix}._module.weight_ih_l0_reverse", f"encoder_s2s_text_{posfix}_bi_r_W"),
        (f"seq2seq_encoder_{posfix}._module.weight_hh_l0_reverse", f"encoder_s2s_text_{posfix}_bi_r_U"),
        (f"seq2seq_encoder_{posfix}._module.bias_ih_l0_reverse", f"encoder_s2s_text_{posfix}_bi_r_b"),
        (f"seq2seq_encoder_{posfix}._module.bias_hh_l0_reverse", f"encoder_s2s_text_{posfix}_bi_r_bu"),
        ],
        dimEmb,
    )

    # Linear Layer
    marianModel[f"linear_layer_{posfix}_weight"] = as_rowmajor( dqModel[f"_linear_layer_{posfix}.weight"])
    marianModel[f"linear_layer_{posfix}_bias"] = as_rowmajor( dqModel[f"_linear_layer_{posfix}.bias"])

    # Attention
    marianModel[f"context_weights_{posfix}"] = as_rowmajor( dqModel[f"context_weights_{posfix}"])

marianModel[f"linear_layer_weight"] = as_rowmajor( dqModel[f"_linear_layer.weight"])
marianModel[f"linear_layer_bias"] = as_rowmajor( dqModel[f"_linear_layer.bias"])

numpy.savez(args.output, **marianModel)
