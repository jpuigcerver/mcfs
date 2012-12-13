#ifndef SIMILARITIES_H_
#define SIMILARITIES_H_

#include <math.h>
#include <vector>
#include <algorithm>
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
  virtual ~Similarity() {}
  virtual float operator() (
      const std::vector<float>& a, const std::vector<float>& b,
      bool normalize = true) const {
    CHECK_EQ(a.size(), b.size());
    return 0.0f;
  }
};

class CosineSimilarity : public Similarity {
public:
  virtual ~CosineSimilarity() {}
  virtual float operator() (
      const std::vector<float>& a, const std::vector<float>& b,
      bool normalize = true) const {
    CHECK_EQ(a.size(), b.size());
    if (a.size() == 0) {
      return 0.0f;
    }
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

class CosineSqrtSimilarity : public CosineSimilarity {
public:
  virtual ~CosineSqrtSimilarity() {}
  virtual float operator() (
      const std::vector<float>& a, const std::vector<float>& b,
      bool normalize = true) const {
    const float s = CosineSimilarity::operator()(a, b, normalize);
    return sqrt(s);
  }
};

class CosinePow2Similarity : public CosineSimilarity {
public:
  virtual ~CosinePow2Similarity() {}
  virtual float operator() (
      const std::vector<float>& a, const std::vector<float>& b,
      bool normalize = true) const {
    const float s = CosineSimilarity::operator()(a, b, normalize);
    return s*s;
  }
};

class NormSimilarity : public Similarity {
private:
  float p;
public:
  NormSimilarity(float p) : p(p) {}
  virtual ~NormSimilarity() {}
  virtual float operator() (
      const std::vector<float>& a, const std::vector<float>& b,
      bool normalize = true) const {
    CHECK_EQ(a.size(), b.size());
    if (a.size() == 0) {
      return 0.0f;
    }
    const std::vector<float> * va = &a;
    const std::vector<float> * vb = &b;
    std::vector<float> a_norm;
    std::vector<float> b_norm;
    // Normalize vectors before compute the euclidean distance
    if (normalize) {
      const float la = v_norm2(a);
      const float lb = v_norm2(b);
      a_norm.assign(a.begin(), a.end());
      b_norm.assign(b.begin(), b.end());
      for (size_t i = 0; i < a.size(); ++i) {
        a_norm[i] /= la;
        b_norm[i] /= lb;
      }
      va = &a_norm;
      vb = &b_norm;
    }
    // Compute the P-norm of va - vb
    float s = 0.0f;
    for (size_t i = 0; i < va->size(); ++i) {
      s += fabs(va->at(i) - vb->at(i));
    }
    if (s > 0) {
      return 1.0f / pow(s, 1.0f / p);
    } else {
      return INFINITY;
    }
  }
};

class InfNormSimilarity : public Similarity {
public:
  virtual ~InfNormSimilarity() {}
  virtual float operator() (
      const std::vector<float>& a, const std::vector<float>& b,
      bool normalize = true) const {
    CHECK_EQ(a.size(), b.size());
    if (a.size() == 0) {
      return 0.0f;
    }
    const std::vector<float> * va = &a;
    const std::vector<float> * vb = &b;
    std::vector<float> a_norm;
    std::vector<float> b_norm;
    // Normalize vectors before compute the euclidean distance
    if (normalize) {
      const float la = v_norm2(a);
      const float lb = v_norm2(b);
      a_norm.assign(a.begin(), a.end());
      b_norm.assign(b.begin(), b.end());
      for (size_t i = 0; i < a.size(); ++i) {
        a_norm[i] /= la;
        b_norm[i] /= lb;
      }
      va = &a_norm;
      vb = &b_norm;
    }
    // Compute the infinity-norm of va - vb
    float s = -INFINITY;
    for (size_t i = 0; i < va->size(); ++i) {
      s = std::max<float>(s, fabs(va->at(i) - vb->at(i)));
    }
    if (s > 0) {
      return 1.0f / s;
    } else {
      return INFINITY;
    }
  }
};

static CosineSimilarity StaticCosineSimilarity;
static CosineSqrtSimilarity StaticCosineSqrtSimilarity;
static CosinePow2Similarity StaticCosinePow2Similarity;
static NormSimilarity StaticNormSimilarityP1(1);
static NormSimilarity StaticNormSimilarityP2(2);
static InfNormSimilarity StaticNormSimilarityPI;

#endif  // SIMILARITIES_H
