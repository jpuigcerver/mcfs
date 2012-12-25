// Copyright 2012 Joan Puigcerver <joapuipe@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef PMF_MODEL_H_
#define PMF_MODEL_H_

#include <model.h>
#include <dataset.h>
#include <protos/pmf-model.pb.h>

#include <string>
#include <vector>

using mcfs::protos::PMFModelConfig;
using mcfs::protos::PMFModelConfig_MatrixInit;

class PMFModel : public Model {
 public:
  PMFModel();
  ~PMFModel();

  void clear();
  std::string info() const;
  bool load(const PMFModelConfig& config);
  bool load(const std::string& filename);
  bool load_string(const std::string& str);
  void test(std::vector<Dataset::Rating>* test_set) const;
  float test(const Dataset& test_set) const;
  float train(const Dataset& train_set, const Dataset& valid_set);
  bool save(PMFModelConfig* config) const;
  bool save(const std::string& filename) const;
  bool save_string(std::string* str) const;

 private:
  Dataset data_;
  uint32_t D_;
  uint32_t max_iters_;
  uint32_t batch_size_;
  float learning_rate_;
  float momentum_;
  PMFModelConfig_MatrixInit matrix_init_id_;
  float lY_;
  float lV_;
  float lW_;
  float* Y_;
  float* V_;
  float* W_;
  float* HY_;  // HY = H + Y
};

#endif  // PMF_MODEL_H_
