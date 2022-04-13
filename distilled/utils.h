

#pragma once

#include "marian.h"

using namespace marian;

using Exprs = std::vector<std::pair<std::string, Expr> >;

namespace distilled {
   
   // https://pytorch.org/docs/stable/generated/torch.nn.Linear.html
   Expr linear( const Expr& x, const Expr& A, const Expr b);
   
   void saveResults(const std::string& filePath, const Exprs& exprs);

}  // namespace distilled::birnn
