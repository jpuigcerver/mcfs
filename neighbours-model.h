#ifndef NEIGHBOURS_MODEL_H_
#define NEIGHBOURS_MODEL_H_

#include <similarities.h>
#include <model.h>
#include <map>

class NeighboursModel : public Model {
 public:
  typedef std::pair<const Rating*, const Rating*> ppc_rating_t;
  typedef std::vector<ppc_rating_t> common_ratings_t;

  void train(const Dataset& train_set, const Dataset& valid_set);
  float test(const Dataset& test_set) const;
  std::vector<Rating> test(const std::vector<user_item_t>& users_items) const;

 private:
  Dataset data_;
  Similarity similarity;
  std::map<uint64_t, vpc_ratings_t> user_ratings_; // Key: user
  std::map<uint64_t, vpc_ratings_t> item_ratings_; // Key: item
  common_ratings_t get_common_ratings(uint32_t a, uint32_t b) const;
  void nd_vectors_from_common_ratings(
      const common_ratings_t& common_ratings,
      std::vector<float> * va, std::vector<float> * vb) const;
};

#endif  // NEIGHBOURS_MODEL_H_
