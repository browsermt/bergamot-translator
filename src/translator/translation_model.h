#ifndef SRC_BERGAMOT_TRANSLATION_MODEL_H_
#define SRC_BERGAMOT_TRANSLATION_MODEL_H_

#include <string>
#include <vector>

#include "batch.h"
#include "batching_pool.h"
#include "common/utils.h"
#include "data/shortlist.h"
#include "definitions.h"
#include "parser.h"
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

  /// Construct TranslationModel from a string configuration. If memoryBundle is empty, TranslationModel is
  /// initialized from file-based loading. Otherwise, TranslationModel is initialized from
  /// the given bytearray memories.
  /// @param [in] config string parsable as YAML expected to adhere with marian config
  /// model, shortlist, vocabs and ssplitPrefixFile bytes. Optional.
  /// @param [in] MemoryBundle object holding memory buffers containing model, shortlist, vocabs and ssplitPrefixFile
  /// bytes. Optional.
  TranslationModel(const std::string& config, MemoryBundle&& memory)
      : TranslationModel(parseOptions(config, /*validate=*/false), std::move(memory), /*replicas=*/1){};

  /// Construct TranslationModel from Marian options. If memoryBundle is empty, TranslationModel is
  /// initialized from file-based loading. Otherwise, TranslationModel is initialized from
  /// the given bytearray memories.
  /// @param [in] options Marian options object
  /// @param [in] memory: MemoryBundle object holding memory buffers containing model, shortlist, vocabs and
  /// ssplitPrefixFile bytes. Optional.
  TranslationModel(Ptr<Options> options, MemoryBundle&& memory, size_t replicas = 1);

  void addRequest(Ptr<Request> request) { batchingPool_.addRequest(request); };
  bool generateBatch(Batch& batch) { return batchingPool_.generateBatch(batch); }

  const Graph& graph(size_t id) const { return backend_[id].graph; }
  const ScorerEnsemble& scorerEnsemble(size_t id) const { return backend_[id].scorerEnsemble; }
  const ShortlistGenerator& shortlistGenerator(size_t id) const { return backend_[id].shortlistGenerator; }
  const Ptr<Options> options() const { return options_; }
  const Vocabs& vocabs() const { return vocabs_; }
  const TextProcessor& textProcessor() const { return textProcessor_; }

 private:
  Ptr<Options> options_;  // This needn't be held?
  MemoryBundle memory_;
  Vocabs vocabs_;

  TextProcessor textProcessor_;

  /// Maintains sentences from multiple requests bucketed by length and sorted by priority in each bucket.
  BatchingPool batchingPool_;

  struct MarianBackend {
    // 1. Construction:
    //    https://github.com/marian-nmt/marian-dev/blob/42f0b8b74bba16fed646c8af7b2f75e02af7a85c/src/translator/translator.h#L90-L120
    // 2. Inference:
    //    https://github.com/marian-nmt/marian-dev/blob/42f0b8b74bba16fed646c8af7b2f75e02af7a85c/src/translator/translator.h#L181
    //
    // Therefore, believing the following needs to be replicated for each thread from the above example.

    Graph graph;

    // Marian has an ensemble of scorers in it's API. For most bergamot purposes this is a vector with a single element
    // and is a pseudo-ensemble to fit the API.
    ScorerEnsemble scorerEnsemble;

    ShortlistGenerator shortlistGenerator;
  };

  // Hold replicas of the backend (graph, scorers, shortlist) for use in each thread.
  // Controlled and consistent external access via graph(id), scorerEnsemble(id),
  std::vector<MarianBackend> backend_;

  void loadBackend(size_t idx);
};

void translateBatch(size_t deviceId, Ptr<TranslationModel> model, Batch& batch);

}  // namespace bergamot
}  // namespace marian

#endif  //  SRC_BERGAMOT_TRANSLATION_MODEL_H_
