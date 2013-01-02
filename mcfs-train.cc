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
// This program is used to train a model to predict the ratings
// of a training dataset. The user must specify the type of the model
// to train (Neighbours or PMF models are available), a training dataset,
// and optionally, the filepath where the trained model will be saved,
// a validation dataset and the hyperparameters of the model.

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <random>

#include <dataset.h>
#include <model.h>
#include <neighbours-model.h>
#include <pmf-model.h>
#include <protos/neighbours-model.pb.h>

DEFINE_string(mtype, "neighbours", "Model type");
DEFINE_string(mconf, "", "Model configuration");
DEFINE_string(mfile, "", "Path to the output model file");
DEFINE_string(train, "", "Train data partition");
DEFINE_string(valid, "", "Validation data partition");
DEFINE_uint64(seed, 0, "Pseudo-random number generator seed");

std::default_random_engine PRNG;

int main(int argc, char ** argv) {
  // Google tools initialization
  google::InitGoogleLogging(argv[0]);
  google::SetUsageMessage(
      "This program is used to train a model to predict the ratings "
      "of a training dataset.\nUsage: " + std::string(argv[0]) +
      " -mtype pmf -mconf \"learning_rate: 0.01\" -mfile output_model "
      "-train train_partition -valid validation_partition");
  google::ParseCommandLineFlags(&argc, &argv, true);
  // Check flags
  CHECK_NE(FLAGS_train, "") << "A train partition must be specified.";
  CHECK_NE(FLAGS_valid, "") << "A validation partition must be specified.";
  PRNG.seed(FLAGS_seed);
  // Create the model to train
  Model * model;
  if (FLAGS_mtype == "neighbours") {
    model = CHECK_NOTNULL(new NeighboursModel());
  } else if (FLAGS_mtype == "pmf") {
    model = CHECK_NOTNULL(new PMFModel());
  } else {
    LOG(FATAL) << "Unknown model type: \"" << FLAGS_mtype << "\"";
  }
  // Configure the hyperparameters of the model
  if (FLAGS_mconf != "") {
    CHECK(model->load_string(FLAGS_mconf));
  }
  // Train the model
  Dataset train_partition;
  CHECK(train_partition.load(FLAGS_train));
  Dataset valid_partition;
  CHECK(valid_partition.load(FLAGS_valid));
  printf("Valid RMSE: %f\n", model->train(train_partition, valid_partition));
  // Save the trained model to a file
  if (FLAGS_mfile != "") {
    CHECK(model->save(FLAGS_mfile));
  }
  delete model;
  return 0;
}
