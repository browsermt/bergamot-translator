import numpy
import argparse

parser = argparse.ArgumentParser(description='Compare the distilled marian results with deepQuest')
parser.add_argument('--marian', help='Marian result file path', required=True)
parser.add_argument('--dq', help='DeepQuest results file path', required=True)
args = parser.parse_args()

marian_results = numpy.load(args.marian)
deep_quest_results = numpy.load(args.dq)

assert numpy.allclose(marian_results['embedded_text_src'],
                      deep_quest_results['embedded_text_src'], atol=1e-6)

assert numpy.allclose(marian_results['encoded_text_src'],
                      deep_quest_results['encoded_text_src'], atol=1e-6)

assert numpy.allclose(marian_results['encoded_text_src_linear_op'],
                      deep_quest_results['encoded_text_src_linear_op'], atol=1e-6)

assert numpy.allclose(marian_results['encoded_text_src_weighted_sum'],
                      deep_quest_results['encoded_text_src_weighted_sum'], atol=1e-6)

assert numpy.allclose(marian_results['embedded_text_tgt'],
                      deep_quest_results['embedded_text_tgt'], atol=1e-6)

assert numpy.allclose(marian_results['encoded_text_tgt'],
                      deep_quest_results['encoded_text_tgt'], atol=1e-6)

assert numpy.allclose(marian_results['encoded_text_tgt_linear_op'],
                      deep_quest_results['encoded_text_tgt_linear_op'], atol=1e-6)

assert numpy.allclose(marian_results['encoded_text_tgt_weighted_sum'],
                      deep_quest_results['encoded_text_tgt_weighted_sum'], atol=1e-6)

assert numpy.allclose(marian_results['encoded_text'],
                      deep_quest_results['encoded_text'], atol=1e-6)

print("Success")
