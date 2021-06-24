#ifndef BERGAMOT_TRANSLATOR_TAG_NESTING_H_
#define BERGAMOT_TRANSLATOR_TAG_NESTING_H_

#include <string>
#include <vector>

#include "annotation.h"
#include "data/alignment.h"

namespace marian {
namespace bergamot {

struct TagNode {
  size_t parent;  // can be removed; used if backtrack
  ByteRange bound;
  std::string_view label;
  std::vector<size_t> child;

  TagNode(ByteRange bound, std::vector<size_t> child) : bound(bound), child(std::move(child)){};
};

class TagProcessor {
  std::vector<TagNode> tagTree_;

  size_t srcLength_;
  size_t tgtLength_;

  std::vector<std::vector<std::vector<double>>> inside_;

 public:
  std::vector<TagNode> tagTreeTarget;  // store query results

  TagProcessor(const marian::data::SoftAlignment &align, std::vector<TagNode> tagTree, size_t srcLength,
               size_t tgtLength)
      : tagTree_(std::move(tagTree)), srcLength_(srcLength), tgtLength_(tgtLength) {
    inside_.resize(srcLength, std::vector<std::vector<double>>(srcLength, std::vector<double>(tgtLength, 0)));
    tagTreeTarget = tagTree_;
    fillInsideNaive(align);
  };

  double alignProbability(const marian::data::SoftAlignment &align, size_t s, size_t t) { return align[t][s]; }

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
    double max = -std::numeric_limits<double>::infinity();
    ByteRange maxi;

    if (query.begin < query.end) {
      for (size_t l = outer.begin; l <= inner.begin; l++) {
        for (size_t r = (l + 1 > inner.end) ? (l + 1) : inner.end; r <= outer.end; r++) {
          double logProduct = 0;
          for (size_t t = 0; t < tgtLength_; t++) {
            if (t >= l && t < r) {
              logProduct += log(inside_[query.begin][query.end - 1][t]);
            } else {
              logProduct += log1p(-inside_[query.begin][query.end - 1][t]);
            }
          }
          if (max < logProduct) {
            max = logProduct;
            maxi.begin = l;
            maxi.end = r;
          }
        }
      }
    } else {
      for (size_t d = outer.begin; d < outer.end; d++) {
        if (d <= inner.begin || d >= inner.end) {
          double logProduct = log(inside_[0][query.begin][d]) + log1p(-inside_[query.begin + 1][srcLength_ - 1][d]);
          if (max < logProduct) {
            max = logProduct;
            maxi.begin = maxi.end = d;
          }
        }
      }
    }

    return maxi;
  }

  int traverseAndQuery(size_t idx, ByteRange self_outer, ByteRange &currentRange) {
    ByteRange child_outer = self_outer;
    ByteRange self_inner;

    if (self_outer.size() <= 0) return 1;

    self_inner.begin = self_outer.end;
    self_inner.end = self_outer.begin;

    for (size_t i = 0; i < tagTree_[idx].child.size(); i++) {
      ByteRange child_range;
      int childExitStatus = traverseAndQuery(tagTree_[idx].child[i], child_outer, child_range);
      if (childExitStatus != 0) return childExitStatus;
      child_outer.begin = child_range.end;
      self_inner.begin = (self_inner.begin > child_range.begin) ? child_range.begin : self_inner.begin;
      self_inner.end = (self_inner.end < child_range.end) ? child_range.end : self_inner.begin;
    }
    currentRange = maxProduct(tagTree_[idx].bound, self_outer, self_inner);
    tagTreeTarget[idx].bound = currentRange;
    return 0;
  }
};
}  // namespace bergamot
}  // namespace marian

#endif  // BERGAMOT_TRANSLATOR_TAG_NESTING_H_
