#ifndef BERGAMOT_TRANSLATOR_TAG_PROCESSOR_H_
#define BERGAMOT_TRANSLATOR_TAG_PROCESSOR_H_

#include <string>
#include <vector>

#include "annotation.h"
#include "data/alignment.h"

namespace marian {
namespace bergamot {

struct TagNode {
  size_t parent;              // can be removed; used if backtrack
  ByteRange bound;            // tag position given by the token indices [bound.begin, bound.end)
  std::string_view label;     // tag content
  std::vector<size_t> child;  // holds the indices of its children nodes

  TagNode(ByteRange bound, std::vector<size_t> child) : bound(bound), child(std::move(child)){};
};

class TagProcessor {
  std::vector<TagNode> tagTree_;  // holds the tree structure of the tag positions in the source sentence

  size_t srcLength_;
  size_t tgtLength_;

  std::vector<std::vector<std::vector<double>>> inside_;

 public:
  std::vector<TagNode> tagTreeTarget;  // store query results: the tag positions in the target sentence

  TagProcessor(const marian::data::SoftAlignment &align, std::vector<TagNode> tagTree, size_t srcLength,
               size_t tgtLength)
      : tagTree_(std::move(tagTree)), srcLength_(srcLength), tgtLength_(tgtLength) {
    inside_.resize(srcLength, std::vector<std::vector<double>>(srcLength, std::vector<double>(tgtLength, 0)));
    tagTreeTarget = tagTree_;
    fillInsideNaive(align);
  };

  void fillInsideNaive(const marian::data::SoftAlignment &align) {
    for (size_t t = 0; t < tgtLength_; t++) {
      for (size_t i = 0; i < srcLength_; i++) {
        inside_[i][i][t] = align[t][i];
        for (size_t j = i + 1; j < srcLength_; j++) {
          inside_[i][j][t] = inside_[i][j - 1][t] + align[t][j];
        }
      }
    }
  }

  // outer and inner are introduced to limit the tag placements to reserve tag nesting order
  // outer is determined by the parent node, as the current bound must be inside the parent bound
  // inner is determined by all the children nodes, as the current tag bound cannot be overlapped
  // with the other children bounds
  ByteRange maxProduct(ByteRange query, ByteRange outer, ByteRange inner) {
    double max = -std::numeric_limits<double>::infinity();
    ByteRange maxBound{};

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
            maxBound.begin = l;
            maxBound.end = r;
          }
        }
      }
    }
    // Empty tags where query.begin = query.end
    // Note: assume the tags are placed before the token, e.g., <b>word
    else {
      if (query.begin == 0) {
        maxBound.begin = maxBound.end = 0;
      } else if (query.begin == tgtLength_) {
        maxBound.begin = maxBound.end = tgtLength_;
      } else {
        for (size_t d = outer.begin; d < outer.end; d++) {
          if (d <= inner.begin || d >= inner.end) {
            double logProduct = 0;
            for (size_t t = 0; t < tgtLength_; t++) {
              if (t < d) {
                logProduct += log(inside_[0][query.begin - 1][t]);
              } else {
                logProduct += log(inside_[query.begin][srcLength_ - 1][t]);
              }
            }
            if (max < logProduct) {
              max = logProduct;
              maxBound.begin = maxBound.end = d;
            }
          }
        }
      }
    }

    return maxBound;
  }

  // return 0 if query solution is found; return 1 if no solution found
  int traverseAndQuery(size_t idx, ByteRange selfOuter, ByteRange &currentRange) {
    ByteRange childOuter = selfOuter;
    ByteRange selfInner{};  // the constraints from all children nodes

    // cannot place the current tag as the parent bound is less than 0
    if (selfOuter.size() <= 0) return 1;

    selfInner.begin = selfOuter.end;
    selfInner.end = selfOuter.begin;

    for (unsigned long i : tagTree_[idx].child) {
      ByteRange childRange{};  // the constraints from all traversed children nodes
      // traverse child node from left to right recursively
      int childExitStatus = traverseAndQuery(i, childOuter, childRange);
      if (childExitStatus != 0) return childExitStatus;
      // the child outer must be after the last child node
      childOuter.begin = childRange.end;
      // the current inner must include the constraints fromm all traversed children nodes
      selfInner.begin = (selfInner.begin > childRange.begin) ? childRange.begin : selfInner.begin;
      selfInner.end = (selfInner.end < childRange.end) ? childRange.end : selfInner.begin;
    }
    currentRange = maxProduct(tagTree_[idx].bound, selfOuter, selfInner);
    tagTreeTarget[idx].bound = currentRange;
    return 0;
  }
};
}  // namespace bergamot
}  // namespace marian

#endif  // BERGAMOT_TRANSLATOR_TAG_PROCESSOR_H_
