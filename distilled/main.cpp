#include <algorithm>
#include <vector>

#include "CLI/CLI.hpp"
#include "layers/generic.h"
#include "marian.h"
#include "models/decoder.h"
#include "models/encoder.h"
#include "models/model_factory.h"
#include "models/s2s.h"

using namespace marian;
using Exprs = std::vector<std::pair<std::string, Expr> >;

void createGraphParams(Ptr<ExpressionGraph>& graph, const int dim_vocab, const int dim_emb) {
  graph->param("embeddings_txt_src", {dim_vocab, dim_emb}, inits::glorotUniform());
}

void saveResults(const std::string& filePath, const Exprs& exprs);
Exprs forward(Ptr<ExpressionGraph>& graph, const std::vector<WordIndex>& tokens_src, const int dim_emb,
              const std::vector<float>& mask_src);

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

  // // Create randon params
  // forward(graph, tokens_src, dim_emb, mask);

  // Load converted python model
  graph->load(modelPath);

  // Simulating the python tokenizer
  dim_vocab = 31781;
  tokens_src = {1, 1, 1, 1, 118, 1, 1, 3061, 1,    1, 1, 1, 2, 1,    1, 1, 1, 1, 1,
                1, 1, 1, 1, 3,   1, 1, 1,    1542, 1, 1, 1, 1, 1542, 1, 2, 1, 1};

  mask_src = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
              1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};

  // createGraphParams( graph, dim_vocab, dim_emb);

  const auto results = forward(graph, tokens_src, dim_emb, mask_src);

  if (!outputPath.empty()) {
    saveResults(outputPath, results);
  }

  return 0;
}

Exprs forward(Ptr<ExpressionGraph>& graph, const std::vector<WordIndex>& tokens_src, const int dim_emb,
              const std::vector<float>& mask_src) {
  const int dim_batch = 1;
  const int dim_tokens_src = tokens_src.size();

  auto embeddings_txt_src = graph->get("embeddings_txt_src");

  auto embedded_text_src = reshape(rows(embeddings_txt_src, tokens_src), {dim_batch, dim_tokens_src, dim_emb});

  auto options =
      New<Options>("enc-depth", 1, "dropout-rnn", 0.0f, "enc-cell", "gru", "dim-rnn", dim_emb, "layer-normalization",
                   false, "skip", false, "enc-cell-depth", 1, "prefix", "encoder_s2s_text_src", "hidden-bias", true);

  marian::EncoderS2S encoderS2S(graph, options);

  auto embedded_text_src_t = transpose(embedded_text_src, {1, 0, 2});

  auto mask_src_expr = graph->constant({static_cast<int>(mask_src.size()), 1, 1}, marian::inits::fromVector(mask_src));

  auto encoded_text_src_t = encoderS2S.applyEncoderRNN(graph, embedded_text_src_t, mask_src_expr, "bidirectional");

  auto encoded_text_src = transpose(encoded_text_src_t, {1, 0, 2});

  std::cout << graph->graphviz() << std::endl;

  graph->forward();

  return {{"embedded_text_src", embedded_text_src}, {"encoded_text_src", encoded_text_src}};
}

void saveResults(const std::string& filePath, const Exprs& exprs) {
  std::vector<io::Item> items;

  std::transform(std::begin(exprs), std::end(exprs), std::back_inserter(items), [](const auto& expr) {
    io::Item item;
    expr.second->val()->get(item, expr.first);
    return item;
  });

  io::saveItems(filePath, items);
}
