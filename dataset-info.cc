
#include <dataset.h>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <stdio.h>

#include <random>

DEFINE_string(input, "", "Input dataset filename");
DEFINE_uint64(seed, 0, "Pseudo-random number generator seed");
DEFINE_uint64(n, 0, "Print n random ratings");

std::default_random_engine PRNG;

int main(int argc, char ** argv) {
  // Google tools initialization
  google::InitGoogleLogging(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, true);
  // Check flags
  CHECK_NE(FLAGS_input, "") <<
      "An input dataset filename must be specified.";
  PRNG.seed(FLAGS_seed);

  Dataset dataset;
  dataset.load(FLAGS_input);
  dataset.info(FLAGS_n);
  return 0;
}
