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

extern void CopyRatings(const Ratings& src, Ratings * dst);
extern bool MoveRatings(Ratings * src, Ratings * dst);
extern float RMSE(const Ratings& a, const Ratings& b);
extern float RMSE(const Dataset& a, const Dataset& b);

#endif  // DATASET_H_
