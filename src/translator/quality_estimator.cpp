#include "quality_estimator.h"

namespace marian {
namespace bergamot {
  Response QualityEstimator::updateQualityScores(){
    for (int sentenceIdx = 0; sentenceIdx < response_.size(); sentenceIdx++) {
        auto &quality = response_.qualityScores[sentenceIdx];
        quality.sequence = 0;
        for (auto &p : quality.word) {
          //UPDATE This to use ONNX Runtime; 
          //create new mthods if we want to provide vizualizations
          p = p*model.graph().node(1).attribute(1).floats(3)*(-1); 
        }
    }
    return response_;
  }
} // namespace bergamot
} // namespace marian