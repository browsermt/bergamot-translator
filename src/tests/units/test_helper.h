#pragma once

#include <ostream>

#include "catch.hpp"
#include "translator/annotation.h"
#include "translator/history.h"

bool operator==(const std::vector<float>& value1, const std::vector<float>& value2) {
  return std::equal(value1.begin(), value1.end(), value2.begin(), value2.end(), [](const auto& a, const auto& b) {
    auto value = Approx(b).epsilon(0.001);
    return a == value;
  });
}

bool operator==(const std::vector<marian::bergamot::ByteRange>& value1,
                const std::vector<marian::bergamot::ByteRange>& value2) {
  return std::equal(value1.begin(), value1.end(), value2.begin(), value2.end(),
                    [](const auto& a, const auto& b) { return (a.begin == b.begin) && (a.end == b.end); });
}

std::ostream& operator<<(std::ostream& os, const marian::bergamot::ByteRange& value) {
  os << "{begin: " << value.begin << ", end: " << value.end << "} ";
  return os;
}

marian::Histories logProbsToHistories(const std::vector<float>& logProbs) {
  marian::Word word;

  auto topHyp = marian::Hypothesis::New();
  float prevLogProb = 0.0;

  for (const auto& logProb : logProbs) {
    topHyp = std::move(marian::Hypothesis::New(std::move(topHyp), word, 0, logProb + prevLogProb));
    prevLogProb += logProb;
  }

  auto history = std::make_shared<marian::History>(1, 0.0);

  history->add(marian::Beam{topHyp}, word, false);

  return {history};
}
