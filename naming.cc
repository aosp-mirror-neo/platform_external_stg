// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// -*- mode: C++ -*-
//
// Copyright 2020-2024 Google LLC
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
// Author: Ignes Simeonova

#include "naming.h"

#include <cstddef>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "graph.h"
#include "scc.h"

namespace stg {

Name Name::Add(Side side, Precedence precedence,
               const std::string& text) const {
  const bool bracket = precedence < precedence_;
  std::ostringstream left;
  std::ostringstream right;

  // Bits on the left require whitespace separation when an identifier is being
  // added. While it would be simpler to unconditionally add a space, we choose
  // to only do this for identifiers and not for pointer and reference tokens,
  // except for the longer pointer-to-member syntax.
  //
  // For illegal types containing && & or & && this could result in &&&.
  left << left_;
  if (bracket) {
    left << '(';
  } else if (side == Side::LEFT
             && (precedence == Precedence::ATOMIC || text.size() > 2)) {
    left << ' ';
  }

  (side == Side::LEFT ? left : right) << text;

  // Bits on the right are arrays [] and functions () and need no whitespace.
  if (bracket) {
    right << ')';
  }
  right << right_;

  return Name{left.str(), precedence, right.str()};
}

Name Name::Qualify(Qualifier qualifier) const {
  std::ostringstream os;
  // Qualifiers attach without affecting precedence but the precedence
  // determines the relative position of the qualifier.
  switch (precedence_) {
    case Precedence::NIL: {
      // Add qualifier to the left of the type stem.
      //
      // This gives the more popular format (const int rather than int const)
      // and is safe because NIL precedence types are always leaf syntax.
      os << qualifier << ' ' << left_;
      return Name{os.str(), precedence_, right_};
    }
    case Precedence::POINTER: {
      // Add qualifier to the right of the sigil.
      //
      // TODO: consider dropping ' ' here.
      os << left_ << ' ' << qualifier;
      return Name{os.str(), precedence_, right_};
    }
    case Precedence::ARRAY_FUNCTION: {
      // Qualifiers should not normally apply to arrays or functions.
      os << '{' << qualifier << ">}" << right_;
      return Name{left_, precedence_, os.str()};
    }
    case Precedence::ATOMIC: {
      // Qualifiers should not normally apply to names.
      os << left_ << "{<" << qualifier << '}';
      return Name{os.str(), precedence_, right_};
    }
  }
}

std::ostream& Name::Print(std::ostream& os) const {
  return os << left_ << right_;
}

std::string Name::ToString() const {
  return left_ + right_;
}

std::ostream& operator<<(std::ostream& os, const Name& name) {
  return name.Print(os);
}

namespace {

struct DescribeWorker {
  DescribeWorker(const Graph& graph, NameCache& names)
      : graph(graph), names(names) {}

  Name operator()(Id id) {
    static const Name black_hole{"#"};
    const auto it = names.find(id);
    if (it != names.end()) {
      return it->second;
    }
    const auto handle = scc.Open(id);
    if (!handle) {
      // a recursive visit, stub out the description
      return black_hole;
    }

    const auto name = graph.Apply(*this, id);

    const auto scc_ids = scc.Close(*handle);
    if (scc_ids.empty()) {
      // first visit to a non-root node in an SCC, stub out the description
      return black_hole;
    }
    // completed SCC
    if (scc_ids.size() == 1) {
      // either a trivial SCC or a self-loop, either way store the description
      names.emplace(id, name);
      return name;
    }
    // a rare non-trivial SCC, ensure all descriptions are unrolled exactly once
    for (const auto scc_id : scc_ids) {
      names.emplace(scc_id, black_hole);
    }
    std::vector<Name> scc_names;
    scc_names.reserve(scc_ids.size());
    for (const auto scc_id : scc_ids) {
      scc_names.push_back(graph.Apply(*this, scc_id));
    }
    for (size_t ix = 0; ix < scc_ids.size(); ++ix) {
      using std::swap;
      swap(names.at(scc_ids[ix]), scc_names[ix]);
    }
    return names.at(id);
  }

  Name operator()(const Special& x) {
    switch (x.kind) {
      case Special::Kind::VOID:
        return Name{"void"};
      case Special::Kind::VARIADIC:
        return Name{"..."};
      case Special::Kind::DECLTYPE_NULLPTR:
        return Name{"decltype(nullptr)"};
      case Special::Kind::DECLTYPE_AUTO:
        return Name{"decltype(auto)"};
      case Special::Kind::AUTO:
        return Name{"auto"};
    }
  }

  Name operator()(const PointerReference& x) {
    std::string sign;
    switch (x.kind) {
      case PointerReference::Kind::POINTER:
        sign = "*";
        break;
      case PointerReference::Kind::LVALUE_REFERENCE:
        sign = "&";
        break;
      case PointerReference::Kind::RVALUE_REFERENCE:
        sign = "&&";
        break;
    }
    return (*this)(x.pointee_type_id)
            .Add(Side::LEFT, Precedence::POINTER, sign);
  }

  Name operator()(const PointerToMember& x) {
    std::ostringstream os;
    os << (*this)(x.containing_type_id) << "::*";
    return (*this)(x.pointee_type_id).Add(Side::LEFT, Precedence::POINTER,
                                          os.str());
  }

  Name operator()(const Typedef& x) {
    return Name{x.name};
  }

  Name operator()(const Qualified& x) {
    return (*this)(x.qualified_type_id).Qualify(x.qualifier);
  }

  Name operator()(const Primitive& x) {
    return Name{x.name};
  }

  Name operator()(const Array& x) {
    std::ostringstream os;
    os << '[' << x.number_of_elements << ']';
    return (*this)(x.element_type_id)
            .Add(Side::RIGHT, Precedence::ARRAY_FUNCTION, os.str());
  }

  Name operator()(const BaseClass& x) {
    return (*this)(x.type_id);
  }

  Name operator()(const Method& x) {
    return (*this)(x.type_id).Add(Side::LEFT, Precedence::ATOMIC, x.name);
  }

  Name operator()(const Member& x) {
    auto description = (*this)(x.type_id);
    if (!x.name.empty()) {
      description = description.Add(Side::LEFT, Precedence::ATOMIC, x.name);
    }
    if (x.bitsize) {
      description = description.Add(
          Side::RIGHT, Precedence::ATOMIC, ':' + std::to_string(x.bitsize));
    }
    return description;
  }

  Name operator()(const VariantMember& x) {
    auto description = (*this)(x.type_id);
    description = description.Add(Side::LEFT, Precedence::ATOMIC, x.name);
    return description;
  }

  Name operator()(const StructUnion& x) {
    std::ostringstream os;
    os << x.kind;
    if (!x.name.empty()) {
      os << ' ' << x.name;
    } else if (x.definition) {
      os << " { ";
      for (const auto& member : x.definition->members) {
        os << (*this)(member) << "; ";
      }
      os << '}';
    }
    return Name{os.str()};
  }

  Name operator()(const Enumeration& x) {
    std::ostringstream os;
    os << "enum";
    if (!x.name.empty()) {
      os << ' ' << x.name;
    } else if (x.definition) {
      os << " { ";
      for (const auto& e : x.definition->enumerators) {
        os << e.first << " = " << e.second << ", ";
      }
      os << '}';
    }
    return Name{os.str()};
  }

  Name operator()(const Variant& x) {
    std::ostringstream os;
    os << "variant " << x.name;
    return Name{os.str()};
  }

  Name operator()(const Function& x) {
    std::ostringstream os;
    os << '(';
    bool sep = false;
    for (const Id p : x.parameters) {
      if (sep) {
        os << ", ";
      } else {
        sep = true;
      }
      os << (*this)(p);
    }
    os << ')';
    return (*this)(x.return_type_id)
            .Add(Side::RIGHT, Precedence::ARRAY_FUNCTION, os.str());
  }

  Name operator()(const ElfSymbol& x) {
    const auto& name = x.full_name ? *x.full_name : x.symbol_name;
    return x.type_id
        ? (*this)(*x.type_id).Add(Side::LEFT, Precedence::ATOMIC, name)
        : Name{name};
  }

  Name operator()(const Interface&) {
    return Name{"interface"};
  }

  const Graph& graph;
  NameCache& names;
  SCC<Id> scc;
};

struct DescribeKindWorker {
  explicit DescribeKindWorker(const Graph& graph) : graph(graph) {}

  std::string operator()(Id id) {
    return graph.Apply(*this, id);
  }

  std::string operator()(const BaseClass&) {
    return "base class";
  }

  std::string operator()(const Method&) {
    return "method";
  }

  std::string operator()(const Member&) {
    return "member";
  }

  std::string operator()(const ElfSymbol& x) {
    std::ostringstream os;
    os << x.symbol_type << " symbol";
    return os.str();
  }

  std::string operator()(const Interface&) {
    return "interface";
  }

  template <typename Node>
      std::string operator()(const Node&) {
    return "type";
  }

  const Graph& graph;
};

struct DescribeExtraWorker {
  explicit DescribeExtraWorker(const Graph& graph) : graph(graph) {}

  std::string operator()(Id id) {
    return graph.Apply(*this, id);
  }

  std::string operator()(const ElfSymbol& x) {
    const auto& name = x.full_name ? *x.full_name : x.symbol_name;
    auto versioned = VersionedSymbolName(x);
    return name == versioned ? std::string() : " {" + versioned + '}';
  }

  template <typename Node>
      std::string operator()(const Node&) {
    return {};
  }

  const Graph& graph;
};

}  // namespace

Name Describe::operator()(Id id) {
  return DescribeWorker(graph, names)(id);
}

std::string DescribeKind::operator()(Id id) {
  return DescribeKindWorker(graph)(id);
}

std::string DescribeExtra::operator()(Id id) {
  return DescribeExtraWorker(graph)(id);
}

}  // namespace stg
