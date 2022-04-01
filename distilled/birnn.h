

#pragma once

#include "utils.h"

namespace distilled::birnn {
   
Exprs forward(Ptr<ExpressionGraph>& graph, const std::vector<WordIndex>& tokens_src, const int dim_emb,
              const std::vector<float>& mask_src);
                 
Expr text_field_embedder_src(Ptr<ExpressionGraph>& graph, const std::vector<WordIndex>& tokens_src, const int dim_batch,
                             const int dim_emb);

Expr seq2seq_encoder_src(Ptr<ExpressionGraph>& graph, const Expr& embedded_text_src, const int dim_emb,
                         const std::vector<float>& mask_src);
                         
Expr linear_layer_src( Ptr<ExpressionGraph>& graph, const Expr& encoded_text_src );

}  // namespace distilled::birnn
