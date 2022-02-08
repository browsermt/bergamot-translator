
#ifndef BERGAMOT_TESTS_COMMON_IMPL
#error "This is an impl file and must not be included directly!"
#endif

Response Bridge<BlockingService>::translate(BlockingService &service, std::shared_ptr<TranslationModel> &model,
                                            std::string &&source, const ResponseOptions &responseOptions) {
  // project source to a vector of std::string, send in, unpack the first element from
  // vector<Response>, return.
  std::vector<std::string> sources = {source};
  Response response = std::move(service.translateMultiple(model, std::move(sources), responseOptions).front());
  ABORT_IF(!response.ok(), "Error in response: {}", response.error);
  return response;
}

Response Bridge<AsyncService>::translate(AsyncService &service, std::shared_ptr<TranslationModel> &model,
                                         std::string &&source, const ResponseOptions &responseOptions) {
  // downgrade to blocking via promise, future, wait and return response;
  std::promise<Response> responsePromise;
  std::future<Response> responseFuture = responsePromise.get_future();

  auto callback = [&responsePromise](Response &&response) { responsePromise.set_value(std::move(response)); };
  service.translate(model, std::move(source), callback, responseOptions);

  responseFuture.wait();

  Response response = responseFuture.get();
  ABORT_IF(!response.ok(), "Error in response: {}", response.error);
  return response;
}

Response Bridge<BlockingService>::pivot(BlockingService &service, std::shared_ptr<TranslationModel> &sourceToPivot,
                                        std::shared_ptr<TranslationModel> &pivotToTarget, std::string &&source,
                                        const ResponseOptions &responseOptions) {
  std::vector<std::string> sources = {source};
  return service.pivotMultiple(sourceToPivot, pivotToTarget, std::move(sources), responseOptions).front();
}

Response Bridge<AsyncService>::pivot(AsyncService &service, std::shared_ptr<TranslationModel> &sourceToPivot,
                                     std::shared_ptr<TranslationModel> &pivotToTarget, std::string &&source,
                                     const ResponseOptions &responseOptions) {
  std::promise<Response> responsePromise;
  std::future<Response> responseFuture = responsePromise.get_future();

  auto callback = [&responsePromise](Response &&response) { responsePromise.set_value(std::move(response)); };
  service.pivot(sourceToPivot, pivotToTarget, std::move(source), callback, responseOptions);
  responseFuture.wait();
  Response response = responseFuture.get();
  return response;
}

template <class Service>
TestSuite<Service>::TestSuite(Service &service) : service_{service} {}

template <class Service>
void TestSuite<Service>::TestSuite::run(const std::string &opModeAsString, std::vector<Ptr<TranslationModel>> &models) {
  if (opModeAsString == "decoder") {
    benchmarkDecoder(models.front());
  } else if (opModeAsString == "test-response-source-sentences") {
    annotatedTextSentences(models.front(), /*source=*/true);
  } else if (opModeAsString == "test-response-target-sentences") {
    annotatedTextSentences(models.front(), /*source=*/false);
  } else if (opModeAsString == "test-response-source-words") {
    annotatedTextWords(models.front(), /*source=*/true);
  } else if (opModeAsString == "test-response-target-words") {
    annotatedTextWords(models.front(), /*source=*/false);
  } else if (opModeAsString == "test-forward-backward") {
    forwardAndBackward(models);
  } else if (opModeAsString == "test-quality-estimator-words") {
    qualityEstimatorWords(models.front());
  } else if (opModeAsString == "test-quality-estimator-scores") {
    qualityEstimatorScores(models.front());
  } else if (opModeAsString == "test-translation-cache") {
    translationCache(models.front());
  } else if (opModeAsString == "test-pivot") {
    pivotTranslate(models);
  } else if (opModeAsString == "test-html-translation") {
    htmlTranslation(models.front());
  } else {
    std::cerr << "Incompatible test mode. Choose from the one of the valid test-modes";
    std::abort();
  }
}

template <class Service>
void TestSuite<Service>::benchmarkDecoder(Ptr<TranslationModel> &model) {
  marian::timer::Timer decoderTimer;
  std::string source = readFromStdin();

  ResponseOptions responseOptions;
  Response response = bridge_.translate(service_, model, std::move(source), responseOptions);

  for (size_t sentenceIdx = 0; sentenceIdx < response.size(); sentenceIdx++) {
    std::cout << response.target.sentence(sentenceIdx) << "\n";
  }

  std::cerr << "Total time: " << std::setprecision(5) << decoderTimer.elapsed() << "s wall" << std::endl;
}

// Reads from stdin and translates.  Prints the tokens separated by space for each sentence. Prints words from source
// side text annotation if source=true, target annotation otherwise.
template <class Service>
void TestSuite<Service>::annotatedTextWords(Ptr<TranslationModel> model, bool sourceSide /*=true*/) {
  ResponseOptions responseOptions;
  std::string source = readFromStdin();
  Response response = bridge_.translate(service_, model, std::move(source), responseOptions);
  AnnotatedText &annotatedText = sourceSide ? response.source : response.target;
  for (size_t s = 0; s < annotatedText.numSentences(); s++) {
    for (size_t w = 0; w < annotatedText.numWords(s); w++) {
      std::cout << (w == 0 ? "" : "\t");
      std::cout << annotatedText.word(s, w);
    }
    std::cout << "\n";
  }
}

// Reads from stdin and translates the read content. Prints the sentences in source or target in constructed response
// in each line, depending on source = true or false respectively.
template <class Service>
void TestSuite<Service>::annotatedTextSentences(Ptr<TranslationModel> model, bool sourceSide /*=true*/) {
  ResponseOptions responseOptions;
  std::string source = readFromStdin();
  Response response = bridge_.translate(service_, model, std::move(source), responseOptions);
  AnnotatedText &annotatedText = sourceSide ? response.source : response.target;
  for (size_t s = 0; s < annotatedText.numSentences(); s++) {
    std::cout << annotatedText.sentence(s) << "\n";
  }
}

template <class Service>
void TestSuite<Service>::forwardAndBackward(std::vector<Ptr<TranslationModel>> &models) {
  ABORT_IF(models.size() != 2, "Forward and backward test needs two models.");
  ResponseOptions responseOptions;
  std::string source = readFromStdin();
  Response forwardResponse = bridge_.translate(service_, models.front(), std::move(source), responseOptions);

  // Make a copy of target
  std::string target = forwardResponse.target.text;
  Response backwardResponse = bridge_.translate(service_, models.back(), std::move(target), responseOptions);

  // Print both onto the command-line
  std::cout << forwardResponse.source.text;
  std::cout << "----------------\n";
  std::cout << forwardResponse.target.text;
  std::cout << "----------------\n";
  std::cout << backwardResponse.target.text;
}

// Reads from stdin and translates the read content. Prints the quality words for each sentence.
template <class Service>
void TestSuite<Service>::qualityEstimatorWords(Ptr<TranslationModel> model) {
  ResponseOptions responseOptions;
  responseOptions.qualityScores = true;
  std::string source = readFromStdin();
  const Response response = bridge_.translate(service_, model, std::move(source), responseOptions);

  for (const auto &sentenceQualityEstimate : response.qualityScores) {
    std::cout << "[SentenceBegin]\n";

    for (const auto &wordByteRange : sentenceQualityEstimate.wordByteRanges) {
      const string_view word(response.target.text.data() + wordByteRange.begin, wordByteRange.size());
      std::cout << word << "\n";
    }
    std::cout << "[SentenceEnd]\n\n";
  }
}

template <class Service>
void TestSuite<Service>::htmlTranslation(Ptr<TranslationModel> model) {
  ResponseOptions responseOptions;
  responseOptions.HTML = true;
  responseOptions.alignment = true;
  std::string source = readFromStdin();
  const Response response = bridge_.translate(service_, model, std::move(source), responseOptions);

  std::cout << response.target.text;
}

// Reads from stdin and translates the read content. Prints the quality scores for each sentence.
template <class Service>
void TestSuite<Service>::qualityEstimatorScores(Ptr<TranslationModel> model) {
  ResponseOptions responseOptions;
  responseOptions.qualityScores = true;

  std::string source = readFromStdin();
  const Response response = bridge_.translate(service_, model, std::move(source), responseOptions);

  for (const auto &sentenceQualityEstimate : response.qualityScores) {
    std::cout << std::fixed << std::setprecision(3) << sentenceQualityEstimate.sentenceScore << "\n";

    for (const float &wordScore : sentenceQualityEstimate.wordScores) {
      std::cout << std::fixed << std::setprecision(3) << wordScore << "\n";
    }
    std::cout << "\n";
  }
}

template <class Service>
void TestSuite<Service>::translationCache(Ptr<TranslationModel> model) {
  ResponseOptions responseOptions;

  // Read a large input text blob from stdin
  const std::string source = readFromStdin();

  // Round 1
  std::string buffer = source;
  Response firstResponse = bridge_.translate(service_, model, std::move(buffer), responseOptions);

  auto statsFirstRun = service_.cacheStats();
  LOG(info, "Cache Hits/Misses = {}/{}", statsFirstRun.hits, statsFirstRun.misses);
  ABORT_IF(statsFirstRun.hits != 0, "Expecting no cache hits, but hits found.");

  // Round 2; There should be cache hits
  buffer = source;
  Response secondResponse = bridge_.translate(service_, model, std::move(buffer), responseOptions);

  auto statsSecondRun = service_.cacheStats();
  LOG(info, "Cache Hits/Misses = {}/{}", statsSecondRun.hits, statsSecondRun.misses);
  ABORT_IF(statsSecondRun.hits <= 0, "At least one hit expected, none found.");
  if (statsSecondRun.hits != statsFirstRun.misses) {
    std::cerr << "Mismatch in expected hits (Hits, Misses = " << statsSecondRun.hits << ", " << statsSecondRun.misses
              << "). This can happen due to random eviction." << std::endl;
  }

  ABORT_IF(firstResponse.target.text != secondResponse.target.text,
           "Recompiled string provided different output when operated with cache. On the same hardware while using "
           "same path, this is expected to be same.");

  std::cout << firstResponse.target.text;
}

template <class Service>
void TestSuite<Service>::pivotTranslate(std::vector<Ptr<TranslationModel>> &models) {
  // We expect a source -> pivot; pivot -> source model to get source -> source and build this test using accuracy of
  // matches.
  ABORT_IF(models.size() != 2, "Forward and backward test needs two models.");
  ResponseOptions responseOptions;
  responseOptions.alignment = true;
  std::string source = readFromStdin();
  std::promise<Response> responsePromise;
  std::future<Response> responseFuture = responsePromise.get_future();

  Response response = bridge_.pivot(service_, models.front(), models.back(), std::move(source), responseOptions);

  const float EPS = 1e-5;
  size_t totalOutcomes = 0;
  size_t favourableOutcomes = 0;

  for (size_t sentenceId = 0; sentenceId < response.source.numSentences(); sentenceId++) {
    std::cout << "> " << response.source.sentence(sentenceId) << "\n";
    std::cout << "< " << response.target.sentence(sentenceId) << "\n\n";

    // Assert what we have is a probability distribution over source-tokens given a target token.
    for (size_t t = 0; t < response.alignments[sentenceId].size(); t++) {
      float sum = 0.0f;
      for (size_t s = 0; s < response.alignments[sentenceId][t].size(); s++) {
        sum += response.alignments[sentenceId][t][s];
      }

      std::cerr << fmt::format("Sum @ (target-token = {}, sentence = {}) = {}", t, sentenceId, sum) << std::endl;
      ABORT_IF((std::abs(sum - 1.0f) > EPS), "Not a probability distribution, something's going wrong");
    }

    // For each target token, find argmax s, i.e find argmax p(s | t), max p(s | t)
    for (size_t t = 0; t < response.alignments[sentenceId].size(); t++) {
      bool valid = false;
      float maxV = 0.0f;
      auto argmaxV = std::make_pair(-1, -1);
      for (size_t s = 0; s < response.alignments[sentenceId][t].size(); s++) {
        auto v = response.alignments[sentenceId][t][s];
        if (v > maxV) {
          maxV = v;
          argmaxV = std::make_pair(t, s);
        }
      }

      auto sourceToken = response.source.word(sentenceId, argmaxV.second);
      auto targetToken = response.target.word(sentenceId, argmaxV.first);
      if (sourceToken == targetToken) {
        favourableOutcomes += 1;
      }

      std::cerr << sourceToken << " " << targetToken << " " << maxV << std::endl;

      totalOutcomes += 1;
    }

    // Assert each alignment over target is a valid probability distribution.
  }

  // Measure accuracy of word match.
  float accuracy = static_cast<float>(favourableOutcomes) / static_cast<float>(totalOutcomes);

  // This is arbitrary value chosen by @jerinphilip, but should be enough to check if things fail.
  // This value is calibrated on bergamot input in BRT. All this is supposed to do is let the developers know if
  // something's largely amiss to the point alignments are not working.
  ABORT_IF(accuracy < 0.70, "Accuracy {} not enough. Please check if something's off.", accuracy * 100);

  std::cout << response.source.text;
  std::cout << response.target.text;
}
