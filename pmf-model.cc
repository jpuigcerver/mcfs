#include <pmf-model.h>
#include <defines.h>
#include <glog/logging.h>
#include <random>
#include <cblas.h>

#include <fcntl.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

using google::protobuf::TextFormat;
using google::protobuf::io::FileInputStream;
using google::protobuf::io::FileOutputStream;

extern std::default_random_engine PRNG;

void init_array_normal(float* mat, size_t n) {
  CHECK_NOTNULL(mat);
  std::normal_distribution<float> ndist(0.0f, 1.0f);
  for (size_t i = 0; i < n; ++i) {
    mat[i] = ndist(PRNG);
  }
}

void init_array_static(float* mat, size_t n) {
  CHECK_NOTNULL(mat);
  for (size_t i = 0; i < n; ++i) {
    mat[i] = (i + 1.0f) / n;
  }
}

void print_mat(const float* m, const size_t N, const size_t M) {
  for (size_t i = 0; i < N; ++i) {
    for (size_t j = 0; j < M; ++j) {
      printf("%g ", m[i * M + j]);
    }
    printf("\n");
  }
}

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
  uint32_t* n_rated_items  = new uint32_t [N];
  memset(n_rated_items,0x00, sizeof(float) * N);
  for (const Dataset::Rating& rat: data.ratings()) {
    const uint32_t i = rat.user;
    const uint32_t j = rat.item;
    const float* Wj = W + j * D; // select row j from W
    float * Hi = H + i * D; // select row i from H
    cblas_saxpy(D, 1.0f, Wj, 1, Hi, 1);
    ++n_rated_items[i];
  }
  for (size_t i = 0; i < N; ++i) {
    if (n_rated_items[i] == 0) {
      continue;
    }
    float * Hi = H + i * D; // select row i from H
    const float a = 1.0f / n_rated_items[i];
    cblas_sscal(D, a, Hi, 1);
  }
}

// Y = X + alpha * Y
void sxpay(const size_t N, const float alpha, const float* X, float* Y) {
  for (size_t i = 0; i < N; ++i) {
    //DLOG(INFO) << "X[" << i << "] = " << X[i]
    //               << ", Y[" << i << "] = " << Y[i];
    Y[i] = X[i] + alpha * Y[i];
    //DLOG(INFO) << "X[" << i << "] - Y[" << i << "] = " << Y[i];
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

float compute_loss(const Dataset& data, const size_t C, const size_t D,
                   const size_t N, const size_t M, const float* Y,
                   const float* V, const float* W, const float lY,
                   const float lV, const float lW) {
  float loss = 0.0f;
  float* H = new float [C * D * N];
  float* Zij = new float [C];
  float* Z = new float [C * N * M];
  float* sZ = new float [C * N * M];
  float* Df = new float [C * N * M];
  float* Df2 = new float [C * N * M];
  for (size_t c = 0; c < C; ++c) {
    // Compute H matrix
    const float* Wc = W + c * D * M;
    float* Hc = H + c * D * N;
    compute_H(data, D, N, Wc, Hc);
    // Compute H' = Y + H
    const float* Yc = Y + c * D * N;
    cblas_saxpy(D * N, 1.0, Yc, 1, Hc, 1);
  }
  // Basic Loss function computation
  for (const Dataset::Rating& rat: data.ratings()) {
    const uint32_t i = rat.user;
    const uint32_t j = rat.item;
    // Compute Zij
    for (size_t c = 0; c < C; ++c) {
      const float* Hci = H + c * N * D + i * D;
      const float* Vcj = V + c * M * D + j * D;
      Zij[c] = cblas_sdot(D, Hci, 1, Vcj, 1);
      Z[c * N * M + i * M + j] = Zij[c];
    }
    // Zij = sigmoid(Zij) [Predicted rating]
    sigmoid(C, Zij);
    sZ[i * M + j] = Zij[0];
    // Zij = Rij - Zij [Prediction error]
    sxpay(C, -1.0f, rat.scores.data(), Zij);
    Df[i * M + j] = Zij[0];
    // Zij = Zij .^ 2 [Squared prediction error]
    sxpow2(C, Zij);
    Df2[i * M + j] = Zij[0];
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

void compute_loss_grad(const Dataset& data, const size_t C, const size_t D,
                       const size_t N, const size_t M, const float* Y,
                       const float* V, const float* W, const float lY,
                       const float lV, const float lW, float* dY,
                       float* dV, float* dW) {
  memset(dY, 0x00, sizeof(float) * C * D * N);
  memset(dV, 0x00, sizeof(float) * C * D * M);
  memset(dW, 0x00, sizeof(float) * C * D * M);
  float* H = new float [C * N * D];
  float* Zij = new float [C];
  float* aux = new float [C];
  float* aux2 = new float [C];
  for (size_t c = 0; c < C; ++c) {
    // Compute H matrix
    const float* Wc = W + c * D * M;
    float* Hc = H + c * D * N;
    compute_H(data, D, N, Wc, Hc);
    // Compute H' = Y + H
    const float* Yc = Y + c * D * N;
    cblas_saxpy(D * N, 1.0, Yc, 1, Hc, 1);
  }
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
  delete [] H;
}

float PMFModel::train(const Dataset& train_set) {
  const uint32_t N = train_set.users();
  const uint32_t M = train_set.items();
  const uint32_t C = train_set.criteria_size();

  Y_ = new float [D_ * N * C];
  V_ = new float [D_ * M * C];
  W_ = new float [D_ * M * C];
  //init_array_normal(Y_, D * N * C);
  //init_array_normal(V_, D * M * C);
  //init_array_normal(W_, D * M * C);
  init_array_static(Y_, D_ * N * C);
  init_array_static(V_, D_ * M * C);
  init_array_static(W_, D_ * M * C);

  data_ = train_set;
  info();

  Dataset norm_data = train_set;
  norm_data.to_normal_scale();
  LOG(INFO) << "Init. Loss = "
            << compute_loss(norm_data, C, D_, N, M,
                            Y_, V_, W_, lY_, lV_, lW_)
            << " MSRE = " << Model::test(train_set);

  float* dY = new float [D_ * N * C];
  float* dV = new float [D_ * M * C];
  float* dW = new float [D_ * M * C];
  for (uint32_t iter = 1; iter <= max_iters_; ++iter) {
    compute_loss_grad(norm_data, C, D_, N, M, Y_, V_, W_,
                      lY_, lV_, lW_, dY, dV, dW);
    cblas_saxpy(C * D_ * N, -learning_rate_, dY, 1, Y_, 1);
    cblas_saxpy(C * D_ * M, -learning_rate_, dV, 1, V_, 1);
    cblas_saxpy(C * D_ * M, -learning_rate_, dW, 1, W_, 1);
    const float loss = compute_loss(norm_data, C, D_, N, M,
                                    Y_, V_, W_, lY_, lV_, lW_);
    LOG(INFO) << "Iter. " << iter << ": Loss = " << loss
              << " MSRE = " << Model::test(train_set);
  }
  return Model::test(train_set);
}

float PMFModel::train_batch(const Dataset& batch) {
  return 0.0;
}

void PMFModel::test(std::vector<Dataset::Rating>* test_set) const {
  const uint32_t N = data_.users();
  const uint32_t M = data_.items();
  const uint32_t C = data_.criteria_size();
  float* H = new float [C * D_ * N];
  float* Zij = new float [C];
  for (size_t c = 0; c < C; ++c) {
    // Compute H matrix
    const float* Wc = W_ + c * D_ * M;
    float* Hc = H + c * D_ * N;
    compute_H(data_, D_, N, Wc, Hc);
    // Compute H' = Y + H
    const float* Yc = Y_ + c * D_ * N;
    cblas_saxpy(D_ * N, 1.0, Yc, 1, Hc, 1);
  }
  // Basic Loss function computation
  for (Dataset::Rating& rat: *test_set) {
    const uint32_t i = rat.user;
    const uint32_t j = rat.item;
    // Compute Zij
    for (size_t c = 0; c < C; ++c) {
      const float* Hci = H + c * D_ * N + i * D_;
      const float* Vcj = V_ + c * D_ * M + j * D_;
      Zij[c] = cblas_sdot(D_, Hci, 1, Vcj, 1);
    }
    // Zij = sigmoid(Zij) [Predicted rating]
    sigmoid(C, Zij);
    memcpy(rat.scores.data(), Zij, sizeof(float) * C);
  }
  delete [] H;
  delete [] Zij;
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
  if (data_.ratings_size() > 0) {
    data_.save(config->mutable_ratings());
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
  }
  config->set_factors(D_);
  // Training options
  config->set_learning_rate(learning_rate_);
  config->set_max_iters(max_iters_);
  config->set_ly(lY_);
  config->set_lv(lV_);
  config->set_lw(lW_);
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
    Y_ = CHECK_NOTNULL(new float [config.y_size()]);
    std::copy(config.y().begin(), config.y().end(), Y_);
  }
  if (config.v_size() > 0) {
    if (V_ != NULL) {
      delete [] V_;
    }
    V_ = CHECK_NOTNULL(new float [config.v_size()]);
    std::copy(config.v().begin(), config.v().end(), V_);
  }
  if (config.w_size() > 0) {
    if (W_ != NULL) {
      delete [] W_;
    }
    W_ = CHECK_NOTNULL(new float [config.w_size()]);
    std::copy(config.w().begin(), config.w().end(), W_);
  }
  D_ = config.factors();
  // Training options
  max_iters_ = config.max_iters();
  learning_rate_ = config.learning_rate();
  lY_ = config.ly();
  lV_ = config.lv();
  lW_ = config.lw();
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

void PMFModel::info(bool log) const {
  if (log) {
    LOG(INFO) << "Factors = " << D_;
    LOG(INFO) << "Max. Iters = " << max_iters_;
    LOG(INFO) << "Learning rate = " << learning_rate_;
    LOG(INFO) << "lY = " << lY_;
    LOG(INFO) << "lV = " << lV_;
    LOG(INFO) << "lW = " << lW_;
  } else {
    printf("Factors = %u\n", D_);
    printf("Max. Iters = %u\n", max_iters_);
    printf("Learning rate = %f\n", learning_rate_);
    printf("lY = %f\n", lY_);
    printf("lV = %f\n", lV_);
    printf("lW = %f\n", lW_);
  }
  data_.info(0, log);
}
