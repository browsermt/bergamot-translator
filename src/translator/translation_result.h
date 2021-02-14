#ifndef SRC_BERGAMOT_TRANSLATION_RESULT_H_
#define SRC_BERGAMOT_TRANSLATION_RESULT_H_

#include "data/types.h"
#include "definitions.h"
#include "translator/beam_search.h"

#include <cassert>
#include <string>
#include <vector>

namespace marian {
namespace bergamot {
class Response {
public:
  Response(std::string &&source, std::vector<TokenRanges> &&sourceRanges,
           Histories &&histories, std::vector<Ptr<Vocab const>> &vocabs);

  Response(Response &&other)
      : source_(std::move(other.source_)),
        translation_(std::move(other.translation_)),
        sourceRanges_(std::move(other.sourceRanges_)),
        targetRanges_(std::move(other.targetRanges_)),
        histories_(std::move(other.histories_)){};

  Response(const Response &) = delete;
  Response &operator=(const Response &) = delete;

  typedef std::vector<std::pair<const string_view, const string_view>>
      SentenceMappings;

  void move(std::string &source, std::string &target,
            SentenceMappings &sentenceMappings);

  const Histories &histories() const { return histories_; }
  const std::string &source() const { return source_; }
  const std::string &translation() const { return translation_; }

private:
  void constructTargetProperties(std::vector<Ptr<Vocab const>> &vocabs);
  void constructSentenceMappings(SentenceMappings &);

  std::string source_;
  std::string translation_;
  Histories histories_;
  std::vector<TokenRanges> sourceRanges_;
  std::vector<TokenRanges> targetRanges_;
};
} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_TRANSLATION_RESULT_H_
