#ifndef MODEL_H_
#define MODEL_H_

#include <dataset.h>
#include <vector>
#include <string>

class Model {
 public:
  virtual ~Model() {};
  virtual float train(const Dataset& train_set) = 0;
  virtual float test(const Dataset& test_set) const = 0;
  virtual void test(std::vector<Dataset::Rating>* users_items) const = 0;
  virtual bool save(const std::string& filename) const = 0;
  virtual bool load(const std::string& filename) = 0;
  virtual bool load_string(const std::string& str) = 0;
  virtual bool save_string(std::string* str) const = 0;
};

#endif  // MODEL_H_
