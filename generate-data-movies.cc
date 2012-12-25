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
// This tool generates data simuating the Yahoo! Movies dataset.
// In the Yahoo! Movies dataset each user provides a 5-criteria rating
// for each movie. The following assumptions were made to generate the data:
//   - Author i rates movie j with uniformly distributed probability fratings
//   - Author i provides a complete rating (all the criteria) for movie j
//   - Each rating is distributed normally.
//   - Rating criteria is correlated.
// Means and Person correlations were obtained from
// http://www.lamsade.dauphine.fr/~tsoukias/papers/Lakiotakietal.pdf [1]
// Since this paper does not provide the standard deviation for the ratings,
// the standard deviation of the MovieLens data set was scaled to the range
// 1..13 (instead of the original 1..5) and each criterion is assumed to have
// this standard deviation.
//
// Example: generate-data-movies -users 100 -movies 100 -fratings 0.1 -seed 1234

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <tnt/tnt.h>
#include <random>

#include <protos/ratings.pb.h>

using google::protobuf::TextFormat;
using google::protobuf::io::FileOutputStream;
using mcfs::protos::Ratings;
using mcfs::protos::Rating;
using mcfs::protos::Ratings_Precision_FLOAT;
using mcfs::protos::Ratings_Precision_INT;

DEFINE_uint64(users, 0, "Number of users in the dataset");
DEFINE_uint64(movies, 0, "Number of movies in the dataset");
DEFINE_double(fratings, 0.0, "Ratio of ratings to generate");
DEFINE_uint64(seed, 0, "Pseudo-random number generator seed");
DEFINE_bool(ascii, false, "Output the ratings in ASCII format");
DEFINE_bool(zos, false, "Output data in range 0..1");

// Averages provided by [1].
float AVERAGES[5] = {9.6, 9.9, 9.5, 10.5, 9.5};
// Std. deviation from scaled MovieLens std. dev.
float STDDEVS[5] = {2.90446, 2.90446, 2.90446, 2.90446, 2.90446};
// Cholesky decomposition of the correlation matrix provided by [1].
float CORR_L[25] = {1, 0.834, 0.871, 0.782, 0.905,
                    0, 0.5518, 0.2367, 0.2425, 0.1998,
                    0, 0, 0.4305, 0.2241, 0.1753,
                    0, 0, 0, 0.5286, 0.0729,
                    0, 0, 0, 0, 0.3241};
TNT::Array2D<float> CORR_L_TNT(5, 5, CORR_L);

int main(int argc, char ** argv) {
  // Google tools initialization
  google::InitGoogleLogging(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, true);
  // Check number of users, movies and ratings ratio.
  CHECK_GT(FLAGS_users, 0) <<
      "The number of users must be greater than zero.";
  CHECK_GT(FLAGS_movies, 0) <<
      "The number of movies must be greater than zero.";
  CHECK_GT(FLAGS_fratings, 0.0) <<
      "The ratio of ratings be greater than zero.";
  // Random generators
  std::default_random_engine rndg(FLAGS_seed);
  std::uniform_real_distribution<float> unif_dist(0.0, 1.0);
  std::normal_distribution<float> norm_dist(0.0, 1.0);
  Ratings ratings_pb;

  for (uint32_t u = 0; u < FLAGS_users; ++u) {
    for (uint32_t m = 0; m < FLAGS_movies; ++m) {
      if ( unif_dist(rndg) < FLAGS_fratings ) {
        // Ratings normally distributed with mean = 0 and dev = 1.
        TNT::Array2D<float> ratings(1, 5);
        for (int r = 0; r < 5; ++r) {
          ratings[0][r] = norm_dist(rndg);
        }
        // Correlated ratings.
        TNT::Array2D<float> cratings = TNT::matmult(ratings, CORR_L_TNT);
        // Ratings distributed with the corret mean and dev, and restricted
        // to range 1..13.
        Rating * final_rating = ratings_pb.add_rating();
        final_rating->set_user(u);
        final_rating->set_item(m);
        for (int r = 0; r < 5; ++r) {
          final_rating->add_score(cratings[0][r] * STDDEVS[r] + AVERAGES[r]);
        }
      }
    }
  }
  float minr[5] = {INFINITY, INFINITY, INFINITY, INFINITY, INFINITY};
  float maxr[5] = {-INFINITY, -INFINITY, -INFINITY, -INFINITY, -INFINITY};
  for (int i = 0; i < ratings_pb.rating_size(); ++i) {
    for (int j = 0; j < 5; ++j) {
      minr[j] = std::min(minr[j], ratings_pb.rating(i).score(j));
      maxr[j] = std::max(maxr[j], ratings_pb.rating(i).score(j));
    }
  }
  for (int i = 0; i < ratings_pb.rating_size(); ++i) {
    for (int j = 0; j < 5; ++j) {
      const float nrating =
          (ratings_pb.rating(i).score(j) - minr[j]) / (maxr[j] - minr[j]);
      ratings_pb.mutable_rating(i)->set_score(j, nrating);
      if (!FLAGS_zos) {
        ratings_pb.mutable_rating(i)->set_score(
            j, round(ratings_pb.rating(i).score(j) * 12.0f + 1.0f));
      }
    }
  }
  ratings_pb.set_criteria_size(5);
  ratings_pb.set_num_users(FLAGS_users);
  ratings_pb.set_num_items(FLAGS_movies);
  for (int i = 0; i < 5; ++i) {
    if (FLAGS_zos) {
      ratings_pb.add_minv(0.0f);
      ratings_pb.add_maxv(1.0f);
      ratings_pb.add_precision(Ratings_Precision_FLOAT);
    } else {
      ratings_pb.add_minv(1.0f);
      ratings_pb.add_maxv(13.0f);
      ratings_pb.add_precision(Ratings_Precision_INT);
    }
  }
  if (FLAGS_ascii) {
    FileOutputStream fs(1);
    TextFormat::Print(ratings_pb, &fs);
  } else {
    ratings_pb.SerializeToFileDescriptor(1);
  }
  return 0;
}
