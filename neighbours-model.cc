// Copyright 2012 Joan Puigcerver <joapuipe@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <neighbours-model.h>

#include <dataset.h>
#include <defines.h>
#include <fcntl.h>
#include <glog/logging.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>
#include <protos/model.pb.h>
#include <protos/ratings.pb.h>
#include <similarities.h>

#include <algorithm>
#include <functional>
#include <map>
#include <utility>

using google::protobuf::TextFormat;
using google::protobuf::io::FileInputStream;
using google::protobuf::io::FileOutputStream;

using mcfs::protos::ModelConfig;
using mcfs::protos::NeighboursModelConfig;
using mcfs::protos::NeighboursModelConfig_Similarity_COSINE;
using mcfs::protos::NeighboursModelConfig_Similarity_COSINE_SQRT;
using mcfs::protos::NeighboursModelConfig_Similarity_COSINE_POW2;
using mcfs::protos::NeighboursModelConfig_Similarity_COSINE_EXPO;
using mcfs::protos::NeighboursModelConfig_Similarity_INV_NORM_P1;
using mcfs::protos::NeighboursModelConfig_Similarity_INV_NORM_P2;
using mcfs::protos::NeighboursModelConfig_Similarity_INV_NORM_PI;
using mcfs::protos::NeighboursModelConfig_Similarity_I_N_P1_EXPO;
using mcfs::protos::NeighboursModelConfig_Similarity_I_N_P2_EXPO;
using mcfs::protos::NeighboursModelConfig_Similarity_I_N_PI_EXPO;
using mcfs::protos::Ratings_Precision_INT;


bool NeighboursModel::load_string(const std::string& str) {
  NeighboursModelConfig config;
  if (!TextFormat::ParseFromString(str, &config)) {
    LOG(ERROR) << "NeighboursModel: Failed to parse.";
    return false;
  }
  return load(config);
}

bool NeighboursModel::save_string(std::string * str) const {
  CHECK_NOTNULL(str);
  NeighboursModelConfig config;
  save(&config);
  return TextFormat::PrintToString(config, str);
}

bool NeighboursModel::load(const std::string& filename) {
  NeighboursModelConfig config;
  int fd = open(filename.c_str(), O_RDONLY);
  if (fd < 0) {
    LOG(ERROR) << "NeighboursModel \"" << filename
               << "\": Failed to open. Error: " << strerror(errno);
    return false;
  }
  FileInputStream fs(fd);
  if (!config.ParseFromFileDescriptor(fd) &&
      !TextFormat::Parse(&fs, &config)) {
    LOG(ERROR) << "NeighboursModel \"" << filename << "\": Failed to parse.";
    return false;
  }
  close(fd);
  return load(config);
}

bool NeighboursModel::load(const NeighboursModelConfig& config) {
  if (!data_.load(config.ratings())) {
    return false;
  }
  K_ = config.k();
  similarity_code_ = config.similarity();
  switch (similarity_code_) {
    case NeighboursModelConfig_Similarity_COSINE:
      similarity_ = &StaticCosineSimilarity;
      break;
    case NeighboursModelConfig_Similarity_COSINE_SQRT:
      similarity_ = &StaticCosineSqrtSimilarity;
      break;
    case NeighboursModelConfig_Similarity_COSINE_POW2:
      similarity_ = &StaticCosinePow2Similarity;
      break;
    case NeighboursModelConfig_Similarity_COSINE_EXPO:
      similarity_ = &StaticCosineExpSimilarity;
      break;
    case NeighboursModelConfig_Similarity_INV_NORM_P1:
      similarity_ = &StaticNormSimilarityP1;
      break;
    case NeighboursModelConfig_Similarity_INV_NORM_P2:
      similarity_ = &StaticNormSimilarityP2;
      break;
    case NeighboursModelConfig_Similarity_INV_NORM_PI:
      similarity_ = &StaticNormSimilarityPI;
      break;
    case NeighboursModelConfig_Similarity_I_N_P1_EXPO:
      similarity_ = &StaticNormExpSimilarityP1;
      break;
    case NeighboursModelConfig_Similarity_I_N_P2_EXPO:
      similarity_ = &StaticNormExpSimilarityP2;
      break;
    case NeighboursModelConfig_Similarity_I_N_PI_EXPO:
      similarity_ = &StaticNormExpSimilarityPI;
      break;
    default:
      LOG(ERROR) << "Unknown similarity code " << similarity_code_;
      return false;
  }
  return true;
}

bool NeighboursModel::save(NeighboursModelConfig * config) const {
  if (data_.ratings_size() > 0) {
    data_.save(config->mutable_ratings());
  }
  config->set_k(K_);
  config->set_similarity(similarity_code_);
  return true;
}

bool NeighboursModel::save(const std::string& filename) const {
  NeighboursModelConfig config;
  if (!save(&config)) {
    return false;
  }
  // Write protocol buffer
  int fd = open(filename.c_str(), O_CREAT | O_WRONLY | O_TRUNC,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (fd < 0) {
    LOG(ERROR) << "NeighboursModel \"" << filename
               << "\": Failed to open. Error: " << strerror(errno);
    return false;
  }
  FileOutputStream fs(fd);
  if (!config.SerializeToFileDescriptor(fd)) {
    LOG(ERROR) << "NeighboursModel \"" << filename << "\": Failed to write.";
    return false;
  }
  close(fd);
  return true;
}


float NeighboursModel::train(const Dataset& train_set,
                             const Dataset& valid_set) {
  data_ = train_set;
  LOG(INFO) << "Model config:\n" << info();
  const float valid_rmse = Model::test(valid_set);
  // The error on the training data is always 0.0 for this model
  LOG(INFO) << "Train RMSE = " << 0.0 << ", Valid RMSE = " << valid_rmse;
  return valid_rmse;
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

void NeighboursModel::test(std::vector<Rating>* test_set) const {
  CHECK_NOTNULL(test_set);
  std::map<UserPair, float> users_similarity;
  // For each user_item to rate...
  for (auto it = test_set->begin(); it != test_set->end(); ++it) {
    Rating& pred_rating = *it;
    // Get the users that rated the item
    const std::vector<Rating*>& item_ratings =
        data_.ratings_by_item(pred_rating.item);
    if (item_ratings.size() == 0) {
      LOG(WARNING) << "Item " << pred_rating.item << " not rated before.";
      for (uint32_t c = 0; c < data_.criteria_size(); ++c) {
        pred_rating.scores[c] = (data_.maxv(c) - data_.minv(c)) / 2.0f;
      }
      continue;
    }
    // For each rating of the item ...
    std::vector<std::pair<float, const Rating*> > weighted_ratings;
    weighted_ratings.reserve(item_ratings.size());
    const Rating* exact_match = NULL;
    for (const Dataset::Rating* data_rating : item_ratings) {
      if (data_rating->user == pred_rating.user) {
        exact_match = data_rating;
        break;
      }
      UserPair user_pair(pred_rating.user, data_rating->user);
      float f = 0.0;
      auto sim_it = users_similarity.find(user_pair);
      if (sim_it == users_similarity.end()) {
        // Get the common ratings between the test user and the rating owner
        std::vector<float> v_u;
        std::vector<float> v_i;
        data_.get_scores_from_common_ratings_by_users(
            pred_rating.user, data_rating->user, &v_u, &v_i);
        // Compute similarity between users
        f = (*similarity_)(v_u, v_i);
        CHECK_EQ(std::isnan(f), 0);
        users_similarity[user_pair] = f;
      } else {
        f = sim_it->second;
      }
      DLOG(INFO) << "Sim(user " << pred_rating.user << ", user "
        << data_rating->user << ") = " << f;
      if (f > 0.0) {
        std::pair<float, const Rating*> wrat(f, data_rating);
        weighted_ratings.push_back(wrat);
      }
    }
    // Check if the desired prediction was in the training set.
    if (exact_match != NULL) {
      pred_rating.scores = exact_match->scores;
      continue;
    }
    // Check if there is enough data to make the desired prediction.
    if (weighted_ratings.size() == 0) {
      LOG(WARNING) << "User " << pred_rating.user
                   << " have not any common rating"
                   << " with users that rated item "
                   << pred_rating.item << ".";
      continue;
    }
    // Sort the ratings of the item by neighbour's similarity
    std::sort(weighted_ratings.begin(), weighted_ratings.end(),
              std::greater<std::pair<float, const Rating*> >());
    // Determine the maximum number of neighbours to use in the prediction
    const uint32_t max_neighbours =
        K_ == 0 ? weighted_ratings.size() : std::min<uint32_t>(
            K_, weighted_ratings.size());
    if (std::isinf(weighted_ratings[0].first)) {
      // Compute the predicted rating where there are users with
      // similarity = INFINITY
      // In this case, the predicted rating is the average
      // among those users.
      // 'r' stores the number of users with similarity = INFINITY
      uint32_t r = 0;
      for (; r < max_neighbours && std::isinf(weighted_ratings[r].first); ++r) {
        const Rating * rat = weighted_ratings[r].second;
        for (uint32_t c = 0; c < rat->scores.size(); ++c) {
          pred_rating.scores[c] += rat->scores[c];
        }
      }
      // Normalize rating
      for (uint32_t c = 0; c < data_.criteria_size(); ++c) {
        pred_rating.scores[c] /= r;
        if (data_.precision(c) == Ratings_Precision_INT) {
          pred_rating.scores[c] = round(pred_rating.scores[c]);
        }
        CHECK_EQ(std::isinf(pred_rating.scores[c]), 0);
      }
    } else {
      // Compute the predicted rating
      float sum_f = 0.0f;
      for (uint32_t r = 0; r < max_neighbours; ++r) {
        const float f = weighted_ratings[r].first / weighted_ratings[0].first;
        const Rating * rat = weighted_ratings[r].second;
        sum_f += f;
        for (uint32_t c = 0; c < rat->scores.size(); ++c) {
          pred_rating.scores[c] += rat->scores[c] * f;
        }
      }
      // Normalize rating
      for (uint32_t c = 0; c < data_.criteria_size(); ++c) {
        pred_rating.scores[c] /= sum_f;
        if (data_.precision(c) == Ratings_Precision_INT) {
          pred_rating.scores[c] = round(pred_rating.scores[c]);
        }
        CHECK_EQ(std::isinf(pred_rating.scores[c]), 0);
      }
    }
  }
}

std::string NeighboursModel::info() const {
#define BUFF_SIZE 50
  char buff[BUFF_SIZE];
  const std::string similarity_label[] = {
    "COSINE", "COSINE_SQRT", "COSINE_POW2", "COSINE_EXPO",
    "INV_NORM_P1", "INV_NORM_P2", "INV_NORM_PI",
    "I_N_P1_EXPO", "I_N_P2_EXPO", "I_N_PI_EXPO"
  };
  std::string msg;
  snprintf(buff, BUFF_SIZE, "K = %u\n", K_);
  msg += buff;
  snprintf(buff, BUFF_SIZE, "Similarity = %s\n",
           similarity_label[similarity_code_].c_str());
  msg += buff;
  msg += data_.info(0);
  return msg;
}
