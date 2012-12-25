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

#ifndef DATASET_H_
#define DATASET_H_

#include <protos/ratings.pb.h>
#include <math.h>
#include <stdint.h>

#include <string>
#include <vector>

class Dataset {
 public:
  struct Rating {
    uint32_t user;
    uint32_t item;
    std::vector<float> scores;
  };

  Dataset();
  Dataset& operator = (const Dataset& other);

  void clear();
  void copy(Dataset* other, size_t i, size_t n) const;
  void erase_scores();
  void get_scores_from_common_ratings_by_users(
      uint32_t user1, uint32_t user2,
      std::vector<float> * ratings_u1,
      std::vector<float> * ratings_u2) const;
  std::string info(uint32_t npr = 0) const;
  bool load(const mcfs::protos::Ratings& ratings);
  bool load(const std::string& filename);
  const std::vector<Rating*>& ratings_by_item(uint32_t item) const;
  const std::vector<Rating*>& ratings_by_user(uint32_t user) const;
  void save(mcfs::protos::Ratings * ratings) const;
  bool save(const std::string&, bool ascii = false) const;
  void shuffle(bool prepare = true);
  void to_normal_scale();
  void to_original_scale();

  inline std::vector<Rating>& mutable_ratings() { return ratings_; }
  inline const std::vector<Rating>& ratings() const { return ratings_; }
  inline float minv(uint32_t c) const { return minv_[c]; }
  inline float maxv(uint32_t c) const { return maxv_[c]; }
  inline float precision(uint32_t c) const { return precision_[c]; }
  inline uint32_t users() const { return N_; }
  inline uint32_t items() const { return M_; }
  inline size_t criteria_size() const { return criteria_size_; }
  inline size_t ratings_size() const { return ratings_.size(); }

  static void partition(Dataset * original, Dataset * partition, float f);
  static float rmse(const Dataset& a, const Dataset& b);

 private:
  struct SortPRatingsByItem {
    bool operator() (const Rating* a, const Rating* b) const;
  };

  struct SortPRatingsByUser {
    bool operator() (const Rating* a, const Rating* b) const;
  };

  std::vector<Rating> ratings_;
  std::vector<float> minv_;
  std::vector<float> maxv_;
  std::vector<int> precision_;
  uint32_t criteria_size_;
  uint32_t N_, M_;
  std::vector<std::vector<Rating*> > ratings_by_user_;
  std::vector<std::vector<Rating*> > ratings_by_item_;

  void count_items();
  void count_users();
  void init_maxv();
  void init_minv();
  void prepare_aux();
};

#endif  // DATASET_H_
