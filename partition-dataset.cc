#include <gflags/gflags.h>
#include <glog/logging.h>
#include <stdio.h>
#include <dataset.h>

DEFINE_string(input, "", "Input dataset filename");
DEFINE_string(part1, "", "Partition 1 filename");
DEFINE_string(part2, "", "Partition 2 filename");
DEFINE_double(f, 0.8, "Fraction of input data used for part1 (Range: 0..1)");
DEFINE_bool(ascii, true, "Output partitions in ASCII format");

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

  Dataset dataset1, dataset2;
  dataset1.load_file(FLAGS_input.c_str());
  Dataset::partition(&dataset1, &dataset2, FLAGS_f);
  if (FLAGS_ascii) {
    dataset1.save_file(FLAGS_part1.c_str(), Dataset::ASCII);
    dataset2.save_file(FLAGS_part2.c_str(), Dataset::ASCII);
  } else {
    dataset1.save_file(FLAGS_part1.c_str(), Dataset::BINARY);
    dataset2.save_file(FLAGS_part2.c_str(), Dataset::BINARY);
  }

  return 0;
}
