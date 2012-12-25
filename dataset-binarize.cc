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
//
// This tool reads from the standard input a list of ratings and creates
// a dataset from that list of ratings. Each rating must be specified in
// a different line, where the first two integers are the user ID and the
// item ID respectively. The following real numbers in the line are the
// scores for each criterion. It is assumed that all the ratings have the
// same criteria size. It is also assumed that the user ID and item ID start
// from 0. Each field must be separated using spaces.
// Warning: The maximum line length is 1000 bytes.
// This tool is specially useful to convert, for instance, the MovieLens data
// to the mcfs dataset format.
//
// Example: generate-binarize -minv "1 1" -maxv "5 5"
//          -precision "INT INT" < human_data > mcfs_data

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <stdio.h>
#include <protos/ratings.pb.h>

#define MAX_LINE_SIZE 1000

using mcfs::protos::Ratings;
using mcfs::protos::Rating;
using mcfs::protos::Ratings_Precision_INT;
using mcfs::protos::Ratings_Precision_FLOAT;

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
    CHECK_NE(buffer, end_user);
    max_user = std::max(user, max_user);
    // Read item id
    char* end_item = NULL;
    uint32_t item = strtoul(end_user, &end_item, 10);
    CHECK_NE(end_user, end_item);
    max_item = std::max(item, max_item);
    // Add new rating
    Rating* rating = ratings_pb.add_rating();
    rating->set_user(user);
    rating->set_item(item);
    LOG(INFO) << "New rating: User = " << user << ", Item = " << item;
    // Read rating scores
    char* end_score = NULL;
    float score = strtof(end_item, &end_score);
    CHECK_NE(end_item, end_score);
    rating->add_score(score);
    uint32_t criteria_size_item = 1;
    LOG(INFO) << "Rating(" << user << "," << item
              << "," << criteria_size_item-1 << ") = " << score;
    do {
      char* end_score2 = NULL;
      score = strtof(end_score, &end_score2);
      if (end_score2 == end_score) { break; }
      end_score = end_score2;
      rating->add_score(score);
      ++criteria_size_item;
      LOG(INFO) << "Rating(" << user << "," << item
                << "," << criteria_size_item-1 << ") = " << score;
    } while (1);
    if (criteria_size == 0) {
      criteria_size = criteria_size_item;
    }
    CHECK_EQ(criteria_size, criteria_size_item);
  }
  ratings_pb.set_criteria_size(criteria_size);
  ratings_pb.set_num_users(max_user + 1);
  ratings_pb.set_num_items(max_item + 1);
  // Set min. values manually
  if (FLAGS_minv != "") {
    const char* ptr = FLAGS_minv.c_str();
    for (uint32_t c = 0; c < criteria_size; ++c) {
      char* end_float = NULL;
      const float v = strtof(ptr, &end_float);
      CHECK_NE(ptr, end_float);
      ptr = end_float;
      ratings_pb.add_minv(v);
      LOG(INFO) << "Min. value for criterion " << ratings_pb.minv_size() - 1
                << ": " << ratings_pb.minv(ratings_pb.minv_size() - 1);
    }
  }
  // Set max. values manually
  if (FLAGS_maxv != "") {
    const char* ptr = FLAGS_maxv.c_str();
    for (uint32_t c = 0; c < criteria_size; ++c) {
      char* end_float = NULL;
      const float v = strtof(ptr, &end_float);
      CHECK_NE(ptr, end_float);
      ptr = end_float;
      ratings_pb.add_maxv(v);
      LOG(INFO) << "Max. value for criterion " << ratings_pb.maxv_size() - 1
                << ": " << ratings_pb.maxv(ratings_pb.maxv_size() - 1);
    }
  }
  // Set precision manually
  if (FLAGS_precision != "") {
    const size_t fpl = FLAGS_precision.length();
    for (uint32_t c = 0, i = 0, j = 0; c < criteria_size; ++c, i = j) {
      while (isspace(FLAGS_precision[i]) && i < fpl) ++i;
      j = i + 1;
      CHECK_LE(j, fpl);
      while (!isspace(FLAGS_precision[j]) && j < fpl) ++j;
      std::string cprec = FLAGS_precision.substr(i, j - i);
      if (cprec == "INT") {
        ratings_pb.add_precision(Ratings_Precision_INT);
      } else if (cprec == "FLOAT") {
        ratings_pb.add_precision(Ratings_Precision_FLOAT);
      } else {
        LOG(FATAL) << "Undefined precision constant: " << cprec;
      }
      LOG(INFO) << "Precision for criterion "
                << ratings_pb.precision_size() - 1
                << ": " << cprec;
    }
  }
  // Serialize dataset to the standard output
  ratings_pb.SerializeToFileDescriptor(1);
  return 0;
}
