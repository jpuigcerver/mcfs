#include <gflags/gflags.h>
#include <glog/logging.h>
#include <stdio.h>
#include <dataset.h>

DEFINE_string(input, "", "Input dataset filename");
DEFINE_string(part1, "", "Partition 1 filename");
DEFINE_string(part2, "", "Partition 2 filename");
DEFINE_float(f, 0.8, "Fraction of input data used for part1 (Range: 0..1)");

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
  CHECK_GT(FLAGS_ratio, 0.0) << "Fraction must be greater than 0.0.";
  CHECK_LT(FLAGS_ratio, 1.0) << "Fraction must be lower than 1.0.";

  Dataset dataset;

  return 0;
}
