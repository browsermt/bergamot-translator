#ifndef SRC_BERGAMOT_RESPONSE_OPTIONS_H_
#define SRC_BERGAMOT_RESPONSE_OPTIONS_H_
#include <string>

namespace marian {
namespace bergamot {

enum ConcatStrategy {
  /// Target text is constructed faithful to the source-text  structure.
  FAITHFUL,

  /// Target text is concatenated by a space.
  SPACE
};

/// ResponseOptions dictate how to construct a Response for an input string of
/// text to be translated.
struct ResponseOptions {
  bool qualityScores{false};  ///< Include quality-scores or not.
  bool alignment{false};      ///< Include alignments or not.

  bool HTML{false};  /// Remove HTML tags from text and (TODO) insert in output.

  /// Whether to include sentenceMappings or not. Alignments require
  /// sentenceMappings and are available irrespective of this option if
  /// `alignment=true`.
  bool sentenceMappings{false};

  ConcatStrategy concatStrategy{ConcatStrategy::FAITHFUL};

  std::string HTMLVoidTags;
  std::string HTMLInlineTags;
};

}  // namespace bergamot
}  // namespace marian

#endif  //  SRC_BERGAMOT_RESPONSE_OPTIONS_H_
