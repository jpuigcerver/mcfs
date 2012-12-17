#ifndef NEIGHBOURS_MODEL_H_
#define NEIGHBOURS_MODEL_H_

#include <similarities.h>
#include <model.h>
#include <dataset.h>
#include <protos/neighbours-model.pb.h>
#include <string>

using mcfs::protos::NeighboursModelConfig;
using mcfs::protos::NeighboursModelConfig_Similarity;
using mcfs::protos::NeighboursModelConfig_Similarity_COSINE;

class NeighboursModel : public Model {
 public:
  typedef Dataset::Rating Rating;
  float train(const Dataset& train_set);
  void test(std::vector<Rating>* test_set) const;
  bool save(const std::string& filename) const;
  bool load(const std::string& filename);
  bool save(NeighboursModelConfig * config) const;
  bool load(const NeighboursModelConfig& config);
  bool load_string(const std::string& str);
  bool save_string(std::string* str) const;
  std::string info() const;

NeighboursModel() : K_(0),
      similarity_code_(NeighboursModelConfig_Similarity_COSINE),
      similarity_(&StaticCosineSimilarity) {}
 private:
  Dataset data_;
  uint32_t K_;
  NeighboursModelConfig_Similarity similarity_code_;
  const Similarity * similarity_;
};

#endif  // NEIGHBOURS_MODEL_H_
