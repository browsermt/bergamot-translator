

#include "utils.h"

namespace distilled {

Expr linear(const Expr& x, const Expr& A, const Expr b) { return dot(x, A, false, true) + b; }

Expr squeeze(const Expr& input, const int dim) {
  if (input->shape()[dim] != 1) {
    return input;
  }

  std::vector<int> newShape;
  std::copy(std::begin(input->shape()), std::end(input->shape()), std::back_inserter(newShape));

  if (dim < 0) {
    newShape.erase(newShape.end() - abs(dim));
  } else {
    newShape.erase(newShape.begin() + dim);
  }

  return reshape(input, Shape(std::move(newShape)));
}

Expr unsqueeze(const Expr& input, const int dim) {
  if ((dim < -1) || (dim > static_cast<int>(input->shape().size() + 1))) {
    throw std::runtime_error("Invalid dimension size");
  }

  std::vector<int> newShape;
  std::copy(std::begin(input->shape()), std::end(input->shape()), std::back_inserter(newShape));

  newShape.insert((dim == -1) ? newShape.end() : (newShape.begin() + dim), 1);

  return reshape(input, Shape(std::move(newShape)));
}

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
