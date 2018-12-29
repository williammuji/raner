// Copyright (c) 2018 Williammuji Wong. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RANER_BASE_AUTO_RESET_H_
#define RANER_BASE_AUTO_RESET_H_

#include <utility>

#include "raner/macros.h"

// base::AutoReset<> is useful for setting a variable to a new value only within
// a particular scope. An base::AutoReset<> object resets a variable to its
// original value upon destruction, making it an alternative to writing
// "var = false;" or "var = old_val;" at all of a block's exit points.
//
// This should be obvious, but note that an base::AutoReset<> instance should
// have a shorter lifetime than its scoped_variable, to prevent invalid memory
// writes when the base::AutoReset<> object is destroyed.

namespace raner {

template <typename T>
class AutoReset {
 public:
  AutoReset(T* scoped_variable, T new_value)
      : scoped_variable_(scoped_variable),
        original_value_(std::move(*scoped_variable)) {
    *scoped_variable_ = std::move(new_value);
  }

  ~AutoReset() { *scoped_variable_ = std::move(original_value_); }

 private:
  T* scoped_variable_;
  T original_value_;

  DISALLOW_COPY_AND_ASSIGN(AutoReset);
};

}  // namespace raner

#endif  // RANER_BASE_AUTO_RESET_H_
