#include "print_utils.h"

namespace marian {
namespace bergamot {

void Printer::sentences(std::ostream &out) {
  for (int sentenceIdx = 0; sentenceIdx < response_->size(); sentenceIdx++) {
    sentence(out, sentenceIdx);
    alignments(out, sentenceIdx);
    quality(out, sentenceIdx);
  }
  out << "--------------------------\n";
  out << '\n';
}
void Printer::sentence(std::ostream &out, size_t sentenceIdx) {
  out << " [src Sentence]: " << response_->source.sentence(sentenceIdx) << '\n';
  out << " [tgt Sentence]: " << response_->target.sentence(sentenceIdx) << '\n';
}

void Printer::alignments(std::ostream &out, size_t sentenceIdx) {
  out << "Alignments" << '\n';
  typedef std::pair<size_t, float> Point;

  // Initialize a point vector.
  std::vector<std::vector<Point>> aggregate(
      response_->source.numWords(sentenceIdx));

  // Handle alignments
  auto &alignments = response_->alignments[sentenceIdx];
  for (auto &p : alignments) {
    aggregate[p.src].emplace_back(p.tgt, p.prob);
  }

  for (size_t src = 0; src < aggregate.size(); src++) {
    out << response_->source.word(sentenceIdx, src) << ": ";
    for (auto &p : aggregate[src]) {
      out << response_->target.word(sentenceIdx, p.first) << "(" << p.second
          << ") ";
    }
    out << '\n';
  }
}

void Printer::quality(std::ostream &out, size_t sentenceIdx) {
  // Handle quality.
  auto &quality = response_->qualityScores[sentenceIdx];
  out << "Quality: whole(" << quality.sequence << "), tokens below:" << '\n';
  size_t wordIdx = 0;
  bool first = true;
  for (auto &p : quality.word) {
    if (first) {
      first = false;
    } else {
      out << " ";
    }
    out << response_->target.word(sentenceIdx, wordIdx) << "(" << p << ")";
    wordIdx++;
  }
  out << '\n';
}

void Printer::text(std::ostream &out) {
  out << "[original]: " << response_->source.text << '\n';
  out << "[translated]: " << response_->target.text << '\n';
}

} // namespace bergamot
} // namespace marian
