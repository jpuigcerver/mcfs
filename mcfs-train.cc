#include <gflags/gflags.h>
#include <glog/logging.h>

#include <dataset.h>
#include <neighbours-model.h>

DEFINE_string(train, "", "Train data partition");
DEFINE_string(valid, "", "Validation data partition");

int main(int argc, char ** argv) {
  // Google tools initialization
  google::InitGoogleLogging(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, true);

  CHECK_NE(FLAGS_train, "") << "A train partition must be specified.";

  Dataset train_partition;
  Dataset valid_partition;
  CHECK(train_partition.load_file(FLAGS_train.c_str()));
  if (FLAGS_valid != "") {
    CHECK(valid_partition.load_file(FLAGS_valid.c_str()));
  }

  NeighboursModel nm;
  nm.train(train_partition, valid_partition);
  printf("Test RMSE: %f\n", nm.test(valid_partition));
  return 0;
}
