#pragma once
#include <string>

namespace marian {
namespace bergamot {
/// Options dictating how to construct a response for an input text.

enum ConcatStrategy { FAITHFUL, SPACE };
enum QualityScoreType { FREE, EXPENSIVE };

struct ResponseOptions {
  bool qualityScores{false};
  bool alignment{false};
  float alignmentThreshold{0.2f};
  QualityScoreType qualityScoreType{QualityScoreType::FREE};
  ConcatStrategy concatStrategy{ConcatStrategy::FAITHFUL};
};

} // namespace bergamot
} // namespace marian
