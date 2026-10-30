// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// -*- mode: C++ -*-
//
// Copyright 2024 Google LLC
//
// Licensed under the Apache License v2.0 with LLVM Exceptions (the
// "License"); you may not use this file except in compliance with the
// License.  You may obtain a copy of the License at
//
//     https://llvm.org/LICENSE.txt
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: Giuliano Procida

#include "scope.h"

#include <string>

#include <catch2/catch_test_macros.hpp>

namespace Test {

using stg::Scope;
using stg::PushScopeName;

TEST_CASE("scope") {
  Scope scope;
  CHECK(scope.name.empty());
  CHECK(scope.named);
  {
    const PushScopeName p1(scope, "1", "A");
    CHECK(!scope.name.empty());
    CHECK(scope.named);
    {
      const PushScopeName p2 (scope, "2", std::string());
      CHECK(!scope.name.empty());
      CHECK(!scope.named);
      {
        const PushScopeName p3(scope, "3", "B");
        CHECK(!scope.name.empty());
        CHECK(!scope.named);
      }
      CHECK(!scope.name.empty());
      CHECK(!scope.named);
    }
    CHECK(!scope.name.empty());
    CHECK(scope.named);
  }
  CHECK(scope.name.empty());
  CHECK(scope.named);
}

}  // namespace Test
