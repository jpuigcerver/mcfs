#include "distances.h"

#include <glog/logging.h>
#include <math.h>

float RMSE(const std::vector<Rating>& a, const std::vector<Rating>& b) {
  CHECK_EQ(a.size(), b.size());
  float s = 0.0f;
  for (auto x = a.begin(), y = b.begin(); x != a.end(); ++x, ++y) {
    CHECK_EQ(x->c_rating.size(), y->c_rating.size());
    for (uint64_t c = 0; c < x->c_rating.size(); ++c) {
      float d = (x->c_rating[c] - y->c_rating[c]);
      s += d * d;
    }
  }
  return sqrtf(s / a.size());
}

float RMSE(const Dataset& a, const Dataset& b) {
  return RMSE(a.ratings, b.ratings);
}

float Distance::operator() (
    const std::vector<float>& a, const std::vector<float>& b) const {
  return 0.0f;
}

float EuclideanDistance::operator() (
    const std::vector<float>& a, const std::vector<float>& b) const {
  CHECK_EQ(a.size(), b.size());
  float s = 0.0f;
  for (auto xa = a.begin(), xb = b.begin(); xa != a.end(); ++xa, ++xb) {
    const float d = xa - xb;
    s += d*d;
  }
  return sqrtf(s);
}

float ManhattanDistance::operator() (
    const std::vector<float>& a, const std::vector<float>& b) const {
  CHECK_EQ(a.size(), b.size());
  float s = 0.0f;
  for (auto xa = a.begin(), xb = b.begin(); xa != a.end(); ++xa, ++xb) {
    s += fabs(xa - xb);
  }
  return s;
}

NormDistance::NormDistance(float p) : p(p) { }

float NormDistance::operator() (
    const std::vector<float>& a, const std::vector<float>& b) const {
  CHECK_EQ(a.size(), b.size());
  if (p != INFINITY) {
    float s = 0.0f;
    for (auto xa = a.begin(), xb = b.begin(); xa != a.end(); ++xa, ++xb) {
      s += powf(fabs(xa - xb), p);
    }
    return powf(s, 1.0f / p);
  } else {
    float m = 0.0f;
    for (auto xa = a.begin(), xb = b.begin(); xa != a.end(); ++xa, ++xb) {
      const float d = fabs(xa - xb);
      if ( d < m ) m = d;
    }
    return m;
  }
}
