

#include "utils.h"

namespace distilled {

Expr linear(const Expr& x, const Expr& A, const Expr b) { return dot(x, A, false, true) + b; }

void saveResults(const std::string& filePath, const Exprs& exprs) {
  std::vector<io::Item> items;

  std::transform(std::begin(exprs), std::end(exprs), std::back_inserter(items), [](const auto& expr) {
    io::Item item;
    expr.second->val()->get(item, expr.first);
    return item;
  });

  io::saveItems(filePath, items);
}

}  // namespace distilled
