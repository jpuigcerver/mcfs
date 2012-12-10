#ifndef MODEL_H_
#define MODEL_H_

#include <dataset.h>
#include <vector>
#include <utility>
#include <protos/ratings.pb.h>

class Model {
 public:
  typedef std::map<uint32_t, const Rating*> map_ratings_t;
  typedef std::vector<const Rating *> vpc_ratings_t;

  virtual ~Model() {};
  virtual bool load(const char * filename) = 0;
  virtual bool save(const char * filename, bool ascii = false) const = 0;
  virtual float train(const Dataset& train_set) = 0;
  virtual float train(const Dataset& train_set, const Dataset& valid_set) = 0;
  virtual float test(const Dataset& test_set) const = 0;
  virtual Ratings test(const Ratings& test_set) const = 0;
};

#endif  // MODEL_H_
