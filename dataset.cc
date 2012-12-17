#include <dataset.h>

#include <fcntl.h>
#include <glog/logging.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <protos/ratings.pb.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

using google::protobuf::TextFormat;
using google::protobuf::io::FileInputStream;
using google::protobuf::io::FileOutputStream;
using mcfs::protos::Ratings_Precision;
using mcfs::protos::Ratings_Precision_FLOAT;

extern std::default_random_engine PRNG;

bool Dataset::save(const std::string& filename, bool ascii) const {
  mcfs::protos::Ratings proto_ratings;
  save(&proto_ratings);
  // Write protocol buffer
  int fd = open(filename.c_str(), O_CREAT | O_WRONLY | O_TRUNC,
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
  if ((ascii && !TextFormat::Print(proto_ratings, &fs)) ||
      (!ascii && !proto_ratings.SerializeToFileDescriptor(fd))) {
    LOG(ERROR) << "Dataset \"" << filename << "\": Failed to write.";
    return false;
  }
  close(fd);
  return true;
}

bool Dataset::load(const std::string& filename) {
  mcfs::protos::Ratings proto_ratings;
  // Read protocol buffer
  int fd = open(filename.c_str(), O_RDONLY);
  if (fd < 0) {
    LOG(ERROR) << "Dataset \"" << filename << "\": Failed to open. Error: "
               << strerror(errno);
    return false;
  }
  FileInputStream fs(fd);
  if (!proto_ratings.ParseFromFileDescriptor(fd)) {
    /*&&
      !TextFormat::Parse(&fs, &proto_ratings)) {*/
    LOG(ERROR) << "Dataset \"" << filename << "\": Failed to parse.";
    return false;
  }
  close(fd);
  if (!proto_ratings.has_criteria_size() && proto_ratings.rating_size() > 0) {
    proto_ratings.set_criteria_size(proto_ratings.rating(0).score_size());
  }
  return load(proto_ratings);
}

bool Dataset::load(const mcfs::protos::Ratings& proto_ratings) {
  // Fill internal data
  clear();
  criteria_size_ = proto_ratings.criteria_size();
  if (proto_ratings.rating_size() == 0) {
    return true;
  }
  // If the criteria size is unknown, set to the first.
  if (criteria_size_ == 0) {
    criteria_size_ = proto_ratings.rating(0).score_size();
  }
  ratings_.resize(proto_ratings.rating_size());
  // Copy from protobuf to self-memory
  for (uint32_t r = 0; r < ratings_.size(); ++r) {
    // Set user & item ID
    ratings_[r].user = proto_ratings.rating(r).user();
    ratings_[r].item = proto_ratings.rating(r).item();
    // Check wether the number of scores is the expected
    if (static_cast<uint32_t>(
            proto_ratings.rating(r).score_size()) != criteria_size_) {
      LOG(ERROR) << "Dataset: Inconsistent criteria size. "
                 << "Size found = " << proto_ratings.rating(r).score_size()
                 << ". Size expected = " << criteria_size_;
      return false;
    }
    // Set the rating scores
    const float * p_scores = proto_ratings.rating(r).score().data();
    ratings_[r].scores.resize(criteria_size_);
    std::copy(p_scores, p_scores + criteria_size_, ratings_[r].scores.begin());
  }
  // Set min value for each criteria
  if (static_cast<uint32_t>(proto_ratings.minv_size()) == criteria_size_) {
    minv_.resize(criteria_size_);
    std::copy(proto_ratings.minv().data(),
              proto_ratings.minv().data() + criteria_size_,
              minv_.begin());
  } else {
    init_minv();
  }
  // Set max value for each criteria
  if (static_cast<uint32_t>(proto_ratings.maxv_size()) == criteria_size_) {
    maxv_.resize(criteria_size_);
    std::copy(proto_ratings.maxv().data(),
              proto_ratings.maxv().data() + criteria_size_,
              maxv_.begin());
  } else {
    init_maxv();
  }
  // Set the precision of each criteria
  if (static_cast<uint32_t>(proto_ratings.precision_size()) == criteria_size_) {
    precision_.resize(criteria_size_);
    std::copy(proto_ratings.precision().data(),
              proto_ratings.precision().data() + criteria_size_,
              precision_.begin());
  } else if (proto_ratings.precision_size() == 1) {
    precision_.resize(criteria_size_, proto_ratings.precision(0));
  } else {
    precision_.resize(criteria_size_, Ratings_Precision_FLOAT);
  }
  if (proto_ratings.has_num_users()) {
    N_ = proto_ratings.num_users();
  } else {
    count_users();
  }
  if (proto_ratings.has_num_items()) {
    M_ = proto_ratings.num_items();
  } else {
    count_items();
  }
  prepare_aux();
  return true;
}

void Dataset::save(mcfs::protos::Ratings * proto_ratings) const {
  CHECK_NOTNULL(proto_ratings);
  // Fill protocol buffer
  proto_ratings->clear_minv();
  proto_ratings->clear_maxv();
  proto_ratings->clear_precision();
  proto_ratings->clear_rating();
  if (criteria_size_ > 0) {
    proto_ratings->set_criteria_size(criteria_size_);
  }
  proto_ratings->set_num_users(N_);
  proto_ratings->set_num_items(M_);
  for (uint32_t c = 0; c < criteria_size_; ++c) {
    proto_ratings->add_minv(minv_[c]);
    proto_ratings->add_maxv(maxv_[c]);
    proto_ratings->add_precision(static_cast<Ratings_Precision>(
        precision_[c]));
  }
  for (uint32_t r = 0; r < ratings_.size(); ++r) {
    mcfs::protos::Rating * rating = proto_ratings->add_rating();
    rating->set_user(ratings_[r].user); // Set user
    rating->set_item(ratings_[r].item); // Set item
    // Set criteria scores
    rating->mutable_score()->Reserve(criteria_size_);
    for (uint32_t c = 0; c < criteria_size_; ++c) {
      rating->add_score(ratings_[r].scores[c]);
    }
  }
}

std::string Dataset::info(uint32_t npr) const {
  const size_t buff_size = 50;
  char buff[buff_size];
  std::string msg;
  // Number of users
  msg += "Users: ";
  snprintf(buff, buff_size, "%u\n", N_);
  msg += buff;
  // Number of items
  msg += "Items: ";
  snprintf(buff, buff_size, "%u\n", M_);
  msg += buff;
  // Number of ratings
  msg += "Ratings: ";
  snprintf(buff, buff_size, "%u\n", static_cast<uint32_t>(ratings_.size()));
  msg += buff;
  // Number of scores per ratings
  msg += "Criteria size: ";
  snprintf(buff, buff_size, "%u\n", criteria_size_);
  msg += buff;
  // Print min. value of each criterion
  msg += "Min. value:";
  for (uint32_t c = 0; c < criteria_size_; ++c) {
    snprintf(buff, buff_size, " %f", minv_[c]);
    msg += buff;
  }
  msg += "\n";
  // Print max. value of each criterion
  msg += "Max. value:";
  for (uint32_t c = 0; c < criteria_size_; ++c) {
    snprintf(buff, buff_size, " %f", maxv_[c]);
    msg += buff;
  }
  msg += "\n";
  // Print precision of each criterion
  msg += "Precision:";
  for (uint32_t c = 0; c < criteria_size_; ++c) {
    const std::string label[] = {" FLOAT", " INT"};
    msg += label[precision_[c]];
  }
  // Print random ratings
  std::uniform_int_distribution<uint32_t> rand(0, ratings_.size()-1);
  for (; npr > 0; --npr) {
    msg += "\n";
    const uint32_t r = rand(PRNG);
    snprintf(
        buff, buff_size, "[%u] %u %u", r, ratings_[r].user, ratings_[r].item);
    msg += buff;
    for (uint32_t c = 0; c < criteria_size_; ++c) {
      snprintf(buff, buff_size, " %f", ratings_[r].scores[c]);
      msg += buff;
    }
  }
  return msg;
}

void Dataset::shuffle(bool prepare) {
  std::uniform_int_distribution<uint32_t> rand(0, ratings_.size()-1);
  for(Rating& r1 : ratings_) {
    Rating& r2 = ratings_[rand(PRNG)];
    std::swap(r1, r2);
  }
  if (prepare) {
    prepare_aux();
  }
}

void Dataset::partition(Dataset* original, Dataset* partition, float f) {
  original->shuffle(false);
  partition->clear();
  const uint32_t new_size_o = static_cast<uint32_t>(f * original->ratings_.size());
  const uint32_t new_size_p = original->ratings_.size() - new_size_o;
  DLOG(INFO) << "Old size: " << original->ratings_.size();
  DLOG(INFO) << "New size: " << new_size_o;
  DLOG(INFO) << "Partition size: " << new_size_p;
  partition->criteria_size_ = original->criteria_size_;
  partition->N_ = original->N_;
  partition->M_ = original->M_;
  partition->minv_ = original->minv_;
  partition->maxv_ = original->maxv_;
  partition->precision_ = original->precision_;
  partition->ratings_.resize(new_size_p);
  std::copy(original->ratings_.begin() + new_size_o, original->ratings_.end(),
            partition->ratings_.begin());
  original->ratings_.resize(new_size_o);
  if (new_size_o == 0 || new_size_p == 0) {
    LOG(WARNING) << "Some partition is empty.";
  }
  original->prepare_aux();
  partition->prepare_aux();
}

void Dataset::copy(Dataset* other, size_t i, size_t n) const {
  CHECK_NOTNULL(other);
  CHECK_LT(i, ratings_.size());
  other->clear();
  other->criteria_size_ = criteria_size_;
  other->N_ = N_;
  other->M_ = M_;
  other->minv_ = minv_;
  other->maxv_ = maxv_;
  other->precision_ = precision_;
  n = std::min(n, ratings_.size());
  other->ratings_.resize(n);
  if (n > 0) {
    std::copy(ratings_.begin() + i, ratings_.begin() + i + n,
              other->ratings_.begin());
  }
  other->prepare_aux();
}

float Dataset::rmse(const Dataset& a, const Dataset& b) {
  CHECK_EQ(a.ratings_.size(), b.ratings_.size());
  CHECK_EQ(a.criteria_size_, b.criteria_size_);
  float s = 0.0f;
  for (uint32_t r = 0; r < a.ratings_.size(); ++r) {
    for (uint32_t c = 0; c < a.criteria_size_; ++c) {
      const float d = a.ratings_[r].scores[c] - b.ratings_[r].scores[c];
      s += d * d;
    }
  }
  const uint32_t total_scores = a.ratings_.size() * a.criteria_size_;
  if (total_scores > 0) {
    return sqrtf(s / total_scores);
  } else {
    return 0.0f;
  }
}

bool Dataset::SortPRatingsByItem::operator() (
    const Rating* a, const Rating* b) const {
  CHECK_NOTNULL(a);
  CHECK_NOTNULL(b);
  return a->item < b->item;
}

bool Dataset::SortPRatingsByUser::operator() (
    const Rating* a, const Rating* b) const {
  CHECK_NOTNULL(a);
  CHECK_NOTNULL(b);
  return a->user < b->user;
}

Dataset& Dataset::operator = (const Dataset& other) {
  other.copy(this, 0, other.ratings_.size());
  return *this;
}

void Dataset::count_users() {
  N_ = 0;
  if (ratings_.size() > 0) {
    for (const Rating& rating : ratings_) {
      N_ = std::max<uint32_t>(rating.user, N_);
    }
    ++N_;
  }
}

void Dataset::count_items() {
  M_ = 0;
  if (ratings_.size() > 0) {
    for (const Rating& rating : ratings_) {
      M_ = std::max<uint32_t>(rating.item, M_);
    }
    ++M_;
  }
}

void Dataset::clear() {
  criteria_size_ = 0;
  N_ = 0;
  M_ = 0;
  ratings_.clear();
  ratings_by_user_.clear();
  ratings_by_item_.clear();
}

void Dataset::prepare_aux() {
  ratings_by_user_.clear();
  ratings_by_item_.clear();
  ratings_by_user_.resize(N_);
  ratings_by_item_.resize(M_);
  for (std::vector<Rating*>& v : ratings_by_user_) {
    v.clear();
  }
  for (std::vector<Rating*>& v : ratings_by_item_) {
    v.clear();
  }
  for (Rating& rating : ratings_) {
    ratings_by_user_[rating.user].push_back(&rating);
    ratings_by_item_[rating.item].push_back(&rating);
  }
  for (std::vector<Rating*>& v : ratings_by_user_) {
    if (v.size() > 0) {
      sort(v.begin(), v.end(), SortPRatingsByItem());
    }
  }
  for (std::vector<Rating*>& v : ratings_by_item_) {
    if (v.size() > 0) {
      sort(v.begin(), v.end(), SortPRatingsByUser());
    }
  }
}

const std::vector<Dataset::Rating*>& Dataset::ratings_by_item(uint32_t item) const {
  CHECK_LT(item, ratings_by_item_.size());
  return ratings_by_item_[item];
}

const std::vector<Dataset::Rating*>& Dataset::ratings_by_user(uint32_t user) const {
  CHECK_LT(user, ratings_by_user_.size());
  return ratings_by_user_[user];
}

void Dataset::init_minv() {
  minv_.resize(criteria_size_, INFINITY);
  for (const Rating& rating: ratings_) {
    for (uint32_t c = 0; c < criteria_size_; ++c) {
      minv_[c] = std::min(minv_[c], rating.scores[c]);
    }
  }
}
void Dataset::init_maxv() {
  maxv_.resize(criteria_size_, -INFINITY);
  for (const Rating& rating: ratings_) {
    for (uint32_t c = 0; c < criteria_size_; ++c) {
      maxv_[c] = std::max(maxv_[c], rating.scores[c]);
    }
  }
}

void Dataset::get_scores_from_common_ratings_by_users(
    uint32_t user1, uint32_t user2,
    std::vector<float> * ratings_u1,
    std::vector<float> * ratings_u2) const {
  CHECK_NOTNULL(ratings_u1);
  CHECK_NOTNULL(ratings_u2);
  ratings_u1->clear();
  ratings_u2->clear();
  // Get the common ratings
  const std::vector<Rating*>& ratings_by_u1 = ratings_by_user_[user1];
  const std::vector<Rating*>& ratings_by_u2 = ratings_by_user_[user2];
  for (uint32_t i = 0, j = 0; i < ratings_by_u1.size() &&
           j < ratings_by_u2.size(); ) {
    const Rating * rating_u1 = ratings_by_u1[i];
    const Rating * rating_u2 = ratings_by_u2[j];
    if (rating_u1->item == rating_u2->item) {
      for(uint32_t c = 0; c < criteria_size_; ++c) {
        ratings_u1->push_back(rating_u1->scores[c]);
        ratings_u2->push_back(rating_u2->scores[c]);
      }
      ++i; ++j;
    } else if (rating_u1->item < rating_u2->item) {
      ++i;
    } else {
      ++j;
    }
  }
  if (ratings_u1->size() == 0) {
    DLOG(WARNING) << "No common ratings between users "
                  << user1 << " and " << user2 << ".";
  }
}


void Dataset::to_normal_scale() {
  for (Rating& rat: ratings_) {
    for (uint32_t c = 0; c < rat.scores.size(); ++c) {
      if (maxv_[c] != minv_[c]) {
        rat.scores[c] = (rat.scores[c] - minv_[c])/(maxv_[c] - minv_[c]);
      } else {
        rat.scores[c] = 0;
      }
    }
  }
}

void Dataset::to_original_scale() {
  for (Rating& rat: ratings_) {
    for (uint32_t c = 0; c < rat.scores.size(); ++c) {
      rat.scores[c] = rat.scores[c]*(maxv_[c] - minv_[c]) + minv_[c];
      if (precision_[c] == mcfs::protos::Ratings_Precision_INT) {
        rat.scores[c] = round(rat.scores[c]);
      }
    }
  }
}

void Dataset::erase_scores() {
  for (Rating& rating : ratings_) {
    for (float& x: rating.scores) {
      x = 0.0f;
    }
  }
}
