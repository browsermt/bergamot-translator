#include <pybind11/iostream.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <translator/annotation.h>
#include <translator/parser.h>
#include <translator/project_version.h>
#include <translator/response.h>
#include <translator/response_options.h>
#include <translator/service.h>
#include <translator/translation_model.h>

#include <iostream>
#include <string>
#include <vector>

namespace py = pybind11;

using marian::bergamot::AnnotatedText;
using marian::bergamot::ByteRange;
using marian::bergamot::ConcatStrategy;
using marian::bergamot::Response;
using marian::bergamot::ResponseOptions;
using Service = marian::bergamot::AsyncService;
using _Model = marian::bergamot::TranslationModel;
using Model = std::shared_ptr<_Model>;
using Alignment = std::vector<std::vector<float>>;
using Alignments = std::vector<Alignment>;

PYBIND11_MAKE_OPAQUE(std::vector<Response>);
PYBIND11_MAKE_OPAQUE(std::vector<std::string>);
PYBIND11_MAKE_OPAQUE(Alignments);

class ServicePyAdapter {
 public:
  ServicePyAdapter(const Service::Config &config) : service_(make_service(config)) {
    // Set marian to throw exceptions instead of std::abort()
    marian::setThrowExceptionOnAbort(true);
  }

  std::shared_ptr<_Model> modelFromConfig(const std::string &config) {
    auto parsedConfig = marian::bergamot::parseOptionsFromString(config);
    return service_.createCompatibleModel(parsedConfig);
  }

  std::shared_ptr<_Model> modelFromConfigPath(const std::string &configPath) {
    auto config = marian::bergamot::parseOptionsFromFilePath(configPath);
    return service_.createCompatibleModel(config);
  }

  std::vector<Response> translate(Model model, std::vector<std::string> &inputs, const ResponseOptions &options) {
    py::scoped_ostream_redirect outstream(std::cout,                                 // std::ostream&
                                          py::module_::import("sys").attr("stdout")  // Python output
    );
    py::scoped_ostream_redirect errstream(std::cerr,                                 // std::ostream&
                                          py::module_::import("sys").attr("stderr")  // Python output
    );

    py::call_guard<py::gil_scoped_release> gil_guard;

    // Prepare promises, save respective futures. Have callback's in async set
    // value to the promises.
    std::vector<std::future<Response>> futures;
    std::vector<std::promise<Response>> promises;
    promises.resize(inputs.size());

    for (size_t i = 0; i < inputs.size(); i++) {
      auto callback = [&promises, i](Response &&response) { promises[i].set_value(std::move(response)); };

      service_.translate(model, std::move(inputs[i]), std::move(callback), options);

      futures.push_back(std::move(promises[i].get_future()));
    }

    // Wait on all futures to be ready.
    std::vector<Response> responses;
    for (size_t i = 0; i < futures.size(); i++) {
      futures[i].wait();
      responses.push_back(std::move(futures[i].get()));
    }

    return responses;
  }

  std::vector<Response> pivot(Model first, Model second, std::vector<std::string> &inputs,
                              const ResponseOptions &options) {
    py::scoped_ostream_redirect outstream(std::cout,                                 // std::ostream&
                                          py::module_::import("sys").attr("stdout")  // Python output
    );
    py::scoped_ostream_redirect errstream(std::cerr,                                 // std::ostream&
                                          py::module_::import("sys").attr("stderr")  // Python output
    );

    py::call_guard<py::gil_scoped_release> gil_guard;
    // Prepare promises, save respective futures. Have callback's in async set
    // value to the promises.
    std::vector<std::future<Response>> futures;
    std::vector<std::promise<Response>> promises;
    promises.resize(inputs.size());

    for (size_t i = 0; i < inputs.size(); i++) {
      auto callback = [&promises, i](Response &&response) { promises[i].set_value(std::move(response)); };

      service_.pivot(first, second, std::move(inputs[i]), std::move(callback), options);

      futures.push_back(std::move(promises[i].get_future()));
    }

    // Wait on all futures to be ready.
    std::vector<Response> responses;
    for (size_t i = 0; i < futures.size(); i++) {
      futures[i].wait();
      responses.push_back(std::move(futures[i].get()));
    }

    return responses;
  }

  private /*functions*/:
  static Service make_service(const Service::Config &config) {
    py::scoped_ostream_redirect outstream(std::cout,                                 // std::ostream&
                                          py::module_::import("sys").attr("stdout")  // Python output
    );
    py::scoped_ostream_redirect errstream(std::cerr,                                 // std::ostream&
                                          py::module_::import("sys").attr("stderr")  // Python output
    );

    py::call_guard<py::gil_scoped_release> gil_guard;

    return Service(config);
  }

  private /*data*/:
  Service service_;
};

PYBIND11_MODULE(_bergamot, m) {
  m.doc() = "Bergamot pybind11 bindings";
  m.attr("__version__") = marian::bergamot::bergamotBuildVersion();
  py::class_<ByteRange>(m, "ByteRange")
      .def(py::init<>())
      .def_readonly("begin", &ByteRange::begin)
      .def_readonly("end", &ByteRange::end)
      .def("__repr__", [](const ByteRange &range) {
        return "{" + std::to_string(range.begin) + ", " + std::to_string(range.end) + "}";
      });

  py::class_<AnnotatedText>(m, "AnnotatedText")
      .def(py::init<>())
      .def("numWords", &AnnotatedText::numWords)
      .def("numSentences", &AnnotatedText::numSentences)
      .def("word",
           [](const AnnotatedText &annotatedText, size_t sentenceIdx, size_t wordIdx) -> std::string {
             auto view = annotatedText.word(sentenceIdx, wordIdx);
             return std::string(view.data(), view.size());
           })
      .def("sentence",
           [](const AnnotatedText &annotatedText, size_t sentenceIdx) -> std::string {
             auto view = annotatedText.sentence(sentenceIdx);
             return std::string(view.data(), view.size());
           })
      .def("wordAsByteRange", &AnnotatedText::wordAsByteRange)
      .def("sentenceAsByteRange", &AnnotatedText::sentenceAsByteRange)
      .def_readonly("text", &AnnotatedText::text);

  py::class_<Response>(m, "Response")
      .def(py::init<>())
      .def_readonly("source", &Response::source)
      .def_readonly("target", &Response::target)
      .def_readonly("alignments", &Response::alignments);

  py::bind_vector<std::vector<std::string>>(m, "VectorString");
  py::bind_vector<std::vector<Response>>(m, "VectorResponse");

  py::enum_<ConcatStrategy>(m, "ConcatStrategy")
      .value("FAITHFUL", ConcatStrategy::FAITHFUL)
      .value("SPACE", ConcatStrategy::SPACE)
      .export_values();

  py::class_<ResponseOptions>(m, "ResponseOptions")
      .def(
          py::init<>([](bool qualityScores, bool alignment, bool HTML, bool sentenceMappings, ConcatStrategy strategy) {
            return ResponseOptions{qualityScores, alignment, HTML, sentenceMappings, strategy};
          }),
          py::arg("qualityScores") = true, py::arg("alignment") = false, py::arg("HTML") = false,
          py::arg("sentenceMappings") = true, py::arg("concatStrategy") = ConcatStrategy::FAITHFUL)
      .def_readwrite("qualityScores", &ResponseOptions::qualityScores)
      .def_readwrite("HTML", &ResponseOptions::HTML)
      .def_readwrite("alignment", &ResponseOptions::alignment)
      .def_readwrite("concatStrategy", &ResponseOptions::concatStrategy)
      .def_readwrite("sentenceMappings", &ResponseOptions::sentenceMappings);

  py::class_<ServicePyAdapter>(m, "Service")
      .def(py::init<const Service::Config &>())
      .def("modelFromConfig", &ServicePyAdapter::modelFromConfig)
      .def("modelFromConfigPath", &ServicePyAdapter::modelFromConfigPath)
      .def("translate", &ServicePyAdapter::translate)
      .def("pivot", &ServicePyAdapter::pivot);

  py::class_<Service::Config>(m, "ServiceConfig")
      .def(py::init<>([](size_t numWorkers, size_t cacheSize, std::string logging) {
             Service::Config config;
             config.numWorkers = numWorkers;
             config.cacheSize = cacheSize;
             config.logger.level = logging;
             return config;
           }),
           py::arg("numWorkers") = 1, py::arg("cacheSize") = 0, py::arg("logLevel") = "off")
      .def_readwrite("numWorkers", &Service::Config::numWorkers)
      .def_readwrite("cacheSize", &Service::Config::cacheSize);

  py::class_<_Model, std::shared_ptr<_Model>>(m, "TranslationModel");
}
