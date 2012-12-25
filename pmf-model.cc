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

#include <pmf-model.h>

#include <cblas.h>
#include <defines.h>
#include <fcntl.h>
#include <glog/logging.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>

#include <algorithm>
#include <random>

using google::protobuf::TextFormat;
using google::protobuf::io::FileInputStream;
using google::protobuf::io::FileOutputStream;

extern std::default_random_engine PRNG;

void init_array_normal(float* mat, size_t n) {
  CHECK_NOTNULL(mat);
  DLOG(INFO) << "Matrix initialized using normal distribution.";
  std::normal_distribution<float> ndist(0.0f, 1.0f);
  for (size_t i = 0; i < n; ++i) {
    mat[i] = ndist(PRNG);
  }
}

void init_array_uniform(float* mat, size_t n) {
  CHECK_NOTNULL(mat);
  DLOG(INFO) << "Matrix initialized using uniform distribution.";
  std::uniform_real_distribution<float> udist(0.0f, 1.0f);
  for (size_t i = 0; i < n; ++i) {
    mat[i] = udist(PRNG);
  }
}

void init_array_static(float* mat, size_t n) {
  CHECK_NOTNULL(mat);
  DLOG(INFO) << "Matrix initialized using static distribution.";
  for (size_t i = 0; i < n; ++i) {
    mat[i] = (i + 1.0f) / n;
  }
}

#ifndef NDEBUG
void print_mat(const std::string& name, const float* m,
               const size_t N, const size_t M, const size_t L) {
  std::string msg;
  if (name != "") {
    msg = name;
  }
  msg += '\n';
  for (size_t i = 0; i < N; ++i) {
    for (size_t j = 0; j < M; ++j) {
      for (size_t k = 0; k < L; ++k) {
        char strelem[20];
        snprintf(strelem, sizeof(strelem), "%e ", m[i * M * L + j * L + k]);
        msg += strelem;
      }
      msg += '\n';
    }
    msg += '\n';
  }
  while (msg.length() > 0 && msg.back() == '\n') {
    msg.erase(msg.length()-1);
  }
  DLOG(INFO) << msg;
}
#endif

void sigmoid(const size_t N, float* x) {
  for (size_t i = 0; i < N; ++i) {
    x[i] = 1.0f / (1.0f + exp(-x[i]));
  }
}

// size(W) = M x D
// size(H) = M x D
void compute_H(const Dataset& data, const size_t D, const size_t N,
               const float* W, float* H) {
  memset(H, 0x00, sizeof(float) * D * N);
  for (const Dataset::Rating& rat: data.ratings()) {
    const uint32_t i = rat.user;
    const uint32_t j = rat.item;
    const float* Wj = W + j * D;  // select row j from W
    float * Hi = H + i * D;  // select row i from H
    cblas_saxpy(D, 1.0f, Wj, 1, Hi, 1);
  }
  for (size_t i = 0; i < N; ++i) {
    if (data.ratings_by_user(i).size() == 0) {
      continue;
    }
    float * Hi = H + i * D;  // select row i from H
    const float a = 1.0f / data.ratings_by_user(i).size();
    cblas_sscal(D, a, Hi, 1);
  }
#ifndef NDEBUG
  print_mat("H", H, 1, N, D);
#endif
}

void compute_HY(const Dataset& data, const size_t C, const size_t N,
                const size_t M, const size_t D, const float* Y,
                const float* W, float* H) {
  for (size_t c = 0; c < C; ++c) {
    // Compute H matrix
    const float* Wc = W + c * D * M;
    float* Hc = H + c * D * N;
    compute_H(data, D, N, Wc, Hc);
    // Compute H' = Y + H
    const float* Yc = Y + c * D * N;
    cblas_saxpy(D * N, 1.0, Yc, 1, Hc, 1);
  }
#ifndef NDEBUG
  print_mat("H' = Y + H", H, C, N, D);
#endif
}

// Y = X + alpha * Y
void sxpay(const size_t N, const float alpha, const float* X, float* Y) {
  for (size_t i = 0; i < N; ++i) {
    Y[i] = X[i] + alpha * Y[i];
  }
}

// X = X .^ 2
void sxpow2(const size_t N, float* X) {
  for (size_t i = 0; i < N; ++i) {
    X[i] *= X[i];
  }
}

// X = sum(X .^ 2)
float snrmfp2(const size_t N, const float* X) {
  float s = 0.0;
  for (size_t i = 0; i < N; ++i) {
    s += X[i] * X[i];
  }
  return s;
}

// X = X*alpha + k
void saxpk(size_t N, const float alpha, float* x, const float k) {
  for (size_t i = 0; i < N; ++i) {
    x[i] = x[i] * alpha + k;
  }
}

// Y = X .* Y
void sxdy(size_t N, const float* x, float* y) {
  for (size_t i = 0; i < N; ++i) {
    y[i] = x[i] * y[i];
  }
}

// s = sum(X(:))
float ssum(size_t N, const float* x) {
  float s = 0.0f;
  for (size_t i = 0; i < N; ++i) {
    s += x[i];
  }
  return s;
}

// s = mult(X(:))
float smul(size_t N, const float* x) {
  float s = 1.0f;
  for (size_t i = 0; i < N; ++i) {
    s *= x[i];
  }
  return s;
}

float compute_loss(const Dataset& data, const size_t C, const size_t N,
                   const size_t M, const size_t D, const float* Y,
                   const float* V, const float* W, const float* H,
                   const float lY, const float lV, const float lW) {
  float loss = 0.0f;
  float* Zij = new float[C];
  // Basic Loss function computation
  for (const Dataset::Rating& rat: data.ratings()) {
    const uint32_t i = rat.user;
    const uint32_t j = rat.item;
    // Compute Zij
    for (size_t c = 0; c < C; ++c) {
      const float* Hci = H + c * N * D + i * D;
      const float* Vcj = V + c * M * D + j * D;
      Zij[c] = cblas_sdot(D, Hci, 1, Vcj, 1);
    }
    // Zij = sigmoid(Zij) [Predicted rating]
    sigmoid(C, Zij);
    // Zij = Rij - Zij [Prediction error]
    sxpay(C, -1.0f, rat.scores.data(), Zij);
    // Zij = Zij .^ 2 [Squared prediction error]
    sxpow2(C, Zij);
    loss += cblas_sasum(C, Zij, 1);
  }
  loss /= 2.0;
  // Regularization Y
  loss += (lY / 2.0) * snrmfp2(C * D * N, Y);
  // Regularization V
  loss += (lV / 2.0) * snrmfp2(C * D * M, V);
  // Regularization W
  loss += (lW / 2.0) * snrmfp2(C * D * M, W);
  return loss;
}

void compute_loss_grad(const Dataset& data, const size_t C, const size_t N,
                       const size_t M, const size_t D, const float* Y,
                       const float* V, const float* W, const float* H,
                       const float lY, const float lV, const float lW,
                       float* dY, float* dV, float* dW) {
  memset(dY, 0x00, sizeof(float) * C * D * N);
  memset(dV, 0x00, sizeof(float) * C * D * M);
  memset(dW, 0x00, sizeof(float) * C * D * M);
  float* Zij = new float[C];
  float* aux = new float[C];
  float* aux2 = new float[C];
  for (const Dataset::Rating& rat: data.ratings()) {
    const uint32_t i = rat.user;
    const uint32_t j = rat.item;
    // Compute Zij
    for (size_t c = 0; c < C; ++c) {
      const float* Hci = H + c * D * N + i * D;
      const float* Vcj = V + c * D * M + j * D;
      Zij[c] = cblas_sdot(D, Hci, 1, Vcj, 1);
    }
    // Zij = g(Zij) [Predicted rating]
    sigmoid(C, Zij);
    memcpy(aux, Zij, sizeof(float) * C);
    memcpy(aux2, Zij, sizeof(float) * C);
    // aux = -1 * (Rij - g(Zij)) = (g(Zij) - Rij) [- Prediction error]
    cblas_saxpy(C, -1.0f, rat.scores.data(), 1, aux, 1);
    // aux2 = 1 - g(Zij)
    saxpk(C, -1.0f, aux2, 1.0f);
    // aux2 = g(Zij) .* (1 - g(Zij))
    sxdy(C, Zij, aux2);
    // aux = g(Zij) .* (1 - g(Zij)) .* (g(Zij) - Rij)
    sxdy(C, aux2, aux);
    const std::vector<Dataset::Rating*>& user_ratings =
        data.ratings_by_user(i);
    for (size_t c = 0; c < C; ++c) {
      for (size_t d = 0; d < D; ++d) {
        // dY(c,d,i) += aux[c] * V(c,d,j)
        const float Vcdj = V[c * D * M + j * D + d];
        dY[c * D * N + i * D + d] += aux[c] * Vcdj;
        // dV(c,d,j) += aux[c] * H'(c,d,i)
        const float Hcdi = H[c * D * N + i * D + d];
        dV[c * D * N + j * D + d] += aux[c] * Hcdi;
        // dW(c,d,j) += aux[c] * Vcdj / ratings_user[i]
        if (user_ratings.size() == 0) {
          continue;
        }
        for (const Dataset::Rating* rat: user_ratings) {
          uint32_t l = rat->item;
          dW[c * D * N + l * D + d] +=
              aux[c] * Vcdj / user_ratings.size();
        }
      }
    }
  }
  // Regularization dY
  for (size_t c = 0; c < C; ++c) {
    for (size_t i = 0; i < N; ++i) {
      for (size_t d = 0; d < D; ++d) {
        dY[c * D * N + i * D + d] += lY * Y[c * D * N + i * D + d];
      }
    }
  }
  // Regularization dV, dW
  for (size_t c = 0; c < C; ++c) {
    for (size_t j = 0; j < M; ++j) {
      for (size_t d = 0; d < D; ++d) {
        dV[c * D * M + j * D + d] += lV * V[c * D * M + j * D + d];
        dW[c * D * M + j * D + d] += lW * W[c * D * M + j * D + d];
      }
    }
  }
  delete [] aux;
  delete [] Zij;
}

float PMFModel::train(const Dataset& train_set, const Dataset& valid_set) {
  const uint32_t N = train_set.users();
  const uint32_t M = train_set.items();
  const uint32_t C = train_set.criteria_size();
  // Initialize the parameters if it's needed
  void (*matrix_init[])(float*, size_t) =
      { &init_array_static, &init_array_normal, &init_array_uniform };
  if (Y_ == NULL) {
    DLOG(INFO) << "Matrix Y created.";
    Y_ = new float[D_ * N * C];
    matrix_init[matrix_init_id_](Y_, D_ * N * C);
  }
#ifndef NDEBUG
  print_mat("Y", Y_, C, N, D_);
#endif
  if (V_ == NULL) {
    DLOG(INFO) << "Matrix V created.";
    V_ = new float[D_ * M * C];
    matrix_init[matrix_init_id_](V_, D_ * M * C);
  }
#ifndef NDEBUG
  print_mat("V", V_, C, M, D_);
#endif
  if (W_ == NULL) {
    DLOG(INFO) << "Matrix W created.";
    W_ = new float[D_ * M * C];
    matrix_init[matrix_init_id_](W_, D_ * M * C);
  }
#ifndef NDEBUG
  print_mat("W", W_, C, M, D_);
#endif
  // Copy the training data
  data_ = train_set;
  // Show Model info
  LOG(INFO) << "Model config:\n" << info();
  // Get a copy of the data normalized to [0..1]
  Dataset norm_data = train_set;
  norm_data.to_normal_scale();
  norm_data.shuffle();
  // Initial guess
  if (HY_ == NULL) {
    DLOG(INFO) << "Matrix HY created.";
    HY_ = new float[D_ * N * C];
    compute_HY(data_, C, N, M, D_, Y_, W_, HY_);
  }
  float last_loss = compute_loss(
      norm_data, C, N, M, D_, Y_, V_, W_, HY_, lY_, lV_, lW_);
  float last_t_rmse = test(train_set);
  float last_v_rmse = test(valid_set);
  LOG(INFO) << "Init: Loss = " << last_loss << ", Train RMSE = " << last_t_rmse
            << ", Valid RMSE = " << last_v_rmse;
  // Prepare matrices that will store the gradients
  float* dY = new float[D_ * N * C];
  float* dV = new float[D_ * M * C];
  float* dW = new float[D_ * M * C];
  // Previous gradients, useful for momentum
  float* dYp = new float[D_ * N * C];
  float* dVp = new float[D_ * M * C];
  float* dWp = new float[D_ * M * C];
  memset(dYp, 0x00, sizeof(float) * D_ * N * C);
  memset(dVp, 0x00, sizeof(float) * D_ * M * C);
  memset(dWp, 0x00, sizeof(float) * D_ * M * C);
  // Prepare uniform distribution for minibatches
  std::uniform_int_distribution<uint32_t> udist(
      0, batch_size_ < norm_data.ratings_size() ?
      norm_data.ratings_size() - batch_size_ :
      norm_data.ratings_size());
  // Training performing SGD
  for (uint32_t iter = 1; iter <= max_iters_; ++iter) {
    // Prepare minibatch
    Dataset mini_batch;
    norm_data.copy(&mini_batch, udist(PRNG), batch_size_);
    // Compute loss gradient for the minibatch
    compute_loss_grad(mini_batch, C, N, M, D_, Y_, V_, W_, HY_,
                      lY_, lV_, lW_, dY, dV, dW);
    // g' = - g' * momentum + g
    sxpay(C * N * D_, -momentum_, dY, dYp);
    sxpay(C * M * D_, -momentum_, dV, dVp);
    sxpay(C * M * D_, -momentum_, dW, dWp);
    // W = W - g' * lr
    cblas_saxpy(C * D_ * N, -learning_rate_, dYp, 1, Y_, 1);
    cblas_saxpy(C * D_ * M, -learning_rate_, dVp, 1, V_, 1);
    cblas_saxpy(C * D_ * M, -learning_rate_, dWp, 1, W_, 1);
    // Compute new H' = H + Y
    compute_HY(data_, C, N, M, D_, Y_, W_, HY_);
    // Compute loss function in the whole train set
    last_loss = compute_loss(
        norm_data, C, N, M, D_, Y_, V_, W_, HY_, lY_, lV_, lW_);
    // Test the model in the whole train & valid set
    last_t_rmse = test(train_set);
    last_v_rmse = test(valid_set);
    LOG(INFO) << "Iter = " << iter << " Loss = " << last_loss
              << " Train RMSE = " << last_t_rmse
              << " Valid RMSE = " << last_v_rmse;
  }
  delete [] dY;
  delete [] dV;
  delete [] dW;
  delete [] dYp;
  delete [] dVp;
  delete [] dWp;
  return last_v_rmse;
}

using mcfs::protos::PMFModelConfig_MatrixInit_STATIC;
PMFModel::PMFModel()
    : D_(10), max_iters_(100), learning_rate_(0.1f),
    momentum_(0.0f), matrix_init_id_(PMFModelConfig_MatrixInit_STATIC),
    lY_(0.0f), lV_(0.0f), lW_(0.0f),
    Y_(NULL), V_(NULL), W_(NULL), HY_(NULL) {
}

PMFModel::~PMFModel() {
  clear();
}

void PMFModel::clear() {
  data_.clear();
  D_ = 10;
  max_iters_ = 100;
  learning_rate_ = 0.1f;
  lY_ = 0.0f;
  lV_ = 0.0f;
  lW_ = 0.0f;
  if (Y_ != NULL) {
    delete [] Y_;
    Y_ = NULL;
  }
  if (V_ != NULL) {
    delete [] V_;
    V_ = NULL;
  }
  if (W_ != NULL) {
    delete [] W_;
    W_ = NULL;
  }
  if (HY_ != NULL) {
    delete [] HY_;
    HY_ = NULL;
  }
}

void PMFModel::test(std::vector<Dataset::Rating>* test_set) const {
  const uint32_t N = data_.users();
  const uint32_t M = data_.items();
  const uint32_t C = data_.criteria_size();
  float* Zij = new float[C];
  // Basic Loss function computation
  for (auto it = test_set->begin(); it != test_set->end(); ++it) {
    Dataset::Rating& rat = *it;
    const uint32_t i = rat.user;
    const uint32_t j = rat.item;
    // Compute Zij
    for (size_t c = 0; c < C; ++c) {
      const float* Hci = HY_ + c * N * D_ + i * D_;
      const float* Vcj = V_ + c * M * D_ + j * D_;
      Zij[c] = cblas_sdot(D_, Hci, 1, Vcj, 1);
    }
    // Zij = sigmoid(Zij) [Predicted rating]
    sigmoid(C, Zij);
    memcpy(rat.scores.data(), Zij, sizeof(float) * C);
  }
  delete [] Zij;
}

float PMFModel::test(const Dataset& test_set) const {
  Dataset pred_ratings = test_set;
  pred_ratings.erase_scores();
  CLOCK(this->test(&pred_ratings.mutable_ratings()));
  pred_ratings.to_original_scale();
  return Dataset::rmse(test_set, pred_ratings);
}

bool PMFModel::save(const std::string& filename) const {
  PMFModelConfig config;
  if (!save(&config)) {
    return false;
  }
  // Write protocol buffer
  int fd = open(filename.c_str(), O_CREAT | O_WRONLY | O_TRUNC,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (fd < 0) {
    LOG(ERROR) << "PMFModel \"" << filename << "\": Failed to open. Error: "
               << strerror(errno);
    return false;
  }
  FileOutputStream fs(fd);
  if (!config.SerializeToFileDescriptor(fd)) {
    LOG(ERROR) << "PMFModel \"" << filename << "\": Failed to write.";
    return false;
  }
  close(fd);
  return true;
}

bool PMFModel::load(const std::string& filename) {
  PMFModelConfig config;
  int fd = open(filename.c_str(), O_RDONLY);
  if (fd < 0) {
    LOG(ERROR) << "PMFModel \"" << filename << "\": Failed to open. Error: "
               << strerror(errno);
    return false;
  }
  FileInputStream fs(fd);
  if (!config.ParseFromFileDescriptor(fd) &&
      !TextFormat::Parse(&fs, &config)) {
    LOG(ERROR) << "PMFModel \"" << filename << "\": Failed to parse.";
    return false;
  }
  close(fd);
  return load(config);
}

bool PMFModel::save(PMFModelConfig* config) const {
  // If there is training data available...
  if (data_.ratings_size() > 0) {
    // Save training data
    data_.save(config->mutable_ratings());
    // Save trained parameters
    if (Y_ != NULL) {
      for (size_t c = 0, k = 0; c < data_.criteria_size(); ++c) {
        for (size_t i = 0; i < data_.users(); ++i) {
          for (size_t d = 0; d < D_; ++d, ++k) {
            config->add_y(Y_[k]);
          }
        }
      }
    }
    if (V_ != NULL) {
      for (size_t c = 0, k = 0; c < data_.criteria_size(); ++c) {
        for (size_t j = 0; j < data_.items(); ++j) {
          for (size_t d = 0; d < D_; ++d, ++k) {
            config->add_v(V_[k]);
          }
        }
      }
    }
    if (W_ != NULL) {
      for (size_t c = 0, k = 0; c < data_.criteria_size(); ++c) {
        for (size_t j = 0; j < data_.items(); ++j) {
          for (size_t d = 0; d < D_; ++d, ++k) {
            config->add_w(W_[k]);
          }
        }
      }
    }
    if (HY_ != NULL) {
      for (size_t c = 0, k = 0; c < data_.criteria_size(); ++c) {
        for (size_t i = 0; i < data_.users(); ++i) {
          for (size_t d = 0; d < D_; ++d, ++k) {
            config->add_hy(HY_[k]);
          }
        }
      }
    }
  }
  config->set_factors(D_);
  // Training options
  config->set_learning_rate(learning_rate_);
  config->set_max_iters(max_iters_);
  config->set_batch_size(batch_size_);
  config->set_ly(lY_);
  config->set_lv(lV_);
  config->set_lw(lW_);
  config->set_matrix_init(matrix_init_id_);
  config->set_momentum(momentum_);
  return true;
}

bool PMFModel::load(const PMFModelConfig& config) {
  if (!data_.load(config.ratings())) {
    return false;
  }
  if (config.y_size() > 0) {
    if (Y_ != NULL) {
      delete [] Y_;
    }
    Y_ = CHECK_NOTNULL(new float[config.y_size()]);
    std::copy(config.y().begin(), config.y().end(), Y_);
  }
  if (config.v_size() > 0) {
    if (V_ != NULL) {
      delete [] V_;
    }
    V_ = CHECK_NOTNULL(new float[config.v_size()]);
    std::copy(config.v().begin(), config.v().end(), V_);
  }
  if (config.w_size() > 0) {
    if (W_ != NULL) {
      delete [] W_;
    }
    W_ = CHECK_NOTNULL(new float[config.w_size()]);
    std::copy(config.w().begin(), config.w().end(), W_);
  }
  if (config.hy_size() > 0) {
    if (HY_ != NULL) {
      delete [] HY_;
    }
    HY_ = CHECK_NOTNULL(new float[config.hy_size()]);
    std::copy(config.hy().begin(), config.hy().end(), HY_);
  }
  D_ = config.factors();
  // Training options
  max_iters_ = config.max_iters();
  learning_rate_ = config.learning_rate();
  batch_size_ = config.batch_size();
  lY_ = config.ly();
  lV_ = config.lv();
  lW_ = config.lw();
  matrix_init_id_ = config.matrix_init();
  momentum_ = config.momentum();
  return true;
}

bool PMFModel::load_string(const std::string& str) {
  PMFModelConfig config;
  if (!TextFormat::ParseFromString(str, &config)) {
    LOG(ERROR) << "PMFModel: Failed to parse.";
    return false;
  }
  return load(config);
}

bool PMFModel::save_string(std::string* str) const {
  CHECK_NOTNULL(str);
  PMFModelConfig config;
  save(&config);
  return TextFormat::PrintToString(config, str);
}

std::string PMFModel::info() const {
  char buff[50];
  std::string msg;
  snprintf(buff, sizeof(buff), "Factors = %u\n", D_);
  msg += buff;
  snprintf(buff, sizeof(buff), "Max. Iters = %u\n", max_iters_);
  msg += buff;
  snprintf(buff, sizeof(buff), "Batch size = %u\n", batch_size_);
  msg += buff;
  snprintf(buff, sizeof(buff), "Learning rate = %f\n", learning_rate_);
  msg += buff;
  snprintf(buff, sizeof(buff), "Momentum = %f\n", momentum_);
  msg += buff;
  snprintf(buff, sizeof(buff), "lY = %f\n", lY_);
  msg += buff;
  snprintf(buff, sizeof(buff), "lV = %f\n", lV_);
  msg += buff;
  snprintf(buff, sizeof(buff), "lW = %f\n", lW_);
  msg += data_.info(0);
  return msg;
}
