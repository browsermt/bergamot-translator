#include <algorithm>
#include <vector>

#include "CLI/CLI.hpp"

#include "birnn.h"

int main(const int argc, const char* argv[]) {
  std::string modelPath;
  std::string outputPath;

  CLI::App cliApp("Disttilied Model");
  cliApp.add_option("-m,--model", modelPath, "model weights npz file path")->check(CLI::ExistingFile)->required();
  cliApp.add_option("-o,--output", outputPath, "creates a output npz file with the inference results");

  CLI11_PARSE(cliApp, argc, argv);

  createLoggers();

  auto graph = New<ExpressionGraph>();

  graph->setDevice({0, DeviceType::cpu});
  graph->reserveWorkspaceMB(128);

  // https://github.com/sheffieldnlp/deepQuest-py/blob/main/deepquestpy/config/birnn.jsonnet#L28
  const int dim_emb = 50;

  // Tokenize source input by marian vocab
  // https://data.statmt.org/bergamot/models/eten/

  marian::Vocab vocab(New<Options>(), 0);
  vocab.load("/workspaces/bergamot/attentions/vocab.eten.spm");

  const auto tokens_word_src = vocab.encode(
      "Pärast Portugali Vabariigi väljakuulutamist võeti 1911. aastal kasutusele uus rahaühik eskuudo , mis jagunes "
      "100 sentaavoks .");

  auto dim_vocab = static_cast<int>(vocab.size());
  auto tokens_src = toWordIndexVector(tokens_word_src);
  std::vector<float> mask_src(tokens_src.size(), 1.0);

  // Load converted python model
  graph->load(modelPath);

  // Simulating the python tokenizer
  dim_vocab = 31781;
  tokens_src = {1, 1, 1, 1, 118, 1, 1, 3061, 1,    1, 1, 1, 2, 1,    1, 1, 1, 1, 1,
                1, 1, 1, 1, 3,   1, 1, 1,    1542, 1, 1, 1, 1, 1542, 1, 2, 1, 1};

  mask_src = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
              1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};

  const auto results = distilled::birnn::forward(graph, tokens_src, dim_emb, mask_src);

  if (!outputPath.empty()) {
    distilled::saveResults(outputPath, results);
  }

  return 0;
}
