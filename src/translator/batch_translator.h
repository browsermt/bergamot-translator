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

void translate(size_t deviceId, TranslationModel& model, const Batch& batch);

class BatchTranslator {
  // Launches minimal marian-translation (only CPU at the moment) in individual
  // threads. Constructor launches each worker thread running mainloop().
  // mainloop runs until until it receives poison from the PCQueue. Threads are
  // shut down in Service which calls join() on the threads.

 public:
  /**
   * Initialise the marian translator.
   * @param device DeviceId that performs translation. Could be CPU or GPU
   * @param vocabs Vector that contains ptrs to two vocabs
   * @param options Marian options object
   * @param modelMemory byte array (aligned to 256!!!) that contains the bytes of a model.bin. Provide a nullptr if not
   * used.
   * @param shortlistMemory byte array of shortlist (aligned to 64)
   */
  explicit BatchTranslator(DeviceId const device, Vocabs& vocabs, Ptr<Options> options,
                           const AlignedMemory* modelMemory, const AlignedMemory* shortlistMemory);

  // convenience function for logging. TODO(jerin)
  std::string _identifier() { return "worker" + std::to_string(device_.no); }
  void translate(Batch& batch);
  void initialize();

 private:
  Ptr<Options> options_;
  DeviceId device_;
  const Vocabs& vocabs_;
  Ptr<ExpressionGraph> graph_;
  std::vector<Ptr<Scorer>> scorers_;
  Ptr<data::ShortlistGenerator const> slgen_;
  const AlignedMemory* modelMemory_{nullptr};
  const AlignedMemory* shortlistMemory_{nullptr};
};

}  // namespace bergamot
}  // namespace marian

#endif  //  SRC_BERGAMOT_BATCH_TRANSLATOR_H_
