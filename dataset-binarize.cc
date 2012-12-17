#include <gflags/gflags.h>
#include <glog/logging.h>
#include <stdio.h>
#include <protos/ratings.pb.h>

#define MAX_LINE_SIZE 1000

using mcfs::protos::Ratings;
using mcfs::protos::Rating;

DEFINE_string(precision, "", "Precision of each criteria");
DEFINE_string(minv, "", "Min. value in each criteria");
DEFINE_string(maxv, "", "Max. value in each criteria");

int main(int argc, char ** argv) {
  // Google tools initialization
  google::InitGoogleLogging(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, true);

  Ratings ratings_pb;
  uint32_t criteria_size = 0;
  char buffer[MAX_LINE_SIZE];
  uint32_t max_user = 0;
  uint32_t max_item = 0;
  while (fgets(buffer, MAX_LINE_SIZE, stdin)) {
    // Read user id
    char* end_user = NULL;
    uint32_t user = strtoul(buffer, &end_user, 10);
    CHECK_NOTNULL(end_user);
    max_user = std::max(user, max_user);
    // Read item id
    char* end_item = NULL;
    uint32_t item = strtoul(end_user, &end_item, 10);
    CHECK_NOTNULL(end_item);
    max_item = std::max(item, max_item);
    // Add new rating
    Rating* rating = ratings_pb.add_rating();
    rating->set_user(user);
    rating->set_item(item);
    LOG(INFO) << "New rating: User = " << user << ", Item = " << item;
    // Read rating scores
    char* end_score = NULL;
    float score = strtof(end_item, &end_score);
    CHECK_NOTNULL(end_score);
    rating->add_score(score);
    uint32_t criteria_size_item = 1;
    LOG(INFO) << "Rating(" << user << "," << item
              << "," << criteria_size_item-1 << ") = " << score;
    do {
      char* end_score2 = NULL;
      score = strtof(end_score, &end_score);
      if (end_score2 == NULL) { break; }
      end_score = end_score2;
      rating->add_score(score);
      LOG(INFO) << "Rating(" << user << "," << item
                << "," << criteria_size_item-1 << ") = " << score;
      ++criteria_size_item;
    } while (1);
    if (criteria_size == 0) {
      criteria_size = criteria_size_item;
    }
    CHECK_EQ(criteria_size, criteria_size_item);
  }
  ratings_pb.set_criteria_size(criteria_size);
  ratings_pb.set_num_users(max_user + 1);
  ratings_pb.set_num_items(max_item + 1);
  if (FLAGS_minv != "") {
    const char* ptr = FLAGS_minv.c_str();
    for (uint32_t c = 0; c < criteria_size; ++c) {
      char* end_float = NULL;
      const float v = strtof(ptr, &end_float);
      CHECK_NOTNULL(end_float);
      ptr = end_float;
      ratings_pb.add_minv(v);
    }
  }
  if (FLAGS_maxv != "") {
    const char* ptr = FLAGS_maxv.c_str();
    for (uint32_t c = 0; c < criteria_size; ++c) {
      char* end_float = NULL;
      const float v = strtof(ptr, &end_float);
      CHECK_NOTNULL(end_float);
      ptr = end_float;
      ratings_pb.add_maxv(v);
    }
  }
  ratings_pb.SerializeToFileDescriptor(1);
  return 0;
}
