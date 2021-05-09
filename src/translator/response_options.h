#ifndef SRC_BERGAMOT_RESPONSE_OPTIONS_H_
#define SRC_BERGAMOT_RESPONSE_OPTIONS_H_
#include "3rd_party/yaml-cpp/yaml.h"
#include "common/options.h"
#include <string>

namespace marian {
namespace bergamot {

enum ConcatStrategy {
  /// Target text is constructed faithful to the source-text  structure.
  FAITHFUL,

  /// Target text is concatenated by a space.
  SPACE
};

enum QualityScoreType {
  /// Provide a free quality-score that comes with the machine-translation model
  /// itself.
  FREE,

  /// An expensive quality-score that runs additional computations to determine
  /// quality of an output.
  EXPENSIVE
};

/// ResponseOptions dictate how to construct a Response for an input string of
/// text to be translated.
struct ResponseOptions {
  bool qualityScores{false}; ///< Include quality-scores or not.
  bool alignment{false};     ///< Include alignments or not.

  /// Whether to include sentenceMappings or not. Alignments require
  /// sentenceMappings and are available irrespective of this option if
  /// `alignment=true`.
  bool sentenceMappings{false};

  /// Threshold between `[0.0f, 1.0f]` to filter alignments into a sparse
  /// matrix. Higher value implies stronger filtering leading to provision of
  /// higher-confidence matches. `1.0f` gives argmax (not the full-dense
  /// matrix).
  float alignmentThreshold{0.2f};

  QualityScoreType qualityScoreType{QualityScoreType::FREE};
  ConcatStrategy concatStrategy{ConcatStrategy::FAITHFUL};

  static ResponseOptions fromYAMLString(std::string yamlConfig) {
    Ptr<marian::Options> options;
    options->parse(yamlConfig);

    ResponseOptions responseOptions;
    responseOptions.qualityScores = options->get<bool>("quality-scores", false);
    responseOptions.alignment = options->get<bool>("alignment", false);
    responseOptions.sentenceMappings = options->get<bool>("sentence-mappings", false);
    responseOptions.alignmentThreshold = options->get<float>("sentence-mappings", 0.2f);

    std::string qualityScoreTypeKey = options->get<std::string>("quality-score-type", "free");

    if (qualityScoreTypeKey == "expensive"){
        responseOptions.qualityScoreType = QualityScoreType::EXPENSIVE;
    } else {
        responseOptions.qualityScoreType = QualityScoreType::FREE;
    }

    std::string concatStrategyKey = options->get<std::string>("concat-strategy", "faithful");
    if (concatStrategyKey == "space"){
        responseOptions.concatStrategy = ConcatStrategy::SPACE;
    
    } else {
        responseOptions.concatStrategy = ConcatStrategy::FAITHFUL;
    }

    return responseOptions;
  }
};

} // namespace bergamot
} // namespace marian

#endif //  SRC_BERGAMOT_RESPONSE_OPTIONS_H_
