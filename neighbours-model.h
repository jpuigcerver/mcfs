#ifndef NEIGHBOURS_MODEL_H_
#define NEIGHBOURS_MODEL_H_

#include <similarities.h>
#include <model.h>
#include <map>

#include <protos/ratings.pb.h>
#include <protos/neighbours-model.pb.h>

using namespace mcfs::models;
using google::protobuf::RepeatedPtrField;

class NeighboursModel : public Model {
 public:
  typedef std::pair<const Rating*, const Rating*> ppc_rating_t;
  typedef std::vector<ppc_rating_t> common_ratings_t;

  bool load(const char * filename);
  bool save(const char * filename, bool ascii = false) const;
  float train(const Dataset& train_set);
  float train(const Dataset& train_set, const Dataset& valid_set);
  float test(const Dataset& test_set) const;
  Ratings test(const Ratings& test_set) const;

  NeighboursModel() : similarity(new CosineSimilarity()) { }
  ~NeighboursModel() { delete similarity; }
 private:
  NeighboursModelConfig config;
  Ratings * data_;
  const Similarity * similarity;
  std::map<uint32_t, vpc_ratings_t> user_ratings_; // Key: user
  std::map<uint32_t, vpc_ratings_t> item_ratings_; // Key: item
  common_ratings_t get_common_ratings(uint32_t a, uint32_t b) const;
  void nd_vectors_from_common_ratings(
      const common_ratings_t& common_ratings,
      std::vector<float> * va, std::vector<float> * vb) const;
};

#endif  // NEIGHBOURS_MODEL_H_
