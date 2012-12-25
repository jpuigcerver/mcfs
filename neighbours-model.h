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

#ifndef NEIGHBOURS_MODEL_H_
#define NEIGHBOURS_MODEL_H_

#include <dataset.h>
#include <protos/neighbours-model.pb.h>
#include <model.h>
#include <similarities.h>

#include <string>
#include <vector>

using mcfs::protos::NeighboursModelConfig;
using mcfs::protos::NeighboursModelConfig_Similarity;
using mcfs::protos::NeighboursModelConfig_Similarity_COSINE;

class NeighboursModel : public Model {
 public:
  typedef Dataset::Rating Rating;
  float train(const Dataset& train_set, const Dataset& valid_set);
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
