#ifndef DATASET_H_
#define DATASET_H_

#include <vector>
#include <string>

struct Rating {
  uint32_t user;
  uint32_t item;
  std::vector<float> c_rating;
  Rating();
  Rating(uint32_t user, uint32_t item, uint64_t criteria_size);
};

struct Dataset {
  typedef enum {ASCII, BINARY} ds_file_format;
  std::vector<Rating> ratings;
  uint64_t criteria_size;
  bool load_file(const char * filename);
  bool save_file(const char * filename, ds_file_format format = ASCII) const;
  void print() const;
  static void partition(Dataset * original, Dataset * partition, float f);
};

#endif  // DATASET_H_
