#ifndef PMF_MODEL_H_
#define PMF_MODEL_H_

#include <model.h>
#include <dataset.h>
#include <protos/pmf-model.pb.h>
#include <string>
#include <unordered_map>

using mcfs::protos::PMFModelConfig;

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
  void info(bool log = true) const;
  float train_batch(const Dataset& batch);

  PMFModel() : D_(10), max_iters_(100), learning_rate_(0.1f),
      lY_(0.0f), lV_(0.0f), lW_(0.0f),
      Y_(NULL), V_(NULL), W_(NULL) {}

  void clear() {
    data_.clear();
    D_ = 10;
    max_iters_ = 100;
    learning_rate_ = 0.1f;
    lY_ = 0.0f;
    lV_ = 0.0f;
    lW_ = 0.0f;
    if (Y_ != NULL) {
      delete [] Y_;
      Y_ = NULL;
    }
    if (V_ != NULL) {
      delete [] V_;
      V_ = NULL;
    }
    if (W_ != NULL) {
      delete [] W_;
      W_ = NULL;
    }
  }
 private:
  Dataset data_;
  uint32_t D_;
  uint32_t max_iters_;
  float learning_rate_;
  float lY_;
  float lV_;
  float lW_;
  float* Y_;
  float* V_;
  float* W_;
};

#endif  // PMF_MODEL_H_
