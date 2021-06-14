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
  std::vector<TagNode> tagTree;

  size_t srcLength;
  size_t tgtLength;

  std::vector<std::vector<std::vector<double>>> inside;
  std::vector<std::vector<std::vector<double>>> outside;

public:
  TagProcessor(marian::data::SoftAlignment &align,
               std::vector<TagNode> tagTree,
               size_t srcLength,
               size_t tgtLength) : tagTree(std::move(tagTree)), srcLength(srcLength), tgtLength(tgtLength) {
    inside.resize(srcLength,std::vector<std::vector<double>>(srcLength,std::vector<double>(tgtLength,0)));
    outside.resize(srcLength,std::vector<std::vector<double>>(srcLength,std::vector<double>(tgtLength,0)));
    tagTreeTarget = tagTree;
    fillInsideNaive(align);
  };

  std::vector<TagNode> tagTreeTarget;

  double alignProbability(marian::data::SoftAlignment &align, size_t s, size_t t) {
    return align[t][s];
  }

  void fillInsideNaive(marian::data::SoftAlignment &align) {
    for (size_t t = 0; t < tgtLength; t++) {
      for (size_t i = 0; i < srcLength; i++) {
        inside[i][i][t] = alignProbability(align, i, t);
        for (size_t j = i + 1; j < srcLength; j++) {
          inside[i][j][t] = inside[i][j - 1][t] + alignProbability(align, j, t);
          outside[i][j][t] = 1.0 - inside[i][j - 1][t];
        }
      }
    }
  }

  ByteRange maxProduct(ByteRange query, ByteRange outer, ByteRange inner) {
    double max = 0;
    ByteRange maxi;

    for (size_t l = outer.begin; l < inner.begin; l++) {
      for (size_t r = (l > inner.end) ? l : inner.end; r < outer.end; r++) {
        double product = 1;
        for (size_t t = l; t < r; t++)
        {
          product *= inside[query.begin][query.end][t] * outside[query.begin][query.end][t];
        }
        if (max < product) {
          max = product;
          maxi.begin = l;
          maxi.end = r + 1;
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

    for (size_t i = 0; i < tagTree[idx].child.size(); i++) {
      ByteRange child_range = traverseAndQuery(tagTree[idx].child[i], child_outer);
      child_outer.begin = child_range.end;
      self_inner.begin = (self_inner.begin > child_range.begin) ? child_range.begin : self_inner.begin;
      self_inner.end = (self_inner.end < child_range.end) ? child_range.end : self_inner.begin;
    }
    ByteRange currentRange = maxProduct(tagTree[idx].bound, self_outer, self_inner);
    std::cout <<idx <<" " <<currentRange.begin <<" " <<currentRange.end <<std::endl;
    return currentRange;
  }
};
}  // namespace bergamot
}  // namespace marian

#endif //BERGAMOT_TRANSLATOR_TAG_NESTING_H_
