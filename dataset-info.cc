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
// This tool prints the information of the input dataset.
// The information includes the number of users and items in the dataset,
// the number of ratings, the number of criteria in each rating,
// the maximum and minimum value for each criterion and the precision
// of each criterion (whether it's a real number or an integer), etc.
// Additionally, this can print n random ratings from the dataset.
//
// Example: dataset-info -input data_file -seed 1234 -n 100

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <stdio.h>
#include <random>

#include <dataset.h>

DEFINE_string(input, "", "Input dataset filename");
DEFINE_uint64(seed, 0, "Pseudo-random number generator seed");
DEFINE_uint64(n, 0, "Print n random ratings");

std::default_random_engine PRNG;

int main(int argc, char ** argv) {
  // Google tools initialization
  google::InitGoogleLogging(argv[0]);
  google::SetUsageMessage(
      "This tool prints the information of the input dataset.\n"
      "Usage: " + std::string(argv[0]) + " --input dataset_file");
  google::ParseCommandLineFlags(&argc, &argv, true);
  // Check flags
  CHECK_NE(FLAGS_input, "") <<
      "An input dataset filename must be specified.";
  PRNG.seed(FLAGS_seed);

  Dataset dataset;
  dataset.load(FLAGS_input);
  printf("%s\n", dataset.info(FLAGS_n).c_str());
  return 0;
}
