#ifndef SRC_BERGAMOT_BATCH_TRANSLATOR_H_
#define SRC_BERGAMOT_BATCH_TRANSLATOR_H_

#include <string>
#include <vector>

#include "batch.h"
#include "common/utils.h"
#include "data/shortlist.h"
#include "definitions.h"
#include "request.h"
#include "text_processor.h"
#include "translator/history.h"
#include "translator/scorers.h"
#include "vocabs.h"

namespace marian {
namespace bergamot {

class TranslationModel {
 public:
  using Graph = Ptr<ExpressionGraph>;
  using ScorerEnsemble = std::vector<Ptr<Scorer>>;
  using ShortlistGenerator = Ptr<data::ShortlistGenerator const>;
  TranslationModel(Ptr<Options> options, MemoryBundle&& memory, size_t replicas = 1);

  const Graph& graph(size_t id = 0) const { return backend_[id].graph; }
  const ScorerEnsemble& scorerEnsemble(size_t id = 0) { return backend_[id].scorerEnsemble; }
  const ShortlistGenerator& shortlistGenerator(size_t id = 0) { return backend_[id].shortlistGenerator; }
  const Ptr<Options> options() { return options_; }
  const Vocabs& vocabs() { return vocabs_; }
  TextProcessor& textProcessor() { return textProcessor_; }

 private:
  struct MarianBackend {
    // 1. Construction:
    //    https://github.com/marian-nmt/marian-dev/blob/42f0b8b74bba16fed646c8af7b2f75e02af7a85c/src/translator/translator.h#L90-L120
    // 2. Inference:
    //    https://github.com/marian-nmt/marian-dev/blob/42f0b8b74bba16fed646c8af7b2f75e02af7a85c/src/translator/translator.h#L181
    //
    // Therefore, the following needs to be replicated for each thread from the above example.

    Graph graph;
    ScorerEnsemble scorerEnsemble;
    ShortlistGenerator shortlistGenerator;
  };

  Ptr<Options> options_;
  MemoryBundle memory_;
  Vocabs vocabs_;
  TextProcessor textProcessor_;
  std::vector<MarianBackend> backend_;
  // TODO: QualityEstimator qualityEstimator_;
};

void translateBatch(size_t deviceId, TranslationModel& model, Batch& batch);

}  // namespace bergamot
}  // namespace marian

#endif  //  SRC_BERGAMOT_BATCH_TRANSLATOR_H_
