

#pragma once

#include "utils.h"

namespace distilled::birnn {

Exprs forward(Ptr<ExpressionGraph>& graph, const int dim_emb, const std::vector<WordIndex>& tokens_src,
              const std::vector<float>& mask_src, const std::vector<WordIndex>& tokens_tgt,
              const std::vector<float>& mask_tgt);

Exprs forward_input(Ptr<ExpressionGraph>& graph, const std::string& posfix, const std::vector<WordIndex>& tokens,
                    const int dim_emb, const std::vector<float>& mask);

Expr text_field_embedder(const Expr& embeddings_txt, const std::vector<WordIndex>& tokens, const int dim_batch,
                         const int dim_emb);

Expr seq2seq_encoder(Ptr<ExpressionGraph>& graph, const std::string& posfix, const Expr& embedded_text,
                     const int dim_emb, const std::vector<float>& mask);

Expr linear_layer(Ptr<ExpressionGraph>& graph, const std::string& posfix, const Expr& encoded_text);

Expr attention(const Expr& context_weights, const Expr& encoded_text);

Expr weighted_sum(const Expr& matrix, const Expr& attention);

}  // namespace distilled::birnn
