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
// This tool splits a dataset in two partitions. The first partition gets
// f*100% random ratings of the original dataset, and the second one the
// remaining ratings. Very useful to create a training and test set from
// a single dataset.

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <stdio.h>
#include <dataset.h>

#include <random>

std::default_random_engine PRNG;

DEFINE_string(input, "", "Input dataset filename");
DEFINE_string(part1, "", "Partition 1 filename");
DEFINE_string(part2, "", "Partition 2 filename");
DEFINE_double(f, 0.8, "Fraction of input data used for part1 (Range: 0..1)");
DEFINE_bool(ascii, false, "Output partitions in ASCII format");
DEFINE_uint64(seed, 0, "Pseudo-random number generator seed");

int main(int argc, char ** argv) {
  // Google tools initialization
  google::InitGoogleLogging(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, true);
  CHECK_NE(FLAGS_input, "") <<
      "An input dataset filename must be specified.";
  CHECK_NE(FLAGS_part1, "") <<
      "An output filename for partition 1 must be specified.";
  CHECK_NE(FLAGS_part2, "") <<
      "An output filename for partition 2 must be specified.";
  CHECK_GT(FLAGS_f, 0.0) << "Fraction must be greater than 0.0.";
  CHECK_LT(FLAGS_f, 1.0) << "Fraction must be lower than 1.0.";
  PRNG.seed(FLAGS_seed);

  Dataset dataset1, dataset2;
  dataset1.load(FLAGS_input);
  Dataset::partition(&dataset1, &dataset2, FLAGS_f);
  dataset1.save(FLAGS_part1, FLAGS_ascii);
  dataset2.save(FLAGS_part2, FLAGS_ascii);
  return 0;
}
