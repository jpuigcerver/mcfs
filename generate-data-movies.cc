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
// This file generates data simuating the Yahoo! Movies dataset.
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

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <stdio.h>
#include <tnt/tnt.h>

#include <random>

DEFINE_uint64(users, 0, "Number of users in the dataset");
DEFINE_uint64(movies, 0, "Number of movies in the dataset");
DEFINE_double(fratings, 0.0, "Ratio of ratings to generate");
DEFINE_int64(seed, 0, "Pseudo-random number generator seed");
DEFINE_bool(ascii, true, "Output the ratings in ASCII format");

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
  // Generate data
  if (FLAGS_ascii) {
    printf("# ASCII\n# 5\n");
  } else {
    printf("# BINARY\n# 5\n");
  }
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
        float final_ratings[5];
        for (int r = 0; r < 5; ++r) {
          cratings[0][r] = cratings[0][r] * STDDEVS[r] + AVERAGES[r];
          final_ratings[r] = round(
              std::max(1.0f, std::min(13.0f, cratings[0][r])));
        }
        if (FLAGS_ascii) {
          printf("%u %u %f %f %f %f %f\n", u, m, final_ratings[0],
                 final_ratings[1], final_ratings[2], final_ratings[3],
                 final_ratings[4]);
        } else {
          uint32_t user_le = htonl(u);
          uint32_t item_le = htonl(m);
          fwrite(&user_le, 4, 1, stdout);
          fwrite(&item_le, 4, 1, stdout);
          fwrite(&final_ratings, 4, 5, stdout);
        }
      }
    }
  }
  return 0;
}
