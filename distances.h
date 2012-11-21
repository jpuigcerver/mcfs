#ifndef DISTANCES_H_
#define DISTANCES_H_

#include <math.h>
#include <vector>
#include <dataset.h>

float RMSE(const std::vector<Rating>& a, const std::vector<Rating>& b);
float RMSE(const Dataset& a, const Dataset& b);

class Distance {
 public:
  virtual float operator() (
      const std::vector<float>& a, const std::vector<float>& b) const;
};

class EuclideanDistance : public Distance {
 public:
  float operator() (
      const std::vector<float>& a, const std::vector<float>& b) const;
};

class ManhattanDistance : public Distance {
 public:
  float operator() (
      const std::vector<float>& a, const std::vector<float>& b) const;
};

class NormDistance : public Distance {
 public:
  NormDistance(float p = INFINITY);
  float operator() (
      const std::vector<float>& a, const std::vector<float>& b) const;
 private:
  float p;
};

#endif  // DISTANCES_H_
