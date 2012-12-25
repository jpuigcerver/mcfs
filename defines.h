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

#ifndef DEFINES_H_
#define DEFINES_H_

#include <glog/logging.h>
#include <time.h>

// Print the elapsed seconds to perform the action A.
#define CLOCK(A) {                                              \
    clock_t t1 = clock();                                       \
    A;                                                          \
    clock_t t2 = clock();                                       \
    LOG(INFO) << "Elapsed seconds: "                            \
              << (t2 - t1)/static_cast<float>(CLOCKS_PER_SEC);  \
  }

// Print the elapsed seconds to perform the action A, with a customized message.
#define CLOCK_MSG(A, M) {                                               \
    clock_t t1 = clock();                                               \
    A;                                                                  \
    clock_t t2 = clock();                                               \
    LOG(INFO) << M << (t2 - t1) / static_cast<float>(CLOCKS_PER_SEC);   \
  }

// Compute the elapsed seconds to perform the action A, and assign the result
// to the float pointer P.
#define CLOCK_PTR(A, P) {                                       \
    clock_t t1 = clock();                                       \
    A;                                                          \
    clock_t t2 = clock();                                       \
    *P = (t2 - t1) / static_cast<float>(CLOCKS_PER_SEC);        \
  }

#endif  // DEFINES_H_
