

#include "birnn.h"

#include "models/decoder.h"
#include "models/encoder.h"
#include "models/model_factory.h"
#include "models/s2s.h"
#include "utils.h"

namespace distilled::birnn {

Exprs forward(Ptr<ExpressionGraph>& graph, const int dim_emb, const std::vector<WordIndex>& tokens_src,
              const std::vector<float>& mask_src, const std::vector<WordIndex>& tokens_tgt,
              const std::vector<float>& mask_tgt) {
  auto exprs_src = forward_input(graph, "src", tokens_src, dim_emb, mask_src);
  auto exprs_tgt = forward_input(graph, "tgt", tokens_tgt, dim_emb, mask_tgt);

  auto encoded_text_src = exprs_src["encoded_text_src_weighted_sum"];
  auto encoded_text_tgt = exprs_tgt["encoded_text_tgt_weighted_sum"];

  auto encoded_text = concatenate({encoded_text_src,encoded_text_tgt},-1);
  auto scores = squeeze(sigmoid(linear_layer(graph, "", encoded_text )), -1);

  std::cout << graph->graphviz() << std::endl;

  graph->forward();

  exprs_src.merge(exprs_tgt);
  exprs_src["encoded_text"] = encoded_text;
  exprs_src["scores"] = scores;

  return exprs_src;
}

Exprs forward_input(Ptr<ExpressionGraph>& graph, const std::string& posfix, const std::vector<WordIndex>& tokens,
                    const int dim_emb, const std::vector<float>& mask) {
  const int dim_batch = 1;
  const int dim_tokens = tokens.size();

  auto embedded_text = text_field_embedder(graph->get("embeddings_txt_" + posfix), tokens, dim_batch, dim_emb);

  auto encoded_text = seq2seq_encoder(graph, posfix, embedded_text, dim_emb, mask);

  auto encoded_text_linear_op = linear_layer(graph, "_" + posfix, encoded_text);

  auto attention_dist = attention(graph->get("context_weights_" + posfix), encoded_text_linear_op);

  auto encoded_text_weighted_sum = weighted_sum(encoded_text_linear_op, attention_dist);

  return {{"embedded_text_" + posfix, embedded_text},
          {"encoded_text_" + posfix, encoded_text},
          {"encoded_text_" + posfix + "_linear_op", encoded_text_linear_op},
          {"attention_dist_" + posfix, attention_dist},
          {"encoded_text_" + posfix + "_weighted_sum", encoded_text_weighted_sum}};
}

Expr text_field_embedder(const Expr& embeddings_txt, const std::vector<WordIndex>& tokens, const int dim_batch,
                         const int dim_emb) {
  const int dim_tokens = tokens.size();
  return reshape(rows(embeddings_txt, tokens), {dim_batch, dim_tokens, dim_emb});
}

Expr seq2seq_encoder(Ptr<ExpressionGraph>& graph, const std::string& posfix, const Expr& embedded_text,
                     const int dim_emb, const std::vector<float>& mask) {
  auto options = New<Options>("enc-depth", 1, "dropout-rnn", 0.0f, "enc-cell", "gru", "dim-rnn", dim_emb,
                              "layer-normalization", false, "skip", false, "enc-cell-depth", 1, "prefix",
                              "encoder_s2s_text_" + posfix, "hidden-bias", true);

  marian::EncoderS2S encoderS2S(graph, options);

  auto embedded_text_t = transpose(embedded_text, {1, 0, 2});

  auto mask_expr = graph->constant({static_cast<int>(mask.size()), 1, 1}, marian::inits::fromVector(mask));

  auto encoded_text_t = encoderS2S.applyEncoderRNN(graph, embedded_text_t, mask_expr, "bidirectional");

  return transpose(encoded_text_t, {1, 0, 2});
}

Expr linear_layer(Ptr<ExpressionGraph>& graph, const std::string& posfix, const Expr& encoded_text) {
  return linear(encoded_text, graph->get("linear_layer" + posfix + "_weight"),
                graph->get("linear_layer" + posfix + "_bias"));
}

Expr attention(const Expr& context_weights, const Expr& encoded_text) {
  return softmax(squeeze(bdot(encoded_text, unsqueeze(repeat(context_weights, encoded_text->shape()[0], 0), -1)), -1));
}

Expr weighted_sum(const Expr& matrix, const Expr& attention) {
  return squeeze(bdot(unsqueeze(attention, 1), matrix), 1);
}

}  // namespace distilled::birnn
