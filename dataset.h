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

Dataset() : criteria_size_(0), N_(0), M_(0) {};
  void shuffle(bool prepare = true);
  bool load(const std::string& filename);
  bool save(const std::string&, bool ascii = false) const;
  void info(uint32_t npr = 0, bool log = true) const;
  static void partition(Dataset * original, Dataset * partition, float f);
  static float rmse(const Dataset& a, const Dataset& b);

  inline std::vector<Rating>& mutable_ratings() { return ratings_; }
  inline const std::vector<Rating>& ratings() const { return ratings_; }
  inline float minv(uint32_t c) const { return minv_[c]; }
  inline float maxv(uint32_t c) const { return maxv_[c]; }
  inline float precision(uint32_t c) const { return precision_[c]; }
  Dataset& operator = (const Dataset& other);

  bool load(const mcfs::protos::Ratings& ratings);
  void save(mcfs::protos::Ratings * ratings) const;


  inline uint32_t users() const {
    return N_;
  }
  inline uint32_t items() const {
    return M_;
  }
  inline size_t criteria_size() const {
    return criteria_size_;
  }
  inline size_t ratings_size() const {
    return ratings_.size();
  }

  void clear();
  void erase_scores();
  void create_batch(Dataset * other, uint32_t batch_size) const;

  const std::vector<Rating*>& ratings_by_item(uint32_t item) const;
  const std::vector<Rating*>& ratings_by_user(uint32_t user) const;

  void get_scores_from_common_ratings_by_users(
      uint32_t user1, uint32_t user2,
      std::vector<float> * ratings_u1,
      std::vector<float> * ratings_u2) const;

  void to_normal_scale();
  void to_original_scale();
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
  std::vector<std::vector<Rating*> >
      ratings_by_user_;
  std::vector<std::vector<Rating*> >
      ratings_by_item_;


  void count_users();
  void count_items();
  void prepare_aux();
  void init_minv();
  void init_maxv();
};

#endif  // DATASET_H_
