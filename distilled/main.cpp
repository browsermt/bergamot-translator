
#include <algorithm>

#include "layers/generic.h"
#include "marian.h"
#include "models/decoder.h"
#include "models/encoder.h"
#include "models/model_factory.h"
#include "models/s2s.h"

using namespace marian;

std::string work_dir = "/workspaces/bergamot/bergamot-translator/distilled";

void createGraph(Ptr<ExpressionGraph>& graph, const Words& tokens_src, const int dim_vocab) {
  // https://github.com/sheffieldnlp/deepQuest-py/blob/main/deepquestpy/config/birnn.jsonnet#L28
  const int dim_emb = 50;
  const int dim_batch = 1;

  const auto tokens_src_dim = static_cast<int>(tokens_src.size());

  auto embeddings_txt_src = graph->param("embeddings_txt_src", {dim_vocab, dim_emb}, inits::glorotUniform());
  auto embedded_txt_src =
      reshape(rows(embeddings_txt_src, toWordIndexVector(tokens_src)), {dim_batch, tokens_src_dim, dim_emb});

  std::cout << graph->graphviz() << std::endl;

  graph->forward();

  graph->save(work_dir + "/distilled.npz");

  std::vector<float> values;

  embedded_txt_src->val()->get(values);

  std::cout << values.size() << std::endl;
}

int main(const int argc, const char* argv[]) {
  createLoggers();

  auto graph = New<ExpressionGraph>();

  graph->setDevice({0, DeviceType::cpu});
  graph->reserveWorkspaceMB(128);

  marian::Vocab vocab(New<Options>(), 0);
  vocab.load("/workspaces/bergamot/attentions/vocab.eten.spm");

  // Source Input
  const auto tokens_src = vocab.encode("Pärast Portugali Vabariigi väljakuulutamist võeti 1911. aastal kasutusele uus rahaühik eskuudo , mis jagunes 100 sentaavoks .");

  const auto dim_vocab = static_cast<int>(vocab.size());

  createGraph(graph, tokens_src, dim_vocab);

  return 0;
}
