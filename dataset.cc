#include <dataset.h>

#include <defines.h>
#include <glog/logging.h>
#include <stdio.h>
#include <string>
#include <algorithm>
#include <arpa/inet.h>

// Parse a header line to extract the number of criteria or the
// data format.
// Returns:
//   0: Parsing error
//   1: Criteria line
//   2: Format line
int parse_header(const char * buff, unsigned int buff_size,
                 unsigned int* criteria, std::string* format) {
  unsigned int i = 0, j = 0;
  if (buff_size == 0 || buff[0] != '#') {
    return 0;
  }
  for (i = 1; i < buff_size && isspace(buff[i]); ++i);
  if (i == buff_size) {
    return 0;
  }
  if (buff[i] >= '0' && buff[i] <= '9') {
    *criteria = strtoul(buff+i, NULL, 10);
    return 1;
  } else {
    format->clear();
    for (j = i; j < buff_size && !isspace(buff[j]); ++j) {
        format->append(1, buff[j]);
    }
    return 2;
  }
}

bool Dataset::save_file(const char* filename, ds_file_format format) const {
  FILE * fp = fopen(filename, "w");
  if (fp == NULL) {
    LOG(ERROR) << "Dataset \"" << filename << "\": Failed to open.";
    return false;
  }
  DLOG(INFO) << "Dataset \"" << filename << "\": Opened correctly.";
  switch (format) {
    case ASCII:
      fprintf(fp, "# ASCII\n# %lu\n", criteria_size);
      for (const Rating& rating : ratings) {
        fprintf(fp, "%u %u", rating.user, rating.item);
        for (float criterion_rating : rating.c_rating) {
          fprintf(fp, " %f", criterion_rating);
        }
        fprintf(fp, "\n");
      }
      break;
    case BINARY:
      fprintf(fp, "# BINARY\n# %lu\n", criteria_size);
      for (const Rating& rating : ratings) {
        uint32_t user_le = htonl(rating.user);
        uint32_t item_le = htonl(rating.item);
        fwrite(&user_le, 4, 1, fp);
        fwrite(&item_le, 4, 1, fp);
        for (float criterion_rating : rating.c_rating) {
          fwrite(&criterion_rating, 4, 1, fp);
        }
      }
      break;
  }
  fclose(fp);
  return true;
}

std::string get_line_profile(size_t criteria) {
  std::string profile = "%lu %lu";
  for(size_t c = 0; c < criteria; ++c) {
    profile += " %f";
  }
  return profile + "\n";
}

bool Dataset::load_file(const char* filename) {
  // Open file
  FILE * fp = fopen(filename, "r");
  if (fp == NULL) {
    LOG(ERROR) << "Dataset \"" << filename << "\": Failed to open.";
    return false;
  }
  DLOG(INFO) << "Dataset \"" << filename << "\": Opened correctly.";
  char buffer[FILE_BUFF_SIZE] = {'\0'};
  unsigned int criteria = 0;
  std::string sformat;
  ds_file_format format = BINARY;
  for (int header_lines = 0; header_lines < 2; ++header_lines) {
    if (fgets(buffer, FILE_BUFF_SIZE, fp) == NULL) {
      LOG(ERROR) << "Dataset \"" << filename << "\": Failed to read.";
      fclose(fp);
      return false;
    }
    int section = parse_header(buffer, FILE_BUFF_SIZE, &criteria, &sformat);
    if (section == 1) {
      // Check criteria
      DLOG(INFO) << "Dataset \"" << filename << "\": Criteria = " << criteria;
      if (criteria < 1) {
        LOG(ERROR) << "Dataset \"" << filename << "\": Bad number of criteria.";
        fclose(fp);
        return false;
      }
    } else if (section == 2) {
      DLOG(INFO) << "Dataset \"" << filename << "\": Format = " << sformat;
      if ("ASCII" == sformat) {
        format = ASCII;
      } else if ("BINARY" == sformat) {
        format = BINARY;
      } else {
        LOG(ERROR) << "Dataset \"" << filename << "\": Bad format \""
                   << sformat << "\"";
        fclose(fp);
        return false;
      }
    } else {
      LOG(ERROR) << "Dataset \"" << filename << "\": Bad header \""
                 << buffer << "\"";
      fclose(fp);
      return false;
    }
  }

  Rating rating;
  rating.c_rating.resize(criteria);
  ratings.clear();
  criteria_size = criteria;
  if (format == ASCII) {
    do {
      // Read line
      fgets(buffer, FILE_BUFF_SIZE, fp);
      // Check for errors or EOF
      if (ferror(fp)) {
        LOG(ERROR) << "Dataset \"" << filename << "\": Failed to read.";
        fclose(fp);
        return false;
      } else if (feof(fp)) { break; }
      // Parse line
      char * end_user = NULL;
      char * end_item = NULL;
      rating.user = strtoul(buffer, &end_user, 10);
      rating.item = strtoul(end_user, &end_item, 10);
      if (end_user == buffer || end_item == end_user) {
        LOG(ERROR) << "Dataset \"" << filename
                   << "\": Bad data.";
        fclose(fp);
        return false;
      }
      char * end_p_rating = end_item;
      char * end_rating = NULL;
      for(uint64_t r = 0; r < criteria; ++r, end_p_rating = end_rating) {
        rating.c_rating[r] = strtof(end_p_rating, &end_rating);
        if (end_p_rating == end_rating) {
          LOG(ERROR) << "Dataset \"" << filename
                     << "\": Bad data.";
          fclose(fp);
          return false;
        }
      }
      ratings.push_back(rating);
    } while(!feof(fp));
  } else {
    while (!feof(fp)) {
      uint32_t user_le = 0, item_le = 0;
      uint32_t read_items = fread(&user_le, 4, 1, fp) +
          fread(&item_le, 4, 1, fp);
      if (read_items == 0) { break; }
      else if (read_items != 2) {
        LOG(ERROR) << "Dataset \"" << filename
                   << "\": Bad data. Read ratings = " << ratings.size();
        fclose(fp);
        return false;
      }
      rating.user = ntohl(user_le);
      rating.item = ntohl(item_le);
      for (uint64_t r = 0; r < criteria; ++r) {
        if (fread(&rating.c_rating[r], 4, 1, fp) != 1) {
          LOG(ERROR) << "Dataset \"" << filename
                     << "\": Bad data. Read ratings = " << ratings.size();
          fclose(fp);
          return false;
        }
      }
      ratings.push_back(rating);
    }
  }
  // All data was read nicely.
  fclose(fp);
  return true;
}

void Dataset::print() const {
  printf("# ASCII\n# %lu\n", criteria_size);
  for(Rating r : ratings) {
    printf("%u %u", r.user, r.item);
    for(float f : r.c_rating) {
      printf(" %f", f);
    }
    printf("\n");
  }
}

// This function moves each element from one dataset to an other with
// probability 1-f. This is useful to create a test set from the original
// training data, for instance.
// TODO(jpuigcerver) Pass some seed to control the generation of random numbers.
void Dataset::partition(Dataset * original, Dataset * partition, float f) {
  random_shuffle(original->ratings.begin(), original->ratings.end());
  partition->ratings.clear();
  partition->criteria_size = original->criteria_size;
  uint64_t start_pos = static_cast<uint64_t>(f * original->ratings.size());
  for (uint64_t i = start_pos; i < original->ratings.size(); ++i) {
    partition->ratings.push_back(original->ratings[i]);
  }
  original->ratings.resize(start_pos);
  if (original->ratings.size() == 0 || partition->ratings.size() == 0) {
    LOG(WARNING) << "Some partition is empty.";
  }
}

Rating::Rating() : user(0), item(0) { }

Rating::Rating(uint32_t user, uint32_t item, uint64_t criteria_size) :
    user(user), item(item), c_rating(criteria_size, 0.0f) { }

float RMSE(const std::vector<Rating>& a, const std::vector<Rating>& b) {
  CHECK_EQ(a.size(), b.size());
  float s = 0.0f;
  for (auto x = a.begin(), y = b.begin(); x != a.end(); ++x, ++y) {
    CHECK_EQ(x->c_rating.size(), y->c_rating.size());
    for (uint64_t c = 0; c < x->c_rating.size(); ++c) {
      DLOG(INFO) << "x(" << c <<") = " << x->c_rating[c]
                 << ", y(" << c <<") = " << y->c_rating[c];
      float d = (x->c_rating[c] - y->c_rating[c]);
      s += d * d;
    }
  }
  return sqrtf(s / a.size());
}

float RMSE(const Dataset& a, const Dataset& b) {
  return RMSE(a.ratings, b.ratings);
}
