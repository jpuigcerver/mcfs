#ifndef SIMILARITIES_H_
#define SIMILARITIES_H_

#include <math.h>
#include <vector>
#include <map>
#include <stddef.h>
#include <glog/logging.h>

template<typename T>
float v_norm2(const std::vector<T>& v) {
  float s = 0.0f;
  for (const auto& x : v) {
    s += x * x;
  }
  return sqrt(s);
}

class Similarity {
 public:
  virtual float operator() (
      const std::vector<float>& a, const std::vector<float>& b,
      bool normalize = true) const {
    CHECK_EQ(a.size(), b.size());
    return 0.0f;
  }
};

class CosineSimilarity : public Similarity {
public:
  virtual float operator() (
      const std::vector<float>& a, const std::vector<float>& b,
      bool normalize = true) const {
    CHECK_EQ(a.size(), b.size());
    float s = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
      s += a[i]*b[i];
    }
    if (normalize) {
      return s / (v_norm2<float>(a) * v_norm2<float>(b));
    } else {
      return s;
    }
  }
};

#endif
