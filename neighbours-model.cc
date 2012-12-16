#include <neighbours-model.h>

#include <glog/logging.h>
#include <algorithm>

#include <defines.h>
#include <dataset.h>
#include <similarities.h>
#include <fcntl.h>
#include <protos/ratings.pb.h>
#include <protos/model.pb.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

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
    LOG(ERROR) << "NeighboursModel \"" << filename << "\": Failed to open. Error: "
               << strerror(errno);
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
    LOG(ERROR) << "NeighboursModel \"" << filename << "\": Failed to open. Error: "
               << strerror(errno);
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


float NeighboursModel::train(const Dataset& train_set) {
  data_ = train_set;
  info();
  // The error on the training data is always 0.0 for this model
  return 0.0f;
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
  DLOG(INFO) << "NeighboursModel::test()";
  CHECK_NOTNULL(test_set);
  std::map<UserPair, float> users_similarity;
  // For each user_item to rate...
  for(Rating& pred_rating : *test_set) {
    // Get the users that rated the item
    const std::vector<Rating*> item_ratings =
        data_.ratings_by_item(pred_rating.item);
    if (item_ratings.size() == 0) {
      LOG(WARNING) << "Item " << pred_rating.item << " not rated before.";
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
        users_similarity[user_pair] = f;
      } else {
        f = sim_it->second;
      }
      DLOG(INFO) << "Sim(user " << pred_rating.user << ", user "
                 << data_rating->user << ") = " << f;
      if (f > 0.0) {
        std::pair<float, const Rating*> wrat (f, data_rating);
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
    if (weighted_ratings[0].first == INFINITY) {
      // Compute the predicted rating where there are users with
      // similarity = INFINITY
      // In this case, the predicted rating is the average
      // among those users.
      // 'r' stores the number of users with similarity = INFINITY
      uint32_t r = 0;
      for (; r < max_neighbours && weighted_ratings[r].first == INFINITY; ++r) {
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
      }
    } else {
      // Compute the predicted rating
      float sum_f = 0.0f;
      for (uint32_t r = 0; r < max_neighbours; ++r) {
        const float f = weighted_ratings[r].first;
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
      }
    }
  }
}

void NeighboursModel::info(bool log) const {
  if (log) {
    LOG(INFO) << "K = " << K_;
  } else {
    printf("K = %u\n", K_);
  }
  switch (similarity_code_) {
    case NeighboursModelConfig_Similarity_COSINE:
      if (log) { LOG(INFO) << "Similarity = COSINE"; }
      else { printf("Similarity = COSINE\n"); }
      break;
    case NeighboursModelConfig_Similarity_COSINE_SQRT:
      if (log) { LOG(INFO) << "Similarity = COSINE_SQRT"; }
      else { printf("Similarity = COSINE_SQRT\n"); }
      break;
    case NeighboursModelConfig_Similarity_COSINE_POW2:
      if (log) { LOG(INFO) << "Similarity = COSINE_POW2"; }
      else { printf("Similarity = COSINE_POW2\n"); }
      break;
    case NeighboursModelConfig_Similarity_COSINE_EXPO:
      if (log) { LOG(INFO) << "Similarity = COSINE_EXPO"; }
      else { printf("Similarity = COSINE_EXPO\n"); }
      break;
    case NeighboursModelConfig_Similarity_INV_NORM_P1:
      if (log) { LOG(INFO) << "Similarity = INV_NORM_P1"; }
      else { printf("Similarity = INV_NORM_P1\n"); }
      break;
    case NeighboursModelConfig_Similarity_INV_NORM_P2:
      if (log) { LOG(INFO) << "Similarity = INV_NORM_P2"; }
      else { printf("Similarity = INV_NORM_P2\n"); }
      break;
    case NeighboursModelConfig_Similarity_INV_NORM_PI:
      if (log) { LOG(INFO) << "Similarity = INV_NORM_PI"; }
      else { printf("Similarity = INV_NORM_PI\n"); }
      break;
    case NeighboursModelConfig_Similarity_I_N_P1_EXPO:
      if (log) { LOG(INFO) << "Similarity = I_N_P1_EXPO"; }
      else { printf("Similarity = I_N_P1_EXPO\n"); }
      break;
    case NeighboursModelConfig_Similarity_I_N_P2_EXPO:
      if (log) { LOG(INFO) << "Similarity = I_N_P2_EXPO"; }
      else { printf("Similarity = I_N_P2_EXPO\n"); }
      break;
    case NeighboursModelConfig_Similarity_I_N_PI_EXPO:
      if (log) { LOG(INFO) << "Similarity = I_N_PI_EXPO"; }
      else { printf("Similarity = I_N_PI_EXPO\n"); }
      break;
    default:
      LOG(ERROR) << "Unknown similarity code " << similarity_code_;
  }
  data_.info(0, log);
}
