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
// This program is used to test a trained model using a test dataset.
// The user must specify the type of the model to use (Neighbours or
// PMF models are available), a test dataset and the configuration file
// of the model.

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <random>

#include <dataset.h>
#include <model.h>
#include <neighbours-model.h>
#include <pmf-model.h>
#include <protos/neighbours-model.pb.h>

DEFINE_string(mtype, "neighbours", "Model type");
DEFINE_string(mfile, "", "Model configuration file");
DEFINE_string(test, "", "Train data partition");
DEFINE_uint64(seed, 0, "Pseudo-random number generator seed");

std::default_random_engine PRNG;

int main(int argc, char ** argv) {
  // Google tools initialization
  google::InitGoogleLogging(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, true);
  // Check flags
  CHECK_NE(FLAGS_mtype, "") << "A model type must be specified.";
  CHECK_NE(FLAGS_mfile, "") << "A model configuration file must be specified.";
  CHECK_NE(FLAGS_test, "") << "A train partition must be specified.";
  PRNG.seed(FLAGS_seed);
  // Create the model to use
  Model * model;
  if (FLAGS_mtype == "neighbours") {
    model = CHECK_NOTNULL(new NeighboursModel());
  } else if (FLAGS_mtype == "pmf") {
    model = CHECK_NOTNULL(new PMFModel());
  } else {
    LOG(FATAL) << "Unknown model type: \"" << FLAGS_mtype << "\"";
  }
  CHECK(model->load(FLAGS_mfile));
  model->info(true);
  // Test the model
  Dataset test_partition;
  CHECK(test_partition.load(FLAGS_test));
  printf("Test RMSE: %f\n", model->test(test_partition));
  delete model;
  return 0;
}
