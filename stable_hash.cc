// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// -*- mode: C++ -*-
//
// Copyright 2022-2025 Google LLC
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
// Author: Siddharth Nayyar
// Author: Giuliano Procida

#include "stable_hash.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "graph.h"
#include "hashing.h"
#include "number.h"

namespace stg {

namespace {

// This combines 2 hash values while decaying (by shifting right) the second
// value. This prevents the most significant bits of the first hash from being
// affected by the decayed hash. Hash combination is done using a simple XOR
// operation to preserve the separation of higher and lower bits. Note that XOR
// is not a very effective method of mixing hash values if the values are
// generated with a weak hashing algorithm.
template <uint8_t decay>
HashValue DecayHashCombine(HashValue a, HashValue b) {
  static_assert(decay > 0 && decay < 32, "decay must lie inside (0, 32)");
  return HashValue(a.value ^ (b.value >> decay));
}

// Decaying hashes are combined in reverse since the each successive hashable
// should be decayed 1 more time than the previous hashable and the last
// hashable should receieve the most decay.
template <uint8_t decay, typename Type, typename Hash>
HashValue DecayHashCombineInReverse(const std::vector<Type>& hashables,
                                    Hash& hash) {
  HashValue result(0);
  for (auto it = hashables.crbegin(); it != hashables.crend(); ++it) {
    result = DecayHashCombine<decay>(hash(*it), result);
  }
  return result;
}

struct StableHashWorker {
  StableHashWorker(const Graph& graph, std::unordered_map<Id, HashValue>& cache)
      : graph(graph), cache(cache) {}

  HashValue operator()(Id id) {
    auto [it, inserted] = cache.emplace(id, 0);
    if (inserted) {
      it->second = graph.Apply(*this, id);
    }
    return it->second;
  }

  HashValue operator()(const Special& x) {
    switch (x.kind) {
      case Special::Kind::VOID:
        return hash("void");
      case Special::Kind::VARIADIC:
        return hash("variadic");
      case Special::Kind::DECLTYPE_NULLPTR:
        return hash("decltype_nullptr");
      case Special::Kind::DECLTYPE_AUTO:
        return hash("decltype_auto");
      case Special::Kind::AUTO:
        return hash("auto");
    }
  }

  HashValue operator()(const PointerReference& x) {
    return DecayHashCombine<2>(hash('r', static_cast<uint32_t>(x.kind)),
                               (*this)(x.pointee_type_id));
  }

  HashValue operator()(const PointerToMember& x) {
    return DecayHashCombine<16>(hash('n', (*this)(x.containing_type_id)),
                                (*this)(x.pointee_type_id));
  }

  HashValue operator()(const Typedef& x) {
    return hash('t', x.name);
  }

  HashValue operator()(const Qualified& x) {
    return DecayHashCombine<2>(hash('q', static_cast<uint32_t>(x.qualifier)),
                               (*this)(x.qualified_type_id));
  }

  HashValue operator()(const Primitive& x) {
    return hash('p', x.name);
  }

  HashValue operator()(const Array& x) {
    return DecayHashCombine<2>(hash('a', x.number_of_elements),
                               (*this)(x.element_type_id));
  }

  HashValue operator()(const BaseClass& x) {
    return DecayHashCombine<2>(hash('b', static_cast<uint32_t>(x.inheritance)),
                               (*this)(x.type_id));
  }

  HashValue operator()(const Method& x) {
    return hash(x.mangled_name);
  }

  HashValue operator()(const Member& x) {
    HashValue value = hash('m', x.name, x.bitsize);
    value = DecayHashCombine<20>(value, hash(x.offset));
    if (x.name.empty()) {
      return DecayHashCombine<2>(value, (*this)(x.type_id));
    } else {
      return DecayHashCombine<8>(value, (*this)(x.type_id));
    }
  }

  HashValue operator()(const VariantMember& x) {
    HashValue value = hash('v', x.name);
    value = DecayHashCombine<8>(value, (*this)(x.type_id));
    return x.discriminant_value
        ? DecayHashCombine<20>(value, hash(*x.discriminant_value))
        : value;
  }

  HashValue operator()(const StructUnion& x) {
    HashValue value = hash('S', static_cast<uint32_t>(x.kind), x.name,
                           static_cast<bool>(x.definition));
    if (!x.name.empty() || !x.definition) {
      return value;
    }

    auto h1 = DecayHashCombineInReverse<8>(x.definition->methods, *this);
    auto h2 = DecayHashCombineInReverse<8>(x.definition->members, *this);
    return DecayHashCombine<2>(value, HashValue(h1.value ^ h2.value));
  }

  HashValue operator()(const Enumeration& x) {
    HashValue value = hash('e', x.name, static_cast<bool>(x.definition));
    if (!x.name.empty() || !x.definition) {
      return value;
    }

    auto hash_enum = [this](const std::pair<std::string, Number>& e) {
      return hash(e.first, e.second);
    };
    return DecayHashCombine<2>(value, DecayHashCombineInReverse<8>(
        x.definition->enumerators, hash_enum));
  }

  HashValue operator()(const Variant& x) {
    HashValue value = hash('V', x.name, x.bytesize);
    if (x.discriminant.has_value()) {
      value = DecayHashCombine<12>(value, (*this)(x.discriminant.value()));
    }
    return DecayHashCombine<2>(value,
                               DecayHashCombineInReverse<8>(x.members, *this));
  }

  HashValue operator()(const Function& x) {
    return DecayHashCombine<2>(
        hash('f', (*this)(x.return_type_id)),
        DecayHashCombineInReverse<4>(x.parameters, *this));
  }

  HashValue operator()(const ElfSymbol& x) {
    HashValue value = hash('s', x.symbol_name);
    if (x.version_info) {
      value = DecayHashCombine<16>(
          value, hash(x.version_info->name, x.version_info->is_default));
    }
    return value;
  }

  HashValue operator()(const Interface&) {
    return hash("interface");
  }

  const Hash hash{};
  const Graph& graph;
  std::unordered_map<Id, HashValue>& cache;
};

}  // namespace

HashValue StableHash::operator()(Id id) {
  return StableHashWorker(graph_, cache_)(id);
}

}  // namespace stg
