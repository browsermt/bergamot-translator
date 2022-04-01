

#include "birnn.h"

#include "models/decoder.h"
#include "models/encoder.h"
#include "models/model_factory.h"
#include "models/s2s.h"
#include "utils.h"

namespace distilled::birnn {
   
Exprs forward(Ptr<ExpressionGraph>& graph, const std::vector<WordIndex>& tokens_src, const int dim_emb,
              const std::vector<float>& mask_src) {
  const int dim_batch = 1;
  const int dim_tokens_src = tokens_src.size();
  
  auto embedded_text_src = text_field_embedder_src( graph, tokens_src, dim_batch, dim_emb );

  auto encoded_text_src = seq2seq_encoder_src(graph, embedded_text_src, dim_emb, mask_src );

  auto encoded_text_src_linear_op =  linear_layer_src(graph, encoded_text_src);
  
  std::cout << graph->graphviz() << std::endl;  
  
  graph->forward();

  return {{"embedded_text_src", embedded_text_src},
          {"encoded_text_src", encoded_text_src},
          {"encoded_text_src_linear_op", encoded_text_src_linear_op}};
}
   
Expr text_field_embedder_src(Ptr<ExpressionGraph>& graph, const std::vector<WordIndex>& tokens_src, const int dim_batch,
                             const int dim_emb) {
  const int dim_tokens_src = tokens_src.size();

  auto embeddings_txt_src = graph->get("embeddings_txt_src");

  auto embedded_text_src = reshape(rows(embeddings_txt_src, tokens_src), {dim_batch, dim_tokens_src, dim_emb});

  return embedded_text_src;
}

Expr seq2seq_encoder_src(Ptr<ExpressionGraph>& graph, const Expr& embedded_text_src, const int dim_emb,
                         const std::vector<float>& mask_src) {
  auto options =
      New<Options>("enc-depth", 1, "dropout-rnn", 0.0f, "enc-cell", "gru", "dim-rnn", dim_emb, "layer-normalization",
                   false, "skip", false, "enc-cell-depth", 1, "prefix", "encoder_s2s_text_src", "hidden-bias", true);

  marian::EncoderS2S encoderS2S(graph, options);

  auto embedded_text_src_t = transpose(embedded_text_src, {1, 0, 2});

  auto mask_src_expr = graph->constant({static_cast<int>(mask_src.size()), 1, 1}, marian::inits::fromVector(mask_src));

  auto encoded_text_src_t = encoderS2S.applyEncoderRNN(graph, embedded_text_src_t, mask_src_expr, "bidirectional");

  return transpose(encoded_text_src_t, {1, 0, 2});
}

Expr linear_layer_src(Ptr<ExpressionGraph>& graph, const Expr& encoded_text_src) {
  return linear(encoded_text_src, graph->get("linear_layer_src_weight"), graph->get("linear_layer_src_bias"));
}

}  // namespace distilled::birnn
