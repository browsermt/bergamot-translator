#include "apps.h"

#include <fstream>
#include <sstream>
#include <thread>

namespace marian {
namespace bergamot {

namespace {

std::string readFromStdin() {
  // Read a large input text blob from stdin
  std::ostringstream inputStream;
  inputStream << std::cin.rdbuf();
  std::string input = inputStream.str();
  return input;
}

// Utility function, common for all testapps.
Response translateForResponse(AsyncService &service, Ptr<TranslationModel> model, std::string &&source,
                              ResponseOptions responseOptions) {
  std::promise<Response> responsePromise;
  std::future<Response> responseFuture = responsePromise.get_future();

  auto callback = [&responsePromise](Response &&response) { responsePromise.set_value(std::move(response)); };
  service.translate(model, std::move(source), callback, responseOptions);

  responseFuture.wait();

  Response response = responseFuture.get();
  return response;
}

}  // namespace

namespace testapp {

void annotatedTextWords(AsyncService &service, Ptr<TranslationModel> model, bool sourceSide) {
  ResponseOptions responseOptions;
  std::string source = readFromStdin();
  Response response = translateForResponse(service, model, std::move(source), responseOptions);
  AnnotatedText &annotatedText = sourceSide ? response.source : response.target;
  for (size_t s = 0; s < annotatedText.numSentences(); s++) {
    for (size_t w = 0; w < annotatedText.numWords(s); w++) {
      std::cout << (w == 0 ? "" : "\t");
      std::cout << annotatedText.word(s, w);
    }
    std::cout << "\n";
  }
}

void annotatedTextSentences(AsyncService &service, Ptr<TranslationModel> model, bool sourceSide) {
  ResponseOptions responseOptions;
  std::string source = readFromStdin();
  Response response = translateForResponse(service, model, std::move(source), responseOptions);
  AnnotatedText &annotatedText = sourceSide ? response.source : response.target;
  for (size_t s = 0; s < annotatedText.numSentences(); s++) {
    std::cout << annotatedText.sentence(s) << "\n";
  }
}

void forwardAndBackward(AsyncService &service, std::vector<Ptr<TranslationModel>> &models) {
  ABORT_IF(models.size() != 2, "Forward and backward test needs two models.");
  ResponseOptions responseOptions;
  std::string source = readFromStdin();
  Response forwardResponse = translateForResponse(service, models.front(), std::move(source), responseOptions);

  // Make a copy of target
  std::string target = forwardResponse.target.text;
  Response backwardResponse = translateForResponse(service, models.back(), std::move(target), responseOptions);

  // Print both onto the command-line
  std::cout << forwardResponse.source.text;
  std::cout << "----------------\n";
  std::cout << forwardResponse.target.text;
  std::cout << "----------------\n";
  std::cout << backwardResponse.target.text;
}

void qualityEstimatorWords(AsyncService &service, Ptr<TranslationModel> model) {
  ResponseOptions responseOptions;
  responseOptions.qualityScores = true;
  std::string source = readFromStdin();
  const Response response = translateForResponse(service, model, std::move(source), responseOptions);

  for (const auto &sentenceQualityEstimate : response.qualityScores) {
    std::cout << "[SentenceBegin]\n";

    for (const auto &wordByteRange : sentenceQualityEstimate.wordByteRanges) {
      const string_view word(response.target.text.data() + wordByteRange.begin, wordByteRange.size());
      std::cout << word << "\n";
    }
    std::cout << "[SentenceEnd]\n\n";
  }
}

void qualityEstimatorScores(AsyncService &service, Ptr<TranslationModel> model) {
  ResponseOptions responseOptions;
  responseOptions.qualityScores = true;

  std::string source = readFromStdin();
  const Response response = translateForResponse(service, model, std::move(source), responseOptions);

  for (const auto &sentenceQualityEstimate : response.qualityScores) {
    std::cout << std::fixed << std::setprecision(3) << sentenceQualityEstimate.sentenceScore << "\n";

    for (const float &wordScore : sentenceQualityEstimate.wordScores) {
      std::cout << std::fixed << std::setprecision(3) << wordScore << "\n";
    }
    std::cout << "\n";
  }
}

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
    Response response = translateForResponse(service, models[idx], std::move(sourceCopied), responseOptions);
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

}  // namespace testapp
}  // namespace bergamot
}  // namespace marian
