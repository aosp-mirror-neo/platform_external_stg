// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// -*- mode: C++ -*-
//
// Copyright 2025 Google LLC
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

#include "number.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <sstream>
#include <vector>

#include <catch2/catch_test_macros.hpp>

namespace {

using stg::Number;

template<typename T>
void CheckStrings(T n) {
  const Number number(n);
  std::ostringstream os1;
  os1 << (n + 0);  // promote char to int
  const auto n_as_string = os1.str();
  std::ostringstream os2;
  os2 << number;
  CHECK(!os2.fail());
  const auto number_as_string = os2.str();
  CHECK(n_as_string == number_as_string);
}

template<typename T>
void CheckAllStrings() {
  auto min = std::numeric_limits<T>::min();
  auto max = std::numeric_limits<T>::max();
  for (auto n = min; n < max; ++n) {
    CheckStrings(n);
  }
  CheckStrings(max);
}

template<typename T>
void CheckLimitStrings() {
  auto min = std::numeric_limits<T>::min();
  auto max = std::numeric_limits<T>::max();
  CheckStrings(min);
  CheckStrings(max);
}

TEST_CASE("all int8_t via Number to string") {
  CheckAllStrings<int8_t>();
}

TEST_CASE("all uint8_t via Number to string") {
  CheckAllStrings<uint8_t>();
}

TEST_CASE("all int16_t via Number to string") {
  CheckAllStrings<int16_t>();
}

TEST_CASE("all uint16_t via Number to string") {
  CheckAllStrings<uint16_t>();
}

TEST_CASE("min/max int32_t via Number to string") {
  CheckLimitStrings<int32_t>();
}

TEST_CASE("min/max uint32_t via Number to string") {
  CheckLimitStrings<uint32_t>();
}

TEST_CASE("min/max int64_t via Number to string") {
  CheckLimitStrings<int64_t>();
}

TEST_CASE("min/max uint64_t via Number to string") {
  CheckLimitStrings<uint64_t>();
}

template<typename T>
void CheckZeroChunks() {
  const Number zero;
  const Number four(4);
  for (size_t ix = 0; ix < 12; ++ix) {
    std::vector<T> chunks;
    chunks.resize(ix);
    const auto number = Number::FromChunks(chunks);
    CHECK(number == zero);
  }
  for (size_t ix = 1; ix < 12; ++ix) {
    std::vector<T> chunks;
    chunks.resize(ix);
    chunks[0] = 4;
    const auto number = Number::FromChunks(chunks);
    CHECK(number == four);
  }
}

TEST_CASE("zero chunks") {
  CheckZeroChunks<uint8_t>();
  CheckZeroChunks<uint16_t>();
  CheckZeroChunks<uint32_t>();
  CheckZeroChunks<uint64_t>();
}

TEST_CASE("Number to chunks and back") {
  for (int64_t a = 0; a < 2; ++a) {
    for (int64_t b = 0; b < 2; ++b) {
      for (int64_t c = 0; c < 2; ++c) {
        for (int64_t d = 0; d < 2; ++d) {
          for (int64_t e = 0; e < 2; ++e) {
            for (int64_t f = 0; f < 2; ++f) {
              for (int64_t g = 0; g < 2; ++g) {
                const int64_t n = (a << 56) + (b << 55) + (c << 32)
                                  + (d << 31) + (e << 8) + (f << 7) + g;
                GIVEN(n) {
                  const Number number(n);
                  const auto chunks8 = Number::ToChunks<uint8_t>(number);
                  const auto chunks16 = Number::ToChunks<uint16_t>(number);
                  const auto chunks32 = Number::ToChunks<uint32_t>(number);
                  const auto chunks64 = Number::ToChunks<uint64_t>(number);
                  const Number number8 = Number::FromChunks(chunks8);
                  const Number number16 = Number::FromChunks(chunks16);
                  const Number number32 = Number::FromChunks(chunks32);
                  const Number number64 = Number::FromChunks(chunks64);
                  CHECK(number8 == number);
                  CHECK(number16 == number);
                  CHECK(number32 == number);
                  CHECK(number64 == number);
                  CHECK(chunks64.size() == (n ? 1 : 0));
                  if (!chunks64.empty()) {
                    CHECK(static_cast<int64_t>(chunks64[0]) == n);
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

template<typename T>
void CheckChunks(const std::vector<T>& chunks) {
  const Number number = Number::FromChunks(chunks);
  std::ostringstream os;
  os << number;
  auto chunks1 = Number::ToChunks<T>(number);
  const T zeros = 0;
  const T ones = ~zeros;
  const T top = 1ULL << (sizeof(T) * 8 - 1);
  const T extension
      = chunks1.empty() || (chunks1.back() & top) == 0 ? zeros : ones;
  while (chunks1.size() < chunks.size()) {
    chunks1.push_back(extension);
  }
  CHECK(chunks1 == chunks);
}

TEST_CASE("1-byte chunks to Number and back") {
  for (size_t length = 0; length <= 10; ++length) {
    std::vector<uint8_t> chunks;
    chunks.resize(length);
    for (size_t bits = 0; bits < (1ULL << (2 * length)); ++bits) {
      for (size_t probe = 0; probe < length; ++probe) {
        uint8_t chunk = 0;
        if (bits & (1ULL << (2 * probe))) {
          chunk = chunk | 0x7f;
        }
        if (bits & (1ULL << (2 * probe + 1))) {
          chunk = chunk | 0x80;
        }
        chunks[probe] = chunk;
      }
      CheckChunks(chunks);
    }
  }
}

TEST_CASE("2-byte chunk to Number and back") {
  for (size_t length = 0; length <= 5; ++length) {
    std::vector<uint16_t> chunks;
    chunks.resize(length);
    for (size_t bits = 0; bits < (1ULL << (4 * length)); ++bits) {
      for (size_t probe = 0; probe < length; ++probe) {
        uint16_t chunk = 0;
        if (bits & (1ULL << (4 * probe))) {
          chunk = chunk | 0x7f;
        }
        if (bits & (1ULL << (4 * probe + 1))) {
          chunk = chunk | 0x80;
        }
        if (bits & (1ULL << (4 * probe + 2))) {
          chunk = chunk | 0x7f00;
        }
        if (bits & (1ULL << (4 * probe + 3))) {
          chunk = chunk | 0x8000;
        }
        chunks[probe] = chunk;
      }
      CheckChunks(chunks);
    }
  }
}

TEST_CASE("4-byte chunk to Number and back") {
  for (size_t length = 0; length <= 3; ++length) {
    std::vector<uint32_t> chunks;
    chunks.resize(length);
    for (size_t bits = 0; bits < (1ULL << (8 * length)); ++bits) {
      for (size_t probe = 0; probe < length; ++probe) {
        uint32_t chunk = 0;
        if (bits & (1ULL << (8 * probe))) {
          chunk = chunk | 0x7f;
        }
        if (bits & (1ULL << (8 * probe + 1))) {
          chunk = chunk | 0x80;
        }
        if (bits & (1ULL << (8 * probe + 2))) {
          chunk = chunk | 0x7f00;
        }
        if (bits & (1ULL << (8 * probe + 3))) {
          chunk = chunk | 0x8000;
        }
        if (bits & (1ULL << (8 * probe + 4))) {
          chunk = chunk | 0x7f0000;
        }
        if (bits & (1ULL << (8 * probe + 5))) {
          chunk = chunk | 0x800000;
        }
        if (bits & (1ULL << (8 * probe + 6))) {
          chunk = chunk | 0x7f000000;
        }
        if (bits & (1ULL << (8 * probe + 7))) {
          chunk = chunk | 0x80000000;
        }
        chunks[probe] = chunk;
      }
      CheckChunks(chunks);
    }
  }
}

TEST_CASE("8-byte chunk to Number and back") {
  for (size_t length = 0; length <= 2; ++length) {
    std::vector<uint64_t> chunks;
    chunks.resize(length);
    for (size_t bits = 0; bits < (1ULL << (8 * length)); ++bits) {
      for (size_t probe = 0; probe < length; ++probe) {
        uint64_t chunk = 0;
        if (bits & (1ULL << (8 * probe))) {
          chunk = chunk | 0x7f;
        }
        if (bits & (1ULL << (8 * probe + 1))) {
          chunk = chunk | 0x80;
        }
        if (bits & (1ULL << (8 * probe + 2))) {
          chunk = chunk | 0x7f00;
        }
        if (bits & (1ULL << (8 * probe + 3))) {
          chunk = chunk | 0x8000;
        }
        if (bits & (1ULL << (8 * probe + 4))) {
          chunk = chunk | 0x7fffffffff0000;
        }
        if (bits & (1ULL << (8 * probe + 5))) {
          chunk = chunk | 0x80000000000000;
        }
        if (bits & (1ULL << (8 * probe + 6))) {
          chunk = chunk | 0x7f00000000000000;
        }
        if (bits & (1ULL << (8 * probe + 7))) {
          chunk = chunk | 0x8000000000000000;
        }
        chunks[probe] = chunk;
      }
      CheckChunks(chunks);
    }
  }
}

constexpr std::array kPowersOf2 = {
  "1",
  "2",
  "4",
  "8",
  "16",
  "32",
  "64",
  "128",
  "256",
  "512",
  "1024",
  "2048",
  "4096",
  "8192",
  "16384",
  "32768",
  "65536",
  "131072",
  "262144",
  "524288",
  "1048576",
  "2097152",
  "4194304",
  "8388608",
  "16777216",
  "33554432",
  "67108864",
  "134217728",
  "268435456",
  "536870912",
  "1073741824",
  "2147483648",
  "4294967296",
  "8589934592",
  "17179869184",
  "34359738368",
  "68719476736",
  "137438953472",
  "274877906944",
  "549755813888",
  "1099511627776",
  "2199023255552",
  "4398046511104",
  "8796093022208",
  "17592186044416",
  "35184372088832",
  "70368744177664",
  "140737488355328",
  "281474976710656",
  "562949953421312",
  "1125899906842624",
  "2251799813685248",
  "4503599627370496",
  "9007199254740992",
  "18014398509481984",
  "36028797018963968",
  "72057594037927936",
  "144115188075855872",
  "288230376151711744",
  "576460752303423488",
  "1152921504606846976",
  "2305843009213693952",
  "4611686018427387904",
  "9223372036854775808",
  "18446744073709551616",
  "36893488147419103232",
  "73786976294838206464",
  "147573952589676412928",
  "295147905179352825856",
  "590295810358705651712",
  "1180591620717411303424",
  "2361183241434822606848",
  "4722366482869645213696",
  "9444732965739290427392",
  "18889465931478580854784",
  "37778931862957161709568",
  "75557863725914323419136",
  "151115727451828646838272",
  "302231454903657293676544",
  "604462909807314587353088",
  "1208925819614629174706176",
  "2417851639229258349412352",
  "4835703278458516698824704",
  "9671406556917033397649408",
  "19342813113834066795298816",
  "38685626227668133590597632",
  "77371252455336267181195264",
  "154742504910672534362390528",
  "309485009821345068724781056",
  "618970019642690137449562112",
  "1237940039285380274899124224",
  "2475880078570760549798248448",
  "4951760157141521099596496896",
  "9903520314283042199192993792",
  "19807040628566084398385987584",
  "39614081257132168796771975168",
  "79228162514264337593543950336",
  "158456325028528675187087900672",
  "316912650057057350374175801344",
  "633825300114114700748351602688",
  "1267650600228229401496703205376",
  "2535301200456458802993406410752",
  "5070602400912917605986812821504",
  "10141204801825835211973625643008",
  "20282409603651670423947251286016",
  "40564819207303340847894502572032",
  "81129638414606681695789005144064",
  "162259276829213363391578010288128",
  "324518553658426726783156020576256",
  "649037107316853453566312041152512",
  "1298074214633706907132624082305024",
  "2596148429267413814265248164610048",
  "5192296858534827628530496329220096",
  "10384593717069655257060992658440192",
  "20769187434139310514121985316880384",
  "41538374868278621028243970633760768",
  "83076749736557242056487941267521536",
  "166153499473114484112975882535043072",
  "332306998946228968225951765070086144",
  "664613997892457936451903530140172288",
  "1329227995784915872903807060280344576",
  "2658455991569831745807614120560689152",
  "5316911983139663491615228241121378304",
  "10633823966279326983230456482242756608",
  "21267647932558653966460912964485513216",
  "42535295865117307932921825928971026432",
  "85070591730234615865843651857942052864",
  "170141183460469231731687303715884105728",
  "340282366920938463463374607431768211456",
  "680564733841876926926749214863536422912",
  "1361129467683753853853498429727072845824",
  "2722258935367507707706996859454145691648",
  "5444517870735015415413993718908291383296",
  "10889035741470030830827987437816582766592",
  "21778071482940061661655974875633165533184",
  "43556142965880123323311949751266331066368",
  "87112285931760246646623899502532662132736",
  "174224571863520493293247799005065324265472",
  "348449143727040986586495598010130648530944",
  "696898287454081973172991196020261297061888",
  "1393796574908163946345982392040522594123776",
  "2787593149816327892691964784081045188247552",
  "5575186299632655785383929568162090376495104",
  "11150372599265311570767859136324180752990208",
  "22300745198530623141535718272648361505980416",
  "44601490397061246283071436545296723011960832",
  "89202980794122492566142873090593446023921664",
  "178405961588244985132285746181186892047843328",
  "356811923176489970264571492362373784095686656",
  "713623846352979940529142984724747568191373312",
  "1427247692705959881058285969449495136382746624",
  "2854495385411919762116571938898990272765493248",
  "5708990770823839524233143877797980545530986496",
  "11417981541647679048466287755595961091061972992",
  "22835963083295358096932575511191922182123945984",
  "45671926166590716193865151022383844364247891968",
  "91343852333181432387730302044767688728495783936",
  "182687704666362864775460604089535377456991567872",
  "365375409332725729550921208179070754913983135744",
  "730750818665451459101842416358141509827966271488",
  "1461501637330902918203684832716283019655932542976",
};

TEST_CASE("unsigned bytes powers") {
  for (size_t ix = 0; ix < kPowersOf2.size(); ++ix) {
    const auto& decimal = kPowersOf2[ix];
    const auto byte = ix / 8;
    const auto bit = ix % 8;
    std::vector<char> bytes(byte + 1, 0);
    bytes[byte] = 1 << bit;
    const auto number = Number::FromUnsignedBytes({bytes.data(), bytes.size()});
    std::ostringstream os;
    os << number;
    CHECK(os.str() == decimal);
    bytes.push_back(0);
    const auto padded = Number::FromUnsignedBytes({bytes.data(), bytes.size()});
    CHECK(padded == number);
  }
}

TEST_CASE("signed bytes powers") {
  for (size_t ix = 0; ix < kPowersOf2.size(); ++ix) {
    const auto& decimal = kPowersOf2[ix];
    const auto byte = ix / 8;
    const auto bit = ix % 8;
    std::vector<char> bytes(byte + (bit == 7 ? 2 : 1), 0);
    bytes[byte] = 1 << bit;
    const auto number = Number::FromSignedBytes({bytes.data(), bytes.size()});
    std::ostringstream os;
    os << number;
    CHECK(os.str() == decimal);
    bytes.push_back(0);
    const auto padded = Number::FromUnsignedBytes({bytes.data(), bytes.size()});
    CHECK(padded == number);
  }
}

TEST_CASE("negative bytes powers") {
  for (size_t ix = 0; ix < kPowersOf2.size(); ++ix) {
    const auto& decimal = kPowersOf2[ix];
    const auto byte = ix / 8;
    const auto bit = ix % 8;
    std::vector<char> bytes(byte + 1, 0);
    for (auto extension_bit = bit; extension_bit < 8; ++extension_bit) {
      bytes[byte] |= 1 << extension_bit;
    }
    const auto number = Number::FromSignedBytes({bytes.data(), bytes.size()});
    std::ostringstream os;
    os << number;
    std::ostringstream decimal_os;
    decimal_os << '-' << decimal;
    CHECK(os.str() == decimal_os.str());
    bytes.push_back(-1);
    const auto padded = Number::FromSignedBytes({bytes.data(), bytes.size()});
    CHECK(padded == number);
  }
}

}  // namespace
