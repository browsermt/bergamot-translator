#ifndef SRC_BERGAMOT_TRANSLATION_MODEL_H_
#define SRC_BERGAMOT_TRANSLATION_MODEL_H_

#include <string>
#include <vector>

#include "batch.h"
#include "batching_pool.h"
#include "byte_array_util.h"
#include "cache.h"
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

/// A TranslationModel is associated with the translation of a single language direction. Holds the graph and other
/// structures required to run the forward pass of the neural network, along with preprocessing logic (TextProcessor)
/// and a BatchingPool to create batches that are to be used in conjuction with an instance.
///
/// Thread-safety is not handled here, but the methods are available at granularity enough to be used in threaded async
/// workflow for translation.

class TranslationModel {
 public:
  using Config = Ptr<Options>;
  using ShortlistGenerator = Ptr<data::ShortlistGenerator const>;

  /// Equivalent to options based constructor, where `options` is parsed from string configuration. Configuration can be
  /// JSON or YAML. Keys expected correspond to those of `marian-decoder`, available at
  /// https://marian-nmt.github.io/docs/cmd/marian-decoder/
  ///
  /// Note that `replicas` is not stable. This is a temporary workaround while a more daunting task of separating
  /// workspace from TranslationModel and binding it to threads is to be undertaken separately. Until the separation is
  /// achieved, both TranslationModel and Service will need to be aware of workers. This is expected to be resolved
  /// eventually, with only Service having the knowledge of how many workers are active.
  ///
  /// WebAssembly uses only single-thread, and we can hardcode replicas = 1 and use it anywhere and (client) needn't be
  /// aware of this ugliness at the moment, thus providing a stable API solely for WebAssembly single-threaded modus
  /// operandi.
  ///
  /// TODO(@jerinphilip): Clean this up.
  TranslationModel(const std::string& config, MemoryBundle&& memory, size_t replicas = 1, std::vector<size_t> gpus = {})
      : TranslationModel(parseOptionsFromString(config, /*validate=*/false), std::move(memory), replicas, gpus){};

  /// Construct TranslationModel from marian-options. If memory is empty, TranslationModel is initialized from
  /// paths available in the options object, backed by filesystem. Otherwise, TranslationModel is initialized from the
  /// given MemoryBundle composed of AlignedMemory holding equivalent parameters.
  ///
  /// @param [in] options: Marian options object.
  /// @param [in] memory: MemoryBundle object holding memory buffers containing parameters to build MarianBackend,
  /// ShortlistGenerator, Vocabs and SentenceSplitter.
  /// @param [in] gpus: Optional array of GPU ids
  TranslationModel(const Config& options, MemoryBundle&& memory, size_t replicas = 1, std::vector<size_t> gpus = {});

  TranslationModel(const Config& options, size_t replicas = 1, std::vector<size_t> gpus = {})
      : TranslationModel(options, getMemoryBundleFromConfig(options), replicas, gpus) {}

  /// Make a Request to be translated by this TranslationModel instance.
  /// @param [in] requestId: Unique identifier associated with this request, available from Service.
  /// @param [in] source: Source text to be translated. Ownership is accepted and eventually returned to the client in
  /// Response corresponding to the Request created here.
  /// @param [in] callback: Callback (from client) to be issued upon completion of translation of all sentences in the
  /// created Request.
  /// @param [in] responseOptions: Configuration used to prepare the Response corresponding to the created request.
  //  @returns Request created from the query parameters wrapped within a shared-pointer.
  Ptr<Request> makeRequest(size_t requestId, std::string&& source, CallbackType callback,
                           const ResponseOptions& responseOptions, std::optional<TranslationCache>& cache);

  Ptr<Request> makePivotRequest(size_t requestId, AnnotatedText&& previousTarget, CallbackType callback,
                                const ResponseOptions& responseOptions, std::optional<TranslationCache>& cache);

  /// Relays a request to the batching-pool specific to this translation model.
  /// @param [in] request: Request constructed through makeRequest
  size_t enqueueRequest(Ptr<Request> request) { return batchingPool_.enqueueRequest(request); };

  /// Generates a batch from the batching-pool for this translation model, compiling from several active requests. Note
  /// that it is possible that calls to this method can give empty-batches.
  ///
  /// @param [out] batch: Batch to write a generated batch on to.
  /// @returns number of sentences that constitute the Batch.
  size_t generateBatch(Batch& batch) { return batchingPool_.generateBatch(batch); }

  /// Translate a batch generated with generateBatch
  ///
  /// @param [in] deviceId: There are replicas of backend created for use in each worker thread. deviceId indicates
  /// which replica to use.
  /// @param [in] batch: A batch generated from generateBatch from the same TranslationModel instance.
  void translateBatch(size_t deviceId, Batch& batch);

  /// Returns a unique-identifier for the model.
  size_t modelId() const { return modelId_; }

 private:
  size_t modelId_;
  Config options_;
  MemoryBundle memory_;
  Vocabs vocabs_;
  TextProcessor textProcessor_;
  std::vector<size_t> gpus_;

  /// Maintains sentences from multiple requests bucketed by length and sorted by priority in each bucket.
  BatchingPool batchingPool_;

  /// A package of marian-entities which form a backend to translate.
  struct MarianBackend {
    using Graph = Ptr<ExpressionGraph>;
    using ScorerEnsemble = std::vector<Ptr<Scorer>>;

    Graph graph;
    ScorerEnsemble scorerEnsemble;
    bool initialized{false};
  };

  // ShortlistGenerator is purely const, we don't need one per thread.
  ShortlistGenerator shortlistGenerator_;

  /// Hold replicas of the backend (graph, scorers, shortlist) for use in each thread.
  /// Controlled and consistent external access via graph(id), scorerEnsemble(id),
  std::vector<MarianBackend> backend_;
  std::shared_ptr<QualityEstimator> qualityEstimator_;

  void loadBackend(size_t idx);
  Ptr<marian::data::CorpusBatch> convertToMarianBatch(Batch& batch);

  static std::atomic<size_t> modelCounter_;
};

}  // namespace bergamot
}  // namespace marian

#endif  //  SRC_BERGAMOT_TRANSLATION_MODEL_H_
