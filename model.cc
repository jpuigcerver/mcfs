#include <model.h>

float Model::test(const Dataset& test_set) const {
  Dataset pred_ratings = test_set;
  pred_ratings.erase_scores();
  CLOCK(this->test(&pred_ratings.mutable_ratings()));
  return Dataset::rmse(test_set, pred_ratings);
}
