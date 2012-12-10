#include <dataset.h>

#include <fcntl.h>
#include <glog/logging.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <google/protobuf/text_format.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

using google::protobuf::TextFormat;
using google::protobuf::io::FileInputStream;
using google::protobuf::io::FileOutputStream;

bool Dataset::save(const char* filename, bool ascii) const {
  int fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (fd < 0) {
    LOG(ERROR) << "Dataset \"" << filename << "\": Failed to open. Error: "
               << strerror(errno);
    return false;
  }
  FileOutputStream fs(fd);
  if (ascii) {
    LOG(ERROR) << "Dataset \"" << filename << "\": ASCII format not implemented.";
    return false;
  }
  if ((ascii && !TextFormat::Print(ratings, &fs)) ||
      (!ascii && !ratings.SerializeToFileDescriptor(fd))) {
    LOG(ERROR) << "Dataset \"" << filename << "\": Failed to write.";
    return false;
  }
  close(fd);
  return true;
}

bool Dataset::load(const char* filename) {
  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    LOG(ERROR) << "Dataset \"" << filename << "\": Failed to open. Error: "
               << strerror(errno);
    return false;
  }
  FileInputStream fs(fd);
  if (!ratings.ParseFromFileDescriptor(fd) && !TextFormat::Parse(&fs, &ratings)) {
    LOG(ERROR) << "Dataset \"" << filename << "\": Failed to parse.";
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

void CopyRatings(const Ratings& src, Ratings * dst) {
  CHECK_NOTNULL(dst);
  dst->clear_rating();
  dst->clear_criteria_size();
  if (src.has_criteria_size()) {
    dst->set_criteria_size(src.criteria_size());
  }
  for (int i = 0; i < src.rating_size(); ++i) {
    const Rating& orig_rating = src.rating(i);
    Rating * new_rating = dst->add_rating();
    new_rating->set_user(orig_rating.user());
    new_rating->set_item(orig_rating.item());
    for (int j = 0; j < orig_rating.rating_size(); ++j) {
      new_rating->add_rating(orig_rating.rating(j));
    }
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
