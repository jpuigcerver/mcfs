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

#ifndef MODEL_H_
#define MODEL_H_

#include <dataset.h>

#include <string>
#include <vector>

class Model {
 public:
  virtual ~Model() {}

  virtual std::string info() const = 0;
  virtual bool load(const std::string& filename) = 0;
  virtual bool load_string(const std::string& str) = 0;
  virtual float test(const Dataset& test_set) const;
  virtual void test(std::vector<Dataset::Rating>* users_items) const = 0;
  virtual float train(const Dataset& train_set, const Dataset& valid_set) = 0;
  virtual bool save(const std::string& filename) const = 0;
  virtual bool save_string(std::string* str) const = 0;
};

#endif  // MODEL_H_
