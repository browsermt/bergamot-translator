#include "common.h"
#include "translator/parser.h"
#include "translator/service.h"
#include "translator/translation_model.h"

using namespace marian::bergamot;

namespace marian::bergamot {

void concurrentMultimodels(AsyncService &service, std::vector<Ptr<TranslationModel>> &models) {
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

  auto fname = [](size_t idx, bool original) -> std::string {
    std::string filename = "model_" + std::to_string(idx) + ((original) ? ".orig.txt" : ".threaded.txt");
    return filename;
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
    std::ofstream out(fname(idx, /*original=*/true));
    out << response.target.text;
  }

  // Configurable to create volume, more requests in queue.
  constexpr size_t wngtTotalLines = 1000000;  // 1M lines.
  constexpr size_t numLinesInBatch = 40;
  constexpr size_t expectedNumResponses = (wngtTotalLines / numLinesInBatch) + 1;

  auto readLines = [](std::stringstream &sourceStream, size_t numLines, std::string &buffer) {
    std::string line;
    size_t linesStreamed = 0;
    while (linesStreamed < numLines && std::getline(sourceStream, line)) {
      buffer += line + "\n";
      linesStreamed++;
    }
    return linesStreamed;
  };

  // Stream lines into partitions
  std::stringstream sourceStream_(source);
  using Partitions = std::vector<std::string>;
  Partitions partitions;

  std::string buffer;
  size_t linesStreamed;
  do {
    linesStreamed = readLines(sourceStream_, numLinesInBatch, buffer);
    if (linesStreamed) {
      partitions.push_back(buffer);
      buffer.clear();
    }
  } while (linesStreamed > 0);

  LOG(info, "Obtained {} partitions from WNGT20", partitions.size());

  // Translate partitions concurrently in separate threads.
  std::vector<std::thread> threads;
  for (size_t idx = 0; idx < models.size(); idx++) {
    Ptr<TranslationModel> model = models[idx];
    std::string outFileName = fname(idx, /*original=*/false);

    threads.emplace_back([&service, model, partitions, responseOptions, outFileName]() {
      std::vector<Response> responses;

      std::vector<std::promise<Response>> promises;
      std::vector<std::future<Response>> futures;
      // Both need to have the required size.
      size_t count = partitions.size();
      promises.resize(count);
      futures.resize(count);

      for (size_t partitionId = 0; partitionId < count; partitionId++) {
        futures[partitionId] = promises[partitionId].get_future();

        auto callback = [partitionId, &promises](Response &&response) {  //
          promises[partitionId].set_value(std::move(response));
        };

        std::string copy = partitions[partitionId];
        service.translate(model, std::move(copy), callback, responseOptions);
      }

      for (auto &fut : futures) {
        responses.push_back(std::move(fut.get()));
      }

      // Write out the obtained responses when ready.
      std::ofstream out(outFileName);
      for (auto &response : responses) {
        out << response.target.text;
      }
    });
  }

  for (size_t idx = 0; idx < models.size(); idx++) {
    threads[idx].join();
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
    marian::bergamot::concurrentMultimodels(service, models);
  } else {
    TestSuite<AsyncService> testSuite(service);
    testSuite.run(config.opMode, models);
  }

  return 0;
}
