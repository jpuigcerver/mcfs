#ifndef MODEL_H_
#define MODEL_H_

#include <dataset.h>
#include <vector>
#include <utility>
#include <protos/ratings.pb.h>

class Model {
 public:
  typedef std::pair<uint64_t, uint64_t> user_item_t;
  typedef std::vector<const Rating*> vpc_ratings_t;

  virtual ~Model() {};
  virtual bool load(const char * filename) = 0;
  virtual bool save(const char * filename, bool ascii = false) const = 0;
  virtual void train(const Dataset& train_set) = 0;
  virtual void train(const Dataset& train_set, const Dataset& valid_set) = 0;
  virtual float test(const Dataset& test_set) const = 0;
  virtual Ratings test(
      const std::vector<user_item_t>& users_items) const = 0;
};

#endif  // MODEL_H_
