#include <neighbours-model.h>

#include <glog/logging.h>
#include <algorithm>

#include <dataset.h>
#include <similarities.h>
#include <fcntl.h>

struct SortPRatingsByItem {
  bool operator() (const Rating * a, const Rating * b) const {
    CHECK_NOTNULL(a);
    CHECK_NOTNULL(b);
    return a->item() < b->item();
  }
};

struct SortPRatingsByUser {
  bool operator() (const Rating * a, const Rating * b) const {
    CHECK_NOTNULL(a);
    CHECK_NOTNULL(b);
    return a->user() < b->user();
  }
};

bool NeighboursModel::load(const char * filename) {
  int fd = open(filename, O_RDONLY);
  if (config.ParseFromFileDescriptor(fd)) {
    return false;
  }
  close(fd);

  return true;
}

bool NeighboursModel::save(const char * filename, bool ascii) const {
  int fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (fd < 0) {
    LOG(ERROR) << "NeighboursModel \"" << filename << "\": Failed to open. Msg: "
               << strerror(errno);
    return false;
  }
  if (ascii) {
    std::string sdata = config.DebugString();
    if (write(fd, sdata.c_str(), sdata.length()) < 0) {
      LOG(ERROR) << "NieghboursModel \"" << filename << "\": Failed to write. Msg: "
                 << strerror(errno);
      return false;
    }
    return false;
  } else {
    config.SerializeToFileDescriptor(fd);
  }
  close(fd);
  return true;
}

float NeighboursModel::train(const Dataset& train_set) {
  return 0.0f;
}

float NeighboursModel::train(
    const Dataset& train_set, const Dataset& valid_set) {
  config.mutable_ratings()->Clear();
  CopyRatings(train_set.ratings, config.mutable_ratings());
  data_ = config.mutable_ratings();
  for(int i = 0; i < data_->rating_size(); ++i) {
    const Rating& rating = data_->rating(i);
    // Add user rating
    {
      auto it = user_ratings_.find(rating.user());
      if (it == user_ratings_.end()) {
        vpc_ratings_t ratings_vec;
        ratings_vec.push_back(&rating);
        user_ratings_[rating.user()] = ratings_vec;
      } else {
        it->second.push_back(&rating);
      }
    }
    // Add item rating
    {
      auto it = item_ratings_.find(rating.item());
      if (it == item_ratings_.end()) {
        vpc_ratings_t ratings_vec;
        ratings_vec.push_back(&rating);
        item_ratings_[rating.item()] = ratings_vec;
      } else {
        it->second.push_back(&rating);
      }
    }
  }
  // Sort user ratings by item order
  for(auto& ratings: user_ratings_) {
    sort(ratings.second.begin(), ratings.second.end(), SortPRatingsByItem());
  }
  // Sort item ratings by user order
  for(auto& ratings: item_ratings_) {
    sort(ratings.second.begin(), ratings.second.end(), SortPRatingsByUser());
  }
  // Test on validation set
  return test(valid_set);
}

float NeighboursModel::test(const Dataset& test_set) const {
  Ratings pred_ratings = test(test_set.ratings);
  return RMSE(test_set.ratings, pred_ratings);
}

class UserPair {
 public:
  UserPair(uint32_t u1, uint32_t u2) {
    if (u1 < u2) {
      this->u1 = u1;
      this->u2 = u2;
    } else {
      this->u2 = u1;
      this->u1 = u2;
    }
  }
  bool operator < (const UserPair& o) const {
    return (u1 < o.u1 || (u1 == o.u1 && u2 < o.u2));
  }
  bool operator == (const UserPair& o) const {
    return (u1 == o.u1 && u2 == o.u2);
  }
  bool operator != (const UserPair& o) const {
    return (u1 != o.u1 || u2 != o.u2);
  }
  uint32_t first() const {
    return u1;
  }
  uint32_t second() const {
    return u2;
  }
 private:
  uint32_t u1;
  uint32_t u2;
};

Ratings NeighboursModel::test(const Ratings& test_set) const {
  Ratings result;
  std::map<UserPair,float> users_similarity;
  // For each user_item to rate...
  for(int i = 0; i < test_set.rating_size(); ++i) {
    // User & Item rating to predict
    const uint32_t user = test_set.rating(i).user();
    const uint32_t item = test_set.rating(i).item();
    // Prepare result rating
    Rating * pred_rating = result.add_rating();
    for (uint64_t c = 0; c < data_->criteria_size(); ++c) {
      pred_rating->add_rating(0.0f);
    }
    // Get the users that rated the item
    auto it = item_ratings_.find(item);
    if (it == item_ratings_.end()) {
      LOG(WARNING) << "Item " << item << " not rated before.";
      continue;
    }
    const vpc_ratings_t& item_ratings = it->second;
    std::vector<float> v_u;
    std::vector<float> v_i;
    std::vector<std::pair<float, const Rating *> > similarities_by_user;
    similarities_by_user.reserve(item_ratings.size());
    // For each rating of the item in the dataset...
    for (const Rating * rating : item_ratings) {
      UserPair user_pair(user, rating->user());
      float f = 0.0;
      auto sim_it = users_similarity.find(user_pair);
      if (sim_it == users_similarity.end()) {
        // Get the common ratings between the test user and the rating owner
        auto common_ratings = get_common_ratings(user, rating->user());
        if (common_ratings.size() == 0) {
          continue;
        }
        // Create a n-dimensional vector from the common ratings
        nd_vectors_from_common_ratings(common_ratings, &v_u, &v_i);
        f = (*similarity)(v_u, v_i);
        users_similarity[user_pair] = f;
      } else {
        f = sim_it->second;
      }
      similarities_by_user.push_back(
          std::pair<float, const Rating *>(f, rating));
      DLOG(INFO) << "Sim(user " << user << ", user "
                 << rating->user() << ") = " << f;
    }
    std::sort(similarities_by_user.begin(), similarities_by_user.end(),
              std::greater<std::pair<float, const Rating *> >());
    const uint64_t max_neighbours =
        config.k() == 0 ? similarities_by_user.size() : config.k();
    float sum_f = 0.0f;
    for (uint64_t n = 0; n < max_neighbours; ++n) {
      const Rating * neighbour_rating = similarities_by_user[n].second;
      const float neighbour_f = similarities_by_user[n].first;
      CHECK_EQ(data_->criteria_size(), neighbour_rating->rating_size());
      for (uint64_t c = 0; c < data_->criteria_size(); ++c) {
        const float curr_rating_c = pred_rating->rating(c);
        pred_rating->set_rating(
            c, curr_rating_c + neighbour_rating->rating(c) * neighbour_f);
      }
      sum_f += neighbour_f;
    }
    if ( sum_f == 0.0 ) {
      LOG(WARNING) << "User " << user << " have not any common rating "
                   << "with users that rated item " << item << ".";
    } else {
      for (uint64_t c = 0; c < data_->criteria_size(); ++c) {
        const float scaled_rating_c = pred_rating->rating(c) / sum_f;
        pred_rating->set_rating(c, scaled_rating_c);
      }
    }
  }
  return result;
}

void NeighboursModel::nd_vectors_from_common_ratings(
    const common_ratings_t& common_ratings,
    std::vector<float> * va, std::vector<float> * vb) const {
  CHECK_NOTNULL(va);
  CHECK_NOTNULL(vb);
  va->clear(); vb->clear();
  va->reserve(common_ratings.size());
  vb->reserve(common_ratings.size());
  for (const auto& rating_pair : common_ratings) {
    for (uint64_t c = 0; c < data_->criteria_size(); ++c) {
      va->push_back(rating_pair.first->rating(c));
      vb->push_back(rating_pair.second->rating(c));
    }
  }
}

NeighboursModel::common_ratings_t
NeighboursModel::get_common_ratings(uint32_t a, uint32_t b) const {
  common_ratings_t result;
  auto it_a = user_ratings_.find(a);
  auto it_b = user_ratings_.find(b);
  // Check whether users have some rating.
  if (it_a == user_ratings_.end()) {
    DLOG(WARNING) << "User " << a << " has no ratings.";
    return result;
  }
  if (it_b == user_ratings_.end()) {
    DLOG(WARNING) << "User " << b << " has no ratings.";
    return result;
  }
  // Get the common ratings
  const vpc_ratings_t& ratings_a = it_a->second;
  const vpc_ratings_t& ratings_b = it_b->second;
  for(auto r_a = ratings_a.begin(), r_b = ratings_b.begin();
      r_a != ratings_a.end() && r_b != ratings_b.end(); ) {
    if ((*r_a)->item() == (*r_b)->item()) {
      result.push_back(ppc_rating_t(*r_a, *r_b));
      ++r_a; ++r_b;
    } else if ((*r_a)->item() < (*r_b)->item()) {
      ++r_a;
    } else {
      ++r_b;
    }
  }
  if (result.size() == 0) {
    DLOG(WARNING) << "No common ratings between users "
                  << a << " and " << b << ".";
  }
  return result;
}
