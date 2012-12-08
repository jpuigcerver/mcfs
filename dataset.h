#ifndef DATASET_H_
#define DATASET_H_

#include <protos/ratings.pb.h>

#include <vector>
#include <string>

struct Dataset {
  Ratings ratings;
  bool load(const char * filename);
  bool save(const char * filename, bool ascii = false) const;
  void print() const;
  static void partition(Dataset * original, Dataset * partition, float f);
};

extern float RMSE(const std::vector<Rating>& a, const std::vector<Rating>& b);
extern float RMSE(const Dataset& a, const Dataset& b);

#endif  // DATASET_H_
