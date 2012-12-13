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
  /*if (ascii) {
    LOG(ERROR) << "Dataset \"" << filename << "\": ASCII format not implemented.";
    return false;
  }*/
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
  criteria_size = proto_ratings.criteria_size();
  if (proto_ratings.rating_size() == 0) {
    return true;
  }
  // If the criteria size is unknown, set to the first.
  if (criteria_size == 0) {
    criteria_size = proto_ratings.rating(0).score_size();
  }
  ratings.resize(proto_ratings.rating_size());
  // Copy from protobuf to self-memory
  for (uint64_t r = 0; r < ratings.size(); ++r) {
    // Set user & item ID
    ratings[r].user = proto_ratings.rating(r).user();
    ratings[r].item = proto_ratings.rating(r).item();
    // Check wether the number of scores is the expected
    if (static_cast<uint64_t>(
            proto_ratings.rating(r).score_size()) != criteria_size) {
      LOG(ERROR) << "Dataset: Inconsistent criteria size. "
                 << "Size found = " << proto_ratings.rating(r).score_size()
                 << ". Size expected = " << criteria_size;
      return false;
    }
    // Set the rating scores
    const float * p_scores = proto_ratings.rating(r).score().data();
    ratings[r].scores.resize(criteria_size);
    std::copy(p_scores, p_scores + criteria_size, ratings[r].scores.begin());
  }
  // Set min value for each criteria
  if (static_cast<uint64_t>(proto_ratings.minv_size()) == criteria_size) {
    minv.resize(criteria_size);
    std::copy(proto_ratings.minv().data(),
              proto_ratings.minv().data() + criteria_size,
              minv.begin());
  } else {
    set_minv();
  }
  // Set max value for each criteria
  if (static_cast<uint64_t>(proto_ratings.maxv_size()) == criteria_size) {
    maxv.resize(criteria_size);
    std::copy(proto_ratings.maxv().data(),
              proto_ratings.maxv().data() + criteria_size,
              maxv.begin());
  } else {
    set_maxv();
  }
  // Set the precision of each criteria
  if (static_cast<uint64_t>(proto_ratings.precision_size()) == criteria_size) {
    precision.resize(criteria_size);
    std::copy(proto_ratings.precision().data(),
              proto_ratings.precision().data() + criteria_size,
              precision.begin());
  } else if (proto_ratings.precision_size() == 1) {
    precision.resize(criteria_size, proto_ratings.precision(0));
  } else {
    precision.resize(criteria_size, Ratings_Precision_FLOAT);
  }
  // Auxiliar structures are not loaded. They'll be loaded on demand.
  aux_ready = false;
  return true;
}

void Dataset::save(mcfs::protos::Ratings * proto_ratings) const {
  CHECK_NOTNULL(proto_ratings);
  // Fill protocol buffer
  proto_ratings->clear_minv();
  proto_ratings->clear_maxv();
  proto_ratings->clear_precision();
  proto_ratings->clear_rating();
  if (criteria_size > 0) {
    proto_ratings->set_criteria_size(criteria_size);
  }
  for (uint64_t c = 0; c < criteria_size; ++c) {
    proto_ratings->add_minv(minv[c]);
    proto_ratings->add_maxv(maxv[c]);
    proto_ratings->add_precision(static_cast<Ratings_Precision>(precision[c]));
  }
  for (uint64_t r = 0; r < ratings.size(); ++r) {
    mcfs::protos::Rating * rating = proto_ratings->add_rating();
    rating->set_user(ratings[r].user); // Set user
    rating->set_item(ratings[r].item); // Set item
    // Set criteria scores
    rating->mutable_score()->Reserve(criteria_size);
    for (uint64_t c = 0; c < criteria_size; ++c) {
      rating->add_score(ratings[r].scores[c]);
    }
  }
}

void Dataset::info(uint64_t npr) const {
  printf("Ratings: %lu\n", ratings.size());
  printf("Criteria size: %lu\n", criteria_size);
  // Print min. value of each criterion
  printf("Min. value:");
  for (uint64_t c = 0; c < criteria_size; ++c) {
    printf(" %f", minv[c]);
  }
  printf("\n");
  // Print max. value of each criterion
  printf("Max. value:");
  for (uint64_t c = 0; c < criteria_size; ++c) {
    printf(" %f", maxv[c]);
  }
  printf("\n");
  // Print precision of each criterion  printf("Max. value:");
  printf("Precision:");
  for (uint64_t c = 0; c < criteria_size; ++c) {
    if (precision[c] == Ratings_Precision_FLOAT) {
      printf(" FLOAT");
    } else {
      printf(" INT");
    }
  }
  printf("\n");
  // Print random ratings
  std::uniform_int_distribution<uint64_t> rand(0, ratings.size()-1);
  for (; npr > 0; --npr) {
    const uint64_t r = rand(PRNG);
    printf("[%lu] %u %u", r, ratings[r].user, ratings[r].item);
    for (uint64_t c = 0; c < criteria_size; ++c) {
      printf(" %f", ratings[r].scores[c]);
    }
    printf("\n");
  }
}

void Dataset::shuffle() {
  std::uniform_int_distribution<uint64_t> rand(0, ratings.size()-1);
  for(Rating& r1 : ratings) {
    Rating& r2 = ratings[rand(PRNG)];
    std::swap(r1, r2);
  }
  aux_ready = false;
}

void Dataset::partition(Dataset * original, Dataset * partition, float f) {
  original->shuffle();
  partition->clear();
  const uint64_t new_size_o = static_cast<uint64_t>(f * original->ratings.size());
  const uint64_t new_size_p = original->ratings.size() - new_size_o;
  DLOG(INFO) << "Old size: " << original->ratings.size();
  DLOG(INFO) << "New size: " << new_size_o;
  DLOG(INFO) << "Partition size: " << new_size_p;
  partition->criteria_size = original->criteria_size;
  partition->minv = original->minv;
  partition->maxv = original->maxv;
  partition->precision = original->precision;
  partition->ratings.resize(new_size_p);
  std::copy(original->ratings.begin() + new_size_o, original->ratings.end(),
            partition->ratings.begin());
  original->ratings.resize(new_size_o);
  if (new_size_o == 0 || new_size_p == 0) {
    LOG(WARNING) << "Some partition is empty.";
  }
}

float Dataset::rmse(const Dataset& a, const Dataset& b) {
  CHECK_EQ(a.ratings.size(), b.ratings.size());
  CHECK_EQ(a.criteria_size, b.criteria_size);
  float s = 0.0f;
  for (uint64_t r = 0; r < a.ratings.size(); ++r) {
    for (uint64_t c = 0; c < a.criteria_size; ++c) {
      const float d = a.ratings[r].scores[c] - b.ratings[r].scores[c];
      s += d * d;
    }
  }
  const uint64_t total_scores = a.ratings.size() * a.criteria_size;
  return sqrtf(s / total_scores);
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

void Dataset::prepare_aux() const {
  if (!aux_ready) {
    ratings_by_user.clear();
    ratings_by_item.clear();
    for (const Rating& rating : ratings) {
      // Create ratings by user...
      {
        const auto& it = ratings_by_user.find(rating.user);
        if (it == ratings_by_user.end()) {
          ratings_by_user[rating.user] = std::vector<const Rating*>(1, &rating);
      } else {
          it->second.push_back(&rating);
        }
      }
      // Create ratings by item...
      {
        const auto& it = ratings_by_item.find(rating.item);
        if (it == ratings_by_item.end()) {
          ratings_by_item[rating.item] = std::vector<const Rating*>(1, &rating);
        } else {
          it->second.push_back(&rating);
        }
      }
    }
    for (auto& it : ratings_by_user) {
      sort(it.second.begin(), it.second.end(), SortPRatingsByItem());
    }
    for (auto& it : ratings_by_item) {
      sort(it.second.begin(), it.second.end(), SortPRatingsByUser());
    }
    aux_ready = true;
  }
}

void Dataset::get_ratings_by_item(
    uint64_t item, const std::vector<const Rating*>** item_ratings) const {
  CHECK_NOTNULL(item_ratings);
  prepare_aux();
  const auto& it = ratings_by_item.find(item);
  if (it == ratings_by_item.end()) {
    *item_ratings = NULL;
  } else {
    *item_ratings = &(it->second);
  }
}

void Dataset::get_common_ratings_by_users(
    uint64_t user1, uint64_t user2,
    std::vector<const Rating*>* ratings_u1,
    std::vector<const Rating*>* ratings_u2) const {
  CHECK_NOTNULL(ratings_u1);
  CHECK_NOTNULL(ratings_u2);
  ratings_u1->clear();
  ratings_u2->clear();
  prepare_aux();
  const auto& it_u1 = ratings_by_user.find(user1);
  const auto& it_u2 = ratings_by_user.find(user2);
  // Check whether users have some rating.
  if (it_u1 == ratings_by_user.end()) {
    DLOG(WARNING) << "User " << user1 << " has no ratings.";
    return;
  }
  if (it_u2 == ratings_by_user.end()) {
    DLOG(WARNING) << "User " << user2 << " has no ratings.";
    return;
  }
  // Get the common ratings
  const std::vector<const Rating*>& ratings_by_u1 = it_u1->second;
  const std::vector<const Rating*>& ratings_by_u2 = it_u2->second;
  for (uint64_t i = 0, j = 0; i < ratings_by_u1.size() &&
           j < ratings_by_u2.size(); ) {
    const Rating * rating_u1 = ratings_by_u1[i];
    const Rating * rating_u2 = ratings_by_u2[j];
    if (rating_u1->item == rating_u2->item) {
      ratings_u1->push_back(rating_u1);
      ratings_u2->push_back(rating_u2);
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

void Dataset::get_scores_from_common_ratings_by_users(
    uint64_t user1, uint64_t user2,
    std::vector<float> * ratings_u1,
    std::vector<float> * ratings_u2) const {
  CHECK_NOTNULL(ratings_u1);
  CHECK_NOTNULL(ratings_u2);
  ratings_u1->clear();
  ratings_u2->clear();
  prepare_aux();
  const auto& it_u1 = ratings_by_user.find(user1);
  const auto& it_u2 = ratings_by_user.find(user2);
  // Check whether users have some rating.
  if (it_u1 == ratings_by_user.end()) {
    DLOG(WARNING) << "User " << user1 << " has no ratings.";
    return;
  }
  if (it_u2 == ratings_by_user.end()) {
    DLOG(WARNING) << "User " << user2 << " has no ratings.";
    return;
  }
  // Get the common ratings
  const std::vector<const Rating*>& ratings_by_u1 = it_u1->second;
  const std::vector<const Rating*>& ratings_by_u2 = it_u2->second;
  for (uint64_t i = 0, j = 0; i < ratings_by_u1.size() &&
           j < ratings_by_u2.size(); ) {
    const Rating * rating_u1 = ratings_by_u1[i];
    const Rating * rating_u2 = ratings_by_u2[j];
    if (rating_u1->item == rating_u2->item) {
      for(uint64_t c = 0; c < criteria_size; ++c) {
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
