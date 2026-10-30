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

#include "hex.h"

#include <cstdint>
#include <sstream>
#include <string_view>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace Test {

struct TestCase {
  std::string_view name;
  int value;
  std::string_view formatted;
};

TEST_CASE("Hex<uint32_t>") {
  const auto test = GENERATE(
      TestCase({"zero", 0, "0x0"}),
      TestCase({"half width", 0xabcd, "0xabcd"}),
      TestCase({"full width", 0x12345678, "0x12345678"}));

  INFO("testing with " << test.name << " value");
  std::ostringstream os;
  os << stg::Hex<uint32_t>(test.value);
  CHECK(os.str() == test.formatted);
}

TEST_CASE("self comparison") {
  const stg::Hex<uint8_t> a(0);
  CHECK(a == a);
  CHECK(!(a != a));
  CHECK(!(a < a));
  CHECK(a <= a);
  CHECK(!(a > a));
  CHECK(a >= a);
}

TEST_CASE("distinct comparison") {
  const stg::Hex<uint8_t> a(0);
  const stg::Hex<uint8_t> b(1);
  CHECK(!(a == b));
  CHECK(a != b);
  CHECK(a < b);
  CHECK(a <= b);
  CHECK(!(a > b));
  CHECK(!(a >= b));
}

}  // namespace Test
