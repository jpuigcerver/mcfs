#ifndef PMF_MODEL_H_
#define PMF_MODEL_H_

#include <model.h>
#include <dataset.h>
#include <protos/pmf-model.pb.h>
#include <string>
#include <unordered_map>

using mcfs::protos::PMFModelConfig;
using mcfs::protos::PMFModelConfig_MatrixInit;

class PMFModel : public Model {
 public:
  typedef Dataset::Rating Rating;
  float train(const Dataset& train_set);
  void test(std::vector<Rating>* test_set) const;
  bool save(const std::string& filename) const;
  bool load(const std::string& filename);
  bool save(PMFModelConfig * config) const;
  bool load(const PMFModelConfig& config);
  bool load_string(const std::string& str);
  bool save_string(std::string* str) const;
  std::string info() const;
  float train_batch(const Dataset& batch);

  PMFModel();
  ~PMFModel();

  void clear();
 private:
  Dataset data_;
  uint32_t D_;
  uint32_t max_iters_;
  float learning_rate_;
  float momentum_;
  PMFModelConfig_MatrixInit matrix_init_id_;
  float lY_;
  float lV_;
  float lW_;
  float* Y_;
  float* V_;
  float* W_;
  float* HY_; // HY = H + Y
};

#endif  // PMF_MODEL_H_
