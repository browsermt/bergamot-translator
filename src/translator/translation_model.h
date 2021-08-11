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

/// A TranslationModel is associated with the translation of a single language direction. Holds the graph and other
/// structures required to run the forward pass of the neural network, along with preprocessing logic (TextProcessor)
/// and a BatchingPool to create batches that are to be used in conjuction with an instance.
///
/// Assuming parameters are provided correctly, the following is an example of use of API provided by this class to
/// complete translation:
///
/// ```cpp
/// TranslationModel model(...);
/// auto request = model.makeRequest(...);
/// model.enqueueRequest(request);
/// Batch batch;
/// while ( generateBatch(batch)) {
///      model.translateBatch(batch);
/// }
/// ```
///
/// Thread-safety is not handled here, but the methods are available at granularity enough to be used in threaded async
/// workflow for translation.

class TranslationModel {
 public:
  using Config = Ptr<Options>;
  /// Equivalent to options based constructor, where `options` is parsed from string configuration. Configuration can be
  /// JSON or YAML. Keys expected correspond to those of `marian-decoder`, available at
  /// https://marian-nmt.github.io/docs/cmd/marian-decoder/
  TranslationModel(const std::string& config, size_t replicas, MemoryBundle&& memory)
      : TranslationModel(parseOptionsFromString(config, /*validate=*/false), replicas, std::move(memory)){};

  /// Construct TranslationModel from marian-options. If memory is empty, TranslationModel is initialized from
  /// paths available in the options object, backed by filesystem. Otherwise, TranslationModel is initialized from the
  /// given MemoryBundle composed of AlignedMemory holding equivalent parameters.
  ///
  /// @param [in] options: Marian options object.
  /// @param [in] memory: MemoryBundle object holding memory buffers containing parameters to build MarianBackend,
  /// ShortlistGenerator, Vocabs and SentenceSplitter.
  TranslationModel(const Config& options, size_t replicas, MemoryBundle&& memory = MemoryBundle{});

  /// Make a Request to be translated by this TranslationModel instance.
  /// @param [in] requestId: Unique identifier associated with this request, available from Service.
  /// @param [in] source: Source text to be translated. Ownership is accepted and eventually returned to the client in
  /// Response corresponding to the Request created here.
  /// @param [in] callback: Callback (from client) to be issued upon completion of translation of all sentences in the
  /// created Request.
  /// @param [in] responseOptions: Configuration used to prepare the Response corresponding to the created request.
  //  @returns Request created from the query parameters wrapped within a shared-pointer.
  Ptr<Request> makeRequest(size_t requestId, std::string&& source, CallbackType callback,
                           const ResponseOptions& responseOptions);

  /// Relays a request to the batching-pool specific to this translation model.
  /// @param [in] request: Request constructed through makeRequest
  void enqueueRequest(Ptr<Request> request) { batchingPool_.enqueueRequest(request); };

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

 private:
  Config options_;
  MemoryBundle memory_;
  Vocabs vocabs_;
  TextProcessor textProcessor_;

  /// Maintains sentences from multiple requests bucketed by length and sorted by priority in each bucket.
  BatchingPool batchingPool_;

  /// A package of marian-entities which form a backend to translate.
  struct MarianBackend {
    using Graph = Ptr<ExpressionGraph>;
    using ScorerEnsemble = std::vector<Ptr<Scorer>>;
    using ShortlistGenerator = Ptr<data::ShortlistGenerator const>;

    Graph graph;
    ScorerEnsemble scorerEnsemble;
    ShortlistGenerator shortlistGenerator;
  };

  /// Hold replicas of the backend (graph, scorers, shortlist) for use in each thread.
  /// Controlled and consistent external access via graph(id), scorerEnsemble(id),
  std::vector<MarianBackend> backend_;

  void loadBackend(size_t idx);
};

}  // namespace bergamot
}  // namespace marian

#endif  //  SRC_BERGAMOT_TRANSLATION_MODEL_H_
