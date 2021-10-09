#pragma once

#include <iostream>
#include <type_traits>

#include "response_options.h"
#include "service.h"
#include "translation_model.h"

namespace marian::bergamot {

std::string readFromStdin();

template <class Service>
struct TranslateForResponse : std::false_type {};

template <>
struct TranslateForResponse<AsyncService> {
  Response operator()(AsyncService &service, Ptr<TranslationModel> model, std::string source,
                      const ResponseOptions &responseOptions) {
    std::promise<Response> responsePromise;
    std::future<Response> responseFuture = responsePromise.get_future();

    auto callback = [&responsePromise](Response &&response) { responsePromise.set_value(std::move(response)); };
    service.translate(model, std::move(source), callback, responseOptions);

    responseFuture.wait();

    Response response = responseFuture.get();
    return response;
  }
};

template <>
struct TranslateForResponse<BlockingService> {
  Response operator()(BlockingService &service, Ptr<TranslationModel> model, std::string source,
                      const ResponseOptions &responseOptions) {
    std::vector<std::string> sources = {source};
    return service.translateMultiple(model, std::move(sources), responseOptions).front();
  }
};

}  // namespace marian::bergamot
