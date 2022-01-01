#include "common.h"
#include "translator/parser.h"
#include "translator/service.h"
#include "translator/translation_model.h"

using namespace marian::bergamot;

namespace marian::bergamot {
void concurrentMultimodelsIntensive(AsyncService &service, std::vector<Ptr<TranslationModel>> &models) {
  // We spawn models in their respective threads asynchronously queueing
  // numLines from WNGT20 sources.shuf.  Only supports from English models
  // therefore. Any number of models are supported by this
  // test/benchmark/demonstration app.
  //
  // Same service, multiple models already loaded, for a batch reused as
  // batches come in. What we hope for is that if the solution isn't correct we
  // will run into corrupt translations or something at some point.

  ABORT_IF(models.size() < 2, "Intensive test needs at least two models.");
  ResponseOptions responseOptions;
  const std::string source = readFromStdin();

  class ResponseWriter {
   public:
    ResponseWriter(size_t idx) : idx_(idx) {}

    void writeOriginal(const Response &response) {
      std::ofstream out(fname(/*original=*/true));
      out << response.target.text;
    }

    void writeThreaded(const std::vector<Response> &responses) {
      std::ofstream out(fname(/*original=*/false));
      for (auto &response : responses) {
        out << response.target.text;
      }
    }

    std::string fname(bool original) const {
      std::string filename = "model_" + std::to_string(idx_) + ((original) ? ".orig.txt" : ".threaded.txt");
      return filename;
    }

   private:
    size_t idx_;
  };

  // First we run one pass to get what is the expected output. There will be
  // variations when we operate in a threaded setting (maybe) due to difference
  // in batching and floating point approximations. Whoever uses this app has
  // to manually inspect the files for differences.
  for (size_t idx = 0; idx < models.size(); idx++) {
    std::string sourceCopied = source;
    std::promise<Response> p;
    auto f = p.get_future();
    auto callback = [&p](Response &&response) { p.set_value(std::move(response)); };

    service.translate(models[idx], std::move(sourceCopied), callback, responseOptions);
    Response response = f.get();
    ResponseWriter(idx).writeOriginal(response);
  }

  // Configurable to create volume, more requests in queue.
  constexpr size_t wngtTotalLines = 1000000;  // 1M lines.
  constexpr size_t numLinesInBatch = 40;
  constexpr size_t expectedNumResponses = (wngtTotalLines / numLinesInBatch) + 1;

  // Continuous queueing creates a stream and feeds batches into the service.
  class ContinuousQueuing {
   public:
    ContinuousQueuing(AsyncService &service, Ptr<TranslationModel> model, const std::string &source,
                      size_t linesAtATime, const ResponseOptions &responsesOptions_)
        : service_(service), model_(model), linesAtATime_(linesAtATime), sourceStream_(source){};

    void enqueue() {
      std::vector<std::string> chunks;
      std::string buffer;
      size_t linesStreamed;
      do {
        linesStreamed = readLines(sourceStream_, linesAtATime_, buffer);
        if (linesStreamed) {
          chunks.push_back(buffer);
          buffer.clear();
        }
      } while (linesStreamed > 0);

      LOG(info, "Obtained {} chunks from WNGT20", chunks.size());
      responseFutures_.resize(chunks.size());
      responsePromises_.resize(chunks.size());
      pending_ = chunks.size();

      for (size_t chunkId = 0; chunkId < chunks.size(); chunkId++) {
        responseFutures_[chunkId] = responsePromises_[chunkId].get_future();

        auto callback = [this, chunkId](Response &&response) {  //
          responsePromises_[chunkId].set_value(std::move(response));
        };

        service_.translate(model_, std::move(chunks[chunkId]), callback, responseOptions_);
      }
    }

    size_t readLines(std::stringstream &sourceStream, size_t numLines, std::string &buffer) {
      std::string line;
      size_t linesStreamed = 0;
      while (std::getline(sourceStream, line)) {
        buffer += line;
        buffer += "\n";
        linesStreamed++;
        if (linesStreamed == numLines) {
          break;
        }
      }
      return linesStreamed;
    }

    std::vector<Response> responses() {
      std::vector<Response> responses_;
      responses_.resize(responseFutures_.size());
      for (size_t idx = 0; idx < responseFutures_.size(); idx++) {
        responseFutures_[idx].wait();
        responses_[idx] = std::move(responseFutures_[idx].get());
      }
      return responses_;
    }

    ContinuousQueuing(const ContinuousQueuing &) = delete;
    ContinuousQueuing &operator=(const ContinuousQueuing &) = delete;

   private:
    std::stringstream sourceStream_;
    std::vector<std::future<Response>> responseFutures_;
    std::vector<std::promise<Response>> responsePromises_;
    const ResponseOptions responseOptions_;
    AsyncService &service_;
    Ptr<TranslationModel> model_;
    const size_t linesAtATime_;
    std::atomic<size_t> pending_;
  };

  std::vector<std::thread> cqFeeds;

  // Leave these parallely running, so interplay happens.
  //
  for (size_t idx = 0; idx < models.size(); idx++) {
    Ptr<TranslationModel> model = models[idx];
    cqFeeds.emplace_back([&service, model, source, numLinesInBatch, responseOptions, idx]() {
      ContinuousQueuing cq(service, model, source, numLinesInBatch, responseOptions);
      cq.enqueue();
      ResponseWriter(idx).writeThreaded(cq.responses());
    });
  }

  for (size_t idx = 0; idx < models.size(); idx++) {
    cqFeeds[idx].join();
  }
}
}  // namespace marian::bergamot

int main(int argc, char *argv[]) {
  ConfigParser<AsyncService> configParser("AsyncService test-suite", /*multiOpMode=*/true);
  configParser.parseArgs(argc, argv);
  auto &config = configParser.getConfig();

  AsyncService service(config.serviceConfig);

  std::vector<std::shared_ptr<TranslationModel>> models;

  for (auto &modelConfigPath : config.modelConfigPaths) {
    TranslationModel::Config modelConfig = parseOptionsFromFilePath(modelConfigPath);
    std::shared_ptr<TranslationModel> model = service.createCompatibleModel(modelConfig);
    models.push_back(model);
  }

  if (config.opMode == "test-multimodels-intensive") {
    marian::bergamot::concurrentMultimodelsIntensive(service, models);
  } else {
    TestSuite<AsyncService> testSuite(service);
    testSuite.run(config.opMode, models);
  }

  return 0;
}
