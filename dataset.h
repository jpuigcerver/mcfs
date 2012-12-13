#ifndef DATASET_H_
#define DATASET_H_

#include <stdint.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <math.h>
#include <protos/ratings.pb.h>

class Dataset {
public:
  struct Rating {
    uint32_t user;
    uint32_t item;
    std::vector<float> scores;
  };

Dataset() : criteria_size(0), aux_ready(false) {};
  void shuffle();
  bool load(const std::string& filename);
  bool save(const std::string&, bool ascii = false) const;
  void info(uint64_t npr = 0) const;
  static void partition(Dataset * original, Dataset * partition, float f);
  static float rmse(const Dataset& a, const Dataset& b);

  inline std::vector<Rating>& mutable_ratings() { return ratings; }
  inline float get_minv(uint64_t c) const { return minv[c]; }
  inline float get_maxv(uint64_t c) const { return maxv[c]; }
  inline float get_precision(uint64_t c) const { return precision[c]; }

  bool load(const mcfs::protos::Ratings& ratings);
  void save(mcfs::protos::Ratings * ratings) const;

  void clear () {
    aux_ready = false;
    criteria_size = 0;
    ratings.clear();
    ratings_by_user.clear();
    ratings_by_item.clear();
  }

  void copy_empty(const Dataset& src) {
    clear();
    ratings.resize(src.ratings.size());
    criteria_size = src.criteria_size;
    for (uint64_t r = 0; r < ratings.size(); ++r) {
      ratings[r].user = src.ratings[r].user;
      ratings[r].item = src.ratings[r].item;
      ratings[r].scores.resize(criteria_size, 0.0f);
    }
  }

  //std::vector<Rating> get_ratings_by_user(uint64_t user) const;
  void get_ratings_by_item(
      uint64_t item, const std::vector<const Rating*>** item_ratings) const;

  const Rating* get_rating(uint64_t user, uint64_t item) const {
    prepare_aux();
    const auto& it = ratings_by_user.find(user);
    if (it == ratings_by_user.end() || it->second.size() == 0) {
      return NULL;
    }
    for(uint64_t l = 0, r = it->second.size()-1; l < r; ) {
      uint64_t c = (l+r)/2;
      if (item == it->second[c]->item) {
        return it->second[c];
      } else if (item < it->second[c]->item) {
        r = c-1;
      } else {
        l = c+1;
      }
    }
    return NULL;
  }

  void get_common_ratings_by_users(
      uint64_t user1, uint64_t user2,
      std::vector<const Rating*>* ratings_u1,
      std::vector<const Rating*>* ratings_u2) const;
  void get_scores_from_common_ratings_by_users(
      uint64_t user1, uint64_t user2,
      std::vector<float> * ratings_u1,
      std::vector<float> * ratings_u2) const;
  inline uint64_t get_criteria_size() const {return criteria_size;}
  inline uint64_t get_ratings_size() const {return ratings.size();}
private:
  struct SortPRatingsByItem {
    bool operator() (const Rating* a, const Rating* b) const;
  };

  struct SortPRatingsByUser {
    bool operator() (const Rating* a, const Rating* b) const;
  };

  mutable std::unordered_map<uint64_t, std::vector<const Rating*> >
      ratings_by_user; // ratings_by_user[user]
  mutable std::unordered_map<uint64_t, std::vector<const Rating*> >
      ratings_by_item; // ratings_by_item[item]


  std::vector<Rating> ratings;
  std::vector<float> minv;
  std::vector<float> maxv;
  std::vector<int> precision;
  uint64_t criteria_size;
  mutable bool aux_ready;

  void prepare_aux() const;
  void set_minv() {
    minv.resize(criteria_size, INFINITY);
    for (const Rating& rating: ratings) {
      for (uint64_t c = 0; c < criteria_size; ++c) {
        minv[c] = std::min(minv[c], rating.scores[c]);
      }
    }
  }
  void set_maxv() {
    maxv.resize(criteria_size, -INFINITY);
    for (const Rating& rating: ratings) {
      for (uint64_t c = 0; c < criteria_size; ++c) {
        maxv[c] = std::max(maxv[c], rating.scores[c]);
      }
    }
  }
};

#endif  // DATASET_H_
