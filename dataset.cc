#include <dataset.h>

#include <fcntl.h>
#include <glog/logging.h>
#include <string.h>

#include <algorithm>
#include <string>

bool Dataset::save(const char* filename, bool ascii) const {
  int fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (fd < 0) {
    LOG(ERROR) << "Dataset \"" << filename << "\": Failed to open. Msg: "
               << strerror(errno);
    return false;
  }
  if (ascii) {
    std::string sdata = ratings.DebugString();
    if (write(fd, sdata.c_str(), sdata.length()) < 0) {
      LOG(ERROR) << "Dataset \"" << filename << "\": Failed to write. Msg: "
                 << strerror(errno);
      return false;
    }
  } else {
    ratings.SerializeToFileDescriptor(fd);
  }
  close(fd);
  return true;
}

bool Dataset::load(const char* filename) {
  int fd = open(filename, O_RDONLY);
  if (ratings.ParseFromFileDescriptor(fd)) {
    return false;
  }
  close(fd);
  if (!ratings.has_criteria_size() && ratings.rating_size() > 0) {
    ratings.set_criteria_size(ratings.rating(0).rating_size());
  }
  return true;
}

void Dataset::print() const {
  ratings.PrintDebugString();
}

// This function moves each element from one dataset to an other with
// probability 1-f. This is useful to create a test set from the original
// training data, for instance.
// TODO(jpuigcerver) Pass some seed to control the generation of random numbers.
void Dataset::partition(Dataset * original, Dataset * partition, float f) {
  random_shuffle(original->ratings.mutable_rating()->begin(),
                 original->ratings.mutable_rating()->end());
  partition->ratings.clear_rating();
  if (original->ratings.has_criteria_size()) {
    partition->ratings.set_criteria_size(original->ratings.criteria_size());
  }
  DLOG(INFO) << "Original size: " << original->ratings.rating_size();
  int start_pos = static_cast<int>(f * original->ratings.rating_size());
  for (int i = original->ratings.rating_size()-1; i >= start_pos; --i) {
    *partition->ratings.add_rating() = original->ratings.rating(i);
    original->ratings.mutable_rating()->RemoveLast();
  }
  DLOG(INFO) << "New size: " << original->ratings.rating_size();
  DLOG(INFO) << "Partition size: " << partition->ratings.rating_size();
  if (original->ratings.rating_size() == 0 ||
      partition->ratings.rating_size() == 0) {
    LOG(WARNING) << "Some partition is empty.";
  }
}

float RMSE(const Ratings& a, const Ratings& b) {
  CHECK_EQ(a.rating_size(), b.rating_size());
  float s = 0.0f;
  for (auto x = a.rating().begin(), y = b.rating().begin();
       x != a.rating().end(); ++x, ++y) {
    CHECK_EQ(x->rating_size(), y->rating_size());
    for (auto ra = x->rating().begin(), rb = y->rating().begin();
         ra != x->rating().end(); ++ra, ++rb) {
      float d = (*ra - *rb);
      s += d * d;
    }
  }
  return sqrtf(s / a.rating_size());
}

float RMSE(const Dataset& a, const Dataset& b) {
  return RMSE(a.ratings, b.ratings);
}
