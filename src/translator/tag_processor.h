#ifndef BERGAMOT_TRANSLATOR_TAG_PROCESSOR_H_
#define BERGAMOT_TRANSLATOR_TAG_PROCESSOR_H_

#include <string>
#include <vector>

#include "annotation.h"
#include "data/alignment.h"

namespace marian {
namespace bergamot {

/// Holding Tag information
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

/// Building TagTree from a ByteRange vector (passing from the browser)
class TagTreeBuilder {
 public:
  TagTreeBuilder(std::vector<ByteRange> brv) {
    treeValid_ = true;
    nTags_ = brv.size();

    // zero-th interval must be root
    parentVector_.push_back(0);
    for (size_t i = 1; i < nTags_; i++) {
      size_t parentIndex = 0;
      size_t parentSoFarOpen = 0;
      size_t parentSoFarClose = SIZE_MAX;
      bool parentFound = false;

      // All intervals that can cover brv[i] must appear before brv[i]
      for (size_t j = 0; j < i; j++) {
        if (brv[j].begin <= brv[i].begin && brv[i].end <= brv[j].end) {
          if (parentSoFarOpen <= brv[j].begin && brv[j].end <= parentSoFarClose) {
            parentSoFarOpen = brv[j].begin;
            parentSoFarClose = brv[j].end;
            parentIndex = j;
            parentFound = true;
          }
        }
      }
      if (parentFound) {
        parentVector_.push_back(parentIndex);
      } else {
        treeValid_ = false;
      }
    }

    // For inspection
    coverageMatrix_.reserve(nTags_ * nTags_);
    for (size_t i = 0; i < nTags_; i++) {
      for (size_t j = 0; j < nTags_; j++) {
        coverageMatrix_[i * nTags_ + j] = (i != j && brv[i].begin <= brv[j].begin && brv[i].end >= brv[j].end);
      }
    }

    //    tt_ = TagTree(brv[0]);
    brv_ = brv;
  }

  TagTree getTagTree() { return growTagTree(0); }

  TagTree growTagTree(size_t index) {
    TagTree tt(brv_[index]);
    for (size_t childIndex = 0; childIndex < nTags_; childIndex++) {
      if (childIndex != index && parentVector_[childIndex] == index) {
        tt.addSubtree(growTagTree(childIndex));
      }
    }
    return tt;
  }

  // for debugging
  void showGraph() {
    std::cout << "GraphgrowTagTree(0); size: " << nTags_ << std::endl;
    for (size_t i = 0; i < nTags_; i++) {
      for (size_t j = 0; j < nTags_; j++) {
        std::cout << " " << coverageMatrix_[i * nTags_ + j];
      }
      std::cout << std::endl;
    }
  }

  // for debugging
  void showParents() {
    if (treeValid_) {
      std::cout << "Graph size: " << nTags_ << std::endl;
      for (size_t i = 0; i < nTags_; i++) {
        std::cout << " " << parentVector_[i];
      }
      std::cout << std::endl;
    } else {
      std::cout << "Tree invalid.\n" << nTags_ << std::endl;
    }
  }

 private:
  size_t nTags_;
  std::vector<bool> coverageMatrix_;
  std::vector<size_t> parentVector_;
  bool treeValid_;
  std::vector<ByteRange> brv_;
};

class TagProcessor {
  TagTree sourceRoot_;  // holds the tree structure of the tag positions in the source sentence
  TagTree targetRoot_;  // holds the tree structure of the tag positions in the target sentence

  size_t srcLength_;
  size_t tgtLength_;

  std::vector<double> inside_;

 public:
  TagProcessor(const marian::data::SoftAlignment &align, const TagTree &sourceRoot, size_t srcLength, size_t tgtLength)
      : sourceRoot_(sourceRoot.copy(true)),
        targetRoot_(sourceRoot.copy(false)),
        srcLength_(srcLength),
        tgtLength_(tgtLength) {
    inside_.resize((srcLength + 1) * srcLength / 2 * tgtLength);
    fillInsideNaive(align);
  };

  const TagTree &getTargetRoot() const { return targetRoot_; }

  int traverseAndQuery() {
    ByteRange targetRootRange{};
    return traverseAndQuery(sourceRoot_, targetRoot_, ByteRange{0, tgtLength_}, targetRootRange);
  }

 private:
  // inside_ is actually a 3d array [srcLength][srcLength][tgtLength], here is flattened to 1d array.
  // As inside_[srcLength][srcLength] is an upper triangle matrix,
  // f(i,j) offset is (2*n-i-1) * i/2 + j
  inline size_t flattenOffset(size_t i, size_t j, size_t k) const {
    return ((((srcLength_ << 1) - i - 1) * i >> 1) + j) * tgtLength_ + k;
  }

  void fillInsideNaive(const marian::data::SoftAlignment &align) {
    for (size_t t = 0; t < tgtLength_; t++) {
      for (size_t i = 0; i < srcLength_; i++) {
        inside_[flattenOffset(i, i, t)] = align[t][i];
        for (size_t j = i + 1; j < srcLength_; j++) {
          inside_[flattenOffset(i, j, t)] = inside_[flattenOffset(i, j - 1, t)] + align[t][j];
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
      double logProductBase = 0;
      for (size_t l = outer.begin; l <= inner.begin; l++) {
        double logProductDynamic = 0;
        for (size_t s = l; s < tgtLength_; s++) {
          logProductDynamic += log(inside_[flattenOffset(query.begin, query.end - 1, s)]);
        }
        for (size_t r = outer.end; r > l && r >= inner.end; r--) {
          double logProductFast = logProductBase + logProductDynamic;
          if (max < logProductFast) {
            max = logProductFast;
            maxBound.begin = l;
            maxBound.end = r;
          }
          logProductDynamic = logProductDynamic - log(inside_[flattenOffset(query.begin, query.end - 1, r - 1)]) +
                              log1p(-inside_[flattenOffset(query.begin, query.end - 1, r - 1)]);
        }
        logProductBase += log1p(-inside_[flattenOffset(query.begin, query.end - 1, l)]);
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
                logProduct += log(inside_[flattenOffset(0, query.begin - 1, t)]);
              } else {
                logProduct += log(inside_[flattenOffset(query.begin, srcLength_ - 1, t)]);
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
