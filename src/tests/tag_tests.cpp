#include "catch.hpp"
#include "translator/tag_processor.h"

using namespace marian::bergamot;

TEST_CASE("Test Tag Nesting Features with random data") {

  std::vector<TagNode> tagTree{
          TagNode({10, 100}, {1, 3, 6}),
          TagNode({15, 25}, {4, 2}),
          TagNode({21, 24}, {}),
          TagNode({30, 49}, {}),
          TagNode({17, 19}, {}),
          TagNode({68, 72}, {}),
          TagNode({55, 89}, {7, 5, 8}),
          TagNode({59, 63}, {}),
          TagNode({77, 82}, {})
  };

  size_t srcLength = 105;
  size_t tgtLength = 90;

  std::mt19937 rng;
  rng.seed(123);

  std::vector<uint_fast32_t> TgtRngSum(tgtLength, 0);
  std::vector<std::vector<uint_fast32_t>> tgtSrcRng(tgtLength, std::vector<uint_fast32_t>(srcLength, 0));

  for (int t = 0; t < tgtLength; ++t) {
    for (int s = 0; s < srcLength; ++s) {
      tgtSrcRng[t][s] = rng();
      TgtRngSum[t] += tgtSrcRng[t][s];
    }
  }

  marian::data::SoftAlignment softAlign(tgtLength, std::vector<float>(srcLength, 0));  // [t][s] -> P(s|t)
  float checkSum = 0;
  for (int t = 0; t < tgtLength; ++t) {
    for (int s = 0; s < srcLength; ++s) {
      softAlign[t][s] = ((float) tgtSrcRng[t][s]) / TgtRngSum[t];
      checkSum += softAlign[t][s];
    }
    REQUIRE(checkSum == Approx(1.0));
    checkSum = 0;
  }

  TagProcessor tp = TagProcessor(softAlign, tagTree, srcLength, tgtLength);

  ByteRange rtn = tp.traverseAndQuery(0, {0, tgtLength});

//  std::vector<TagNode> tagTreeTarget = tp.tagTreeTarget;

//  for (size_t i = 0; i < tagTreeTarget.size(); i++)
//  {
//    std::cout <<i <<tagTreeTarget[i].bound.begin << tagTreeTarget[i].bound.end <<std::endl;
//  }
}
