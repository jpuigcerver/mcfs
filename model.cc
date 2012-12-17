#include <model.h>

float Model::test(const Dataset& test_set, bool to_orig) const {
  Dataset pred_ratings = test_set;
  pred_ratings.erase_scores();
  LOG(INFO) << "Testing... ";
  CLOCK(this->test(&pred_ratings.mutable_ratings()));
  LOG(INFO) << "Test DONE";
  if (to_orig) {
    pred_ratings.to_original_scale();
  }
  return Dataset::rmse(test_set, pred_ratings);
}
