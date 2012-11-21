#include <neighbours-model.h>

#include <glog/logging.h>
#include <algorithm>

#include <dataset.h>
#include <similarities.h>

struct SortPRatingsByItem {
  bool operator() (const Rating * a, const Rating * b) const {
    CHECK_NOTNULL(a);
    CHECK_NOTNULL(b);
    return a->item < b->item;
  }
};

struct SortPRatingsByUser {
  bool operator() (const Rating * a, const Rating * b) const {
    CHECK_NOTNULL(a);
    CHECK_NOTNULL(b);
    return a->user < b->user;
  }
};

void NeighboursModel::train(
    const Dataset& train_set, const Dataset& valid_set) {
  data_ = train_set;
  for(auto& rating : data_.ratings) {
    // Add user rating
    {
      auto it = user_ratings_.find(rating.user);
      if (it == user_ratings_.end()) {
        vpc_ratings_t ratings_vec;
        ratings_vec.push_back(&rating);
        user_ratings_[rating.user] = ratings_vec;
      } else {
        it->second.push_back(&rating);
      }
    }
    // Add item rating
    {
      auto it = item_ratings_.find(rating.item);
      if (it == item_ratings_.end()) {
        vpc_ratings_t ratings_vec;
        ratings_vec.push_back(&rating);
        item_ratings_[rating.item] = ratings_vec;
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
  similarity = CosineSimilarity();
}

float NeighboursModel::test(const Dataset& test_set) const {
  std::vector<user_item_t> user_items;
  user_items.reserve(test_set.ratings.size());
  for(const Rating& rating : test_set.ratings) {
    user_items.push_back(user_item_t(rating.user, rating.item));
  }
  std::vector<Rating> pred_ratings = test(user_items);
  return RMSE(test_set.ratings, pred_ratings);
}

std::vector<Rating> NeighboursModel::test(
    const std::vector<user_item_t>& users_items) const {
  std::vector<Rating> result;
  // For each user_item to rate...
  for(const auto& user_item : users_items) {
    const uint32_t user = user_item.first;
    const uint32_t item = user_item.second;
    Rating pred_rating(user, item, data_.criteria_size);
    // Get the users that rated the item
    auto it = item_ratings_.find(item);
    if (it == item_ratings_.end()) {
      LOG(WARNING) << "Item " << item << " not rated before.";
      result.push_back(pred_rating);
      continue;
    }
    const vpc_ratings_t& item_ratings = it->second;
    std::vector<float> v_u;
    std::vector<float> v_i;
    float sum_f = 0.0f;
    // For each rating of the item ...
    for (const auto& rating : item_ratings) {
      // Get the common ratings between the test user and the rating owner
      auto common_ratings = get_common_ratings(user, rating->user);
      if (common_ratings.size() == 0) {
        continue;
      }
      // Create a n-dimensional vector from the common ratings
      nd_vectors_from_common_ratings(common_ratings, &v_u, &v_i);
      float f = similarity(v_u, v_i);
      sum_f += f;
      for (uint64_t c = 0; c < data_.criteria_size; ++c) {
        pred_rating.c_rating[c] += rating->c_rating[c] * f;
      }
    }
    if ( sum_f == 0.0 ) {
      LOG(WARNING) << "User " << user << " have not any common rating "
                   << "with users that rated item " << item << ".";
    } else {
      for (uint64_t c = 0; c < data_.criteria_size; ++c) {
        pred_rating.c_rating[c] /= sum_f;
      }
    }
    result.push_back(pred_rating);
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
    for (uint64_t c = 0; c < data_.criteria_size; ++c) {
      va->push_back(rating_pair.first->c_rating[c]);
      vb->push_back(rating_pair.second->c_rating[c]);
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
    if ((*r_a)->item == (*r_b)->item) {
      result.push_back(ppc_rating_t(*r_a, *r_b));
      ++r_a; ++r_b;
    } else if ((*r_a)->item < (*r_b)->item) {
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
