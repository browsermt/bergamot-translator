#ifndef BERGAMOT_TRANSLATOR_TAG_PROCESSOR_H_
#define BERGAMOT_TRANSLATOR_TAG_PROCESSOR_H_

#include <string>
#include <vector>

#include "annotation.h"
#include "data/alignment.h"

namespace marian {
namespace bergamot {

class TagTree {
 public:
  friend class TagProcessor;  // allow TagProcessor class to access private members

  TagTree(const ByteRange &bound) : bound_(bound) {}

  // for debugging
  void print(size_t indent = 0) const {
    for (size_t nsp = 0; nsp < indent * 2; nsp++) std::cout << " ";
    std::cout << bound_.begin << " " << bound_.end << std::endl;
    for (auto &i : subtree_) {
      i.print(indent + 1);
    }
  }

  // bottom-up construction
  void addSubtree(const TagTree &st) { subtree_.emplace_back(st); }

  // copy the skeleton (false) or the whole tree (true)
  TagTree copy(bool copyBound) const {
    ByteRange newBound = copyBound ? bound_ : ByteRange{0, 0};
    TagTree newCurrent(newBound);
    for (auto &i : subtree_) {
      TagTree subCopy = i.copy(copyBound);
      newCurrent.addSubtree(subCopy);
    }
    return newCurrent;
  }

  friend bool operator==(const TagTree &lhs, const TagTree &rhs) {
    if (lhs.bound_.begin != rhs.bound_.begin || lhs.bound_.end != rhs.bound_.end) return false;
    if (lhs.subtree_.size() != rhs.subtree_.size()) return false;
    for (size_t i = 0; i < lhs.subtree_.size(); i++) {
      if (lhs.subtree_[i] != rhs.subtree_[i]) return false;
    }
    return true;
  }

  friend bool operator!=(const TagTree &lhs, const TagTree &rhs) { return !(lhs == rhs); }

 private:
  // holds tag positions given by the token indices [begin, end):
  // for a tag pair, bound_.begin means the position of the opening tag,
  //                 bound_.end means the position of the closing tag (excluding the current token)
  // for an empty tag, bound_.begin = bound_.end. The tag is placed before the token, e.g., <b>word
  ByteRange bound_;
  std::vector<TagTree> subtree_;  // holds the children nodes
};

class TagProcessor {
  TagTree sourceRoot_;  // holds the tree structure of the tag positions in the source sentence
  TagTree targetRoot_;  // holds the tree structure of the tag positions in the target sentence

  size_t srcLength_;
  size_t tgtLength_;

  std::vector<std::vector<std::vector<double>>> inside_;

 public:
  TagProcessor(const marian::data::SoftAlignment &align, const TagTree &sourceRoot, size_t srcLength, size_t tgtLength)
      : sourceRoot_(sourceRoot.copy(true)),
        targetRoot_(sourceRoot.copy(false)),
        srcLength_(srcLength),
        tgtLength_(tgtLength) {
    inside_.resize(srcLength, std::vector<std::vector<double>>(srcLength, std::vector<double>(tgtLength, 0)));
    fillInsideNaive(align);
  };

  const TagTree &getTargetRoot() const { return targetRoot_; }

  int traverseAndQuery() {
    ByteRange targetRootRange{};
    return traverseAndQuery(sourceRoot_, targetRoot_, ByteRange{0, tgtLength_}, targetRootRange);
  }

 private:
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
      } else if (query.begin == srcLength_) {
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
  int traverseAndQuery(TagTree sourceTagTree, TagTree &targetTagTree, ByteRange selfOuter, ByteRange &currentRange) {
    ByteRange childOuter = selfOuter;
    ByteRange selfInner{};  // the constraints from all children nodes

    // cannot place the current tag as the parent bound is less than 0
    if (selfOuter.size() <= 0) return 1;

    selfInner.begin = selfOuter.end;
    selfInner.end = selfOuter.begin;

    for (size_t cid = 0; cid < sourceTagTree.subtree_.size(); cid++) {
      ByteRange childRange{};  // the constraints from all traversed children nodes
      // traverse child node from left to right recursively
      int childExitStatus =
          traverseAndQuery(sourceTagTree.subtree_[cid], targetTagTree.subtree_[cid], childOuter, childRange);
      if (childExitStatus != 0) return childExitStatus;
      // the child outer must be after the last child node
      childOuter.begin = childRange.end;
      // the current inner must include the constraints fromm all traversed children nodes
      selfInner.begin = (selfInner.begin > childRange.begin) ? childRange.begin : selfInner.begin;
      selfInner.end = (selfInner.end < childRange.end) ? childRange.end : selfInner.begin;
    }
    currentRange = maxProduct(sourceTagTree.bound_, selfOuter, selfInner);
    targetTagTree.bound_ = currentRange;
    return 0;
  }
};
}  // namespace bergamot
}  // namespace marian

#endif  // BERGAMOT_TRANSLATOR_TAG_PROCESSOR_H_
