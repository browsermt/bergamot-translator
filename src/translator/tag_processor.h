#ifndef BERGAMOT_TRANSLATOR_TAG_NESTING_H_
#define BERGAMOT_TRANSLATOR_TAG_NESTING_H_

#include <vector>
#include <string>

#include "data/alignment.h"
#include "annotation.h"

namespace marian {
namespace bergamot {

struct TagNode {
  size_t parent;  // can be removed; used if backtrack
  ByteRange bound;
  std::string_view label;
  std::vector<size_t> child;

  TagNode(ByteRange bound,std::vector<size_t> child): bound(bound),child(std::move(child)){};
};

class TagProcessor {
  std::vector<TagNode> tagTree_;

  size_t srcLength_;
  size_t tgtLength_;

  std::vector<std::vector<std::vector<double>>> inside_;

public:
  std::vector<TagNode> tagTreeTarget; // store query results

  TagProcessor(const marian::data::SoftAlignment &align,
               std::vector<TagNode> tagTree,
               size_t srcLength,
               size_t tgtLength) : tagTree_(std::move(tagTree)), srcLength_(srcLength), tgtLength_(tgtLength) {
    inside_.resize(srcLength, std::vector<std::vector<double>>(srcLength, std::vector<double>(tgtLength, 0)));
    tagTreeTarget = tagTree_;
    fillInsideNaive(align);
  };

  double alignProbability(const marian::data::SoftAlignment &align, size_t s, size_t t) {
    return align[t][s];
  }

  void fillInsideNaive(const marian::data::SoftAlignment &align) {
    for (size_t t = 0; t < tgtLength_; t++) {
      for (size_t i = 0; i < srcLength_; i++) {
        inside_[i][i][t] = alignProbability(align, i, t);
        for (size_t j = i + 1; j < srcLength_; j++) {
          inside_[i][j][t] = inside_[i][j - 1][t] + alignProbability(align, j, t);
        }
      }
    }
  }

  ByteRange maxProduct(ByteRange query, ByteRange outer, ByteRange inner) {
    double max = 0;
    ByteRange maxi;

    for (size_t l = outer.begin; l <= inner.begin; l++) {
      for (size_t r = (l+1 > inner.end) ? (l+1) : inner.end; r <= outer.end; r++) {
        double product = 1;
        for (size_t t = 0; t < tgtLength_; t++)
        {
          if (t >= l && t < r)
          {
            product *= inside_[query.begin][query.end - 1][t];
          }
          else
          {
            product *= 1 - inside_[query.begin][query.end - 1][t];
          }
        }
        if (max < product) {
          max = product;
          maxi.begin = l;
          maxi.end = r;
        }
      }
    }

    return maxi;
  }

  ByteRange traverseAndQuery(size_t idx, ByteRange self_outer) {
    ByteRange child_outer = self_outer;
    ByteRange self_inner;

    self_inner.begin = self_outer.end;
    self_inner.end = self_outer.begin;

    for (size_t i = 0; i < tagTree_[idx].child.size(); i++) {
      ByteRange child_range = traverseAndQuery(tagTree_[idx].child[i], child_outer);
      child_outer.begin = child_range.end;
      self_inner.begin = (self_inner.begin > child_range.begin) ? child_range.begin : self_inner.begin;
      self_inner.end = (self_inner.end < child_range.end) ? child_range.end : self_inner.begin;
    }
    ByteRange currentRange = maxProduct(tagTree_[idx].bound, self_outer, self_inner);
    tagTreeTarget[idx].bound = currentRange;
    return currentRange;
  }
};
}  // namespace bergamot
}  // namespace marian

#endif //BERGAMOT_TRANSLATOR_TAG_NESTING_H_
