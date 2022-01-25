#include <algorithm>

#include "layers/generic.h"
#include "marian.h"
#include "models/decoder.h"
#include "models/encoder.h"
#include "models/model_factory.h"
#include "models/s2s.h"

using namespace marian;

std::string work_dir = "/workspaces/bergamot/bergamot-translator/distilled";

void createGraphParams(Ptr<ExpressionGraph>& graph, const int dim_vocab, const int dim_emb) {
  graph->param("embeddings_txt_src", {dim_vocab, dim_emb}, inits::glorotUniform());
}

void saveResults(const std::vector<std::pair<std::string, Expr> >& exprs);

void forward(Ptr<ExpressionGraph>& graph, const std::vector<WordIndex>& tokens_src, const int dim_emb, const std::vector< float >& mask_src) {
  const int dim_batch = 1;
  const int dim_tokens_src = tokens_src.size();

  auto embeddings_txt_src = graph->get("embeddings_txt_src");

  auto embedded_text_src = reshape(rows(embeddings_txt_src, tokens_src), {dim_batch, dim_tokens_src, dim_emb});
  
  auto options = New<Options>( "enc-depth", 1,
                               "dropout-rnn", 0.0f,
                               "enc-cell", "gru",
                               "dim-rnn", dim_emb,
                               "layer-normalization", false,
                               "skip", false,
                               "enc-cell-depth", 1,
                               "prefix", "encoder_s2s_text_src" );
  
  marian::EncoderS2S encoderS2S(graph, options);
  
  auto mask_src_expr = graph->constant({static_cast< int >( mask_src.size() ), 1, 1 }, marian::inits::fromVector(mask_src));

  // auto encoded_text_src = encoderS2S.applyEncoderRNN(graph, embedded_text_src, mask_src_expr , "bidirectional");
  
  auto encoder_s2s_text_src_bi_b = graph->get("encoder_s2s_text_src_bi_b");
  auto encoder_s2s_text_src_bi_bu = graph->get("encoder_s2s_text_src_bi_bu");
  auto encoder_s2s_text_src_bi_W = graph->get("encoder_s2s_text_src_bi_W");
  auto encoder_s2s_text_src_bi_U = graph->get("encoder_s2s_text_src_bi_U");
  
  auto encoder_s2s_text_src_bi_r_b = graph->get("encoder_s2s_text_src_bi_r_b");
  auto encoder_s2s_text_src_bi_r_bu = graph->get("encoder_s2s_text_src_bi_r_bu");
  auto encoder_s2s_text_src_bi_r_W = graph->get("encoder_s2s_text_src_bi_r_W");
  auto encoder_s2s_text_src_bi_r_U = graph->get("encoder_s2s_text_src_bi_r_U");

  auto xW = dot(embedded_text_src,encoder_s2s_text_src_bi_W );
  
  debug(embedded_text_src, "embedded_text_src");
  debug(encoder_s2s_text_src_bi_W, "encoder_s2s_text_src_bi_W");
  debug(xW, "xW");
  
  std::cout<< graph->graphviz() << std::endl;

  graph->forward();

  saveResults({{"embedded_text_src", embedded_text_src} });
}

int main(const int argc, const char* argv[]) {
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
  std::vector<float> mask_src( tokens_src.size(), 1.0 );

  // // Create randon params
  // forward(graph, tokens_src, dim_emb, mask);

  // Load converted python model
  graph->load(work_dir + "/distilled.npz");

  // Simulating the python tokenizer
  dim_vocab = 31781;
  tokens_src = {1, 1, 1, 1, 118, 1, 1, 3061, 1,    1, 1, 1, 2, 1,    1, 1, 1, 1, 1,
                1, 1, 1, 1, 3,   1, 1, 1,    1542, 1, 1, 1, 1, 1542, 1, 2, 1, 1};
                
  mask_src = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
              1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
              1.0, 1.0, 1.0, 1.0, 1.0};
           
  // createGraphParams( graph, dim_vocab, dim_emb);

  forward(graph, tokens_src, dim_emb, mask_src);

  return 0;
}

void saveResults(const std::vector<std::pair<std::string, Expr> >& exprs) {
  std::vector<io::Item> items;

  std::transform(std::begin(exprs), std::end(exprs), std::back_inserter(items), [](const auto& expr) {
    io::Item item;
    expr.second->val()->get(item, expr.first);
    return item;
  });

  io::saveItems(work_dir + "/distillied_marian_results.npz", items);
}
