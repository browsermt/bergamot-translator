

#pragma once

#include "marian.h"

using namespace marian;

using Exprs = std::map<std::string, Expr >;

namespace distilled {

// https://pytorch.org/docs/stable/generated/torch.nn.Linear.html
Expr linear(const Expr& x, const Expr& A, const Expr b);

// https://pytorch.org/docs/stable/generated/torch.squeeze.html
Expr squeeze(const Expr& input, const int dim);

// https://pytorch.org/docs/stable/generated/torch.unsqueeze.html
Expr unsqueeze(const Expr& input, const int dim);

void saveResults(const std::string& filePath, const Exprs& exprs);

}  // namespace distilled
