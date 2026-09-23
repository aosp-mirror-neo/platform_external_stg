// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// -*- mode: C++ -*-
//
// Copyright 2022 Google LLC
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

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <catch2/catch.hpp>
#include "comparison.h"
#include "fidelity.h"
#include "graph.h"
#include "input.h"
#include "naming.h"
#include "reader_options.h"
#include "reporting.h"
#include "runtime.h"

namespace stg {
namespace {

struct IgnoreTestCase {
  const std::string name;
  const std::string file0;
  const std::string file1;
  const diff::Ignore ignore;
  const std::string expected_output;
  const bool expected_same;
};

std::string filename_to_path(const std::string& f) {
  return std::filesystem::path("testdata") / f;
}

Id Read(Runtime& runtime, Graph& graph, const std::string& input) {
  return Read(runtime, graph, InputFormat::STG, filename_to_path(input).c_str(),
              ReadOptions(), nullptr);
}

TEST_CASE("ignore") {
  const auto test = GENERATE(
      IgnoreTestCase(
          {"symbol type presence change",
           "symbol_type_presence_0.stg",
           "symbol_type_presence_1.stg",
           diff::Ignore(),
           "symbol_type_presence_small_diff",
           false}),
      IgnoreTestCase(
          {"symbol type presence change pruned",
           "symbol_type_presence_0.stg",
           "symbol_type_presence_1.stg",
           diff::Ignore(diff::Ignore::SYMBOL_TYPE_PRESENCE),
           "empty",
           true}),
      IgnoreTestCase(
          {"type declaration status change",
           "type_declaration_status_0.stg",
           "type_declaration_status_1.stg",
           diff::Ignore(),
           "type_declaration_status_small_diff",
           false}),
      IgnoreTestCase(
          {"type declaration status change pruned",
           "type_declaration_status_0.stg",
           "type_declaration_status_1.stg",
           diff::Ignore(diff::Ignore::TYPE_DECLARATION_STATUS),
           "empty",
           true}),
      IgnoreTestCase(
          {"primitive type encoding",
           "primitive_type_encoding_0.stg",
           "primitive_type_encoding_1.stg",
           diff::Ignore(),
           "primitive_type_encoding_small_diff",
           false}),
      IgnoreTestCase(
          {"primitive type encoding ignored",
           "primitive_type_encoding_0.stg",
           "primitive_type_encoding_1.stg",
           diff::Ignore(diff::Ignore::PRIMITIVE_TYPE_ENCODING),
           "empty",
           true}),
      IgnoreTestCase(
          {"member size",
           "member_size_0.stg",
           "member_size_1.stg",
           diff::Ignore(),
           "member_size_small_diff",
           false}),
      IgnoreTestCase(
          {"member size ignored",
           "member_size_0.stg",
           "member_size_1.stg",
           diff::Ignore(diff::Ignore::MEMBER_SIZE),
           "empty",
           true}),
      IgnoreTestCase(
          {"enum underlying type",
           "enum_underlying_type_0.stg",
           "enum_underlying_type_1.stg",
           diff::Ignore(),
           "enum_underlying_type_small_diff",
           false}),
      IgnoreTestCase(
          {"enum underlying type ignored",
           "enum_underlying_type_0.stg",
           "enum_underlying_type_1.stg",
           diff::Ignore(diff::Ignore::ENUM_UNDERLYING_TYPE),
           "empty",
           true}),
      IgnoreTestCase(
          {"qualifier",
           "qualifier_0.stg",
           "qualifier_1.stg",
           diff::Ignore(),
           "qualifier_small_diff",
           false}),
      IgnoreTestCase(
          {"qualifier ignored",
           "qualifier_0.stg",
           "qualifier_1.stg",
           diff::Ignore(diff::Ignore::QUALIFIER),
           "empty",
           true}),
      IgnoreTestCase(
          {"CRC change",
           "crc_change_0.stg",
           "crc_change_1.stg",
           diff::Ignore(),
           "crc_change_small_diff",
           false}),
      IgnoreTestCase(
          {"CRC change ignored",
           "crc_change_0.stg",
           "crc_change_1.stg",
           diff::Ignore(diff::Ignore::SYMBOL_CRC),
           "empty",
           true}),
      IgnoreTestCase(
          {"interface addition",
           "interface_addition_0.stg",
           "interface_addition_1.stg",
           diff::Ignore(),
           "interface_addition_small_diff",
           false}),
      IgnoreTestCase(
          {"interface addition ignored",
           "interface_addition_0.stg",
           "interface_addition_1.stg",
           diff::Ignore(diff::Ignore::INTERFACE_ADDITION),
           "empty",
           true}),
      IgnoreTestCase(
          {"type addition",
           "type_addition_0.stg",
           "type_addition_1.stg",
           diff::Ignore(),
           "type_addition_small_diff",
           false}),
      IgnoreTestCase(
          {"type addition ignored",
           "type_addition_0.stg",
           "type_addition_1.stg",
           diff::Ignore(diff::Ignore::INTERFACE_ADDITION),
           "empty",
           true}),
      IgnoreTestCase(
          {"type definition addition",
           "type_addition_1.stg",
           "type_addition_2.stg",
           diff::Ignore(),
           "type_definition_addition_small_diff",
           false}),
      IgnoreTestCase(
          {"type definition addition ignored",
           "type_addition_1.stg",
           "type_addition_2.stg",
           diff::Ignore(diff::Ignore::TYPE_DEFINITION_ADDITION),
           "empty",
           true})
      );

  SECTION(test.name) {
    std::ostringstream os;
    Runtime runtime(os, false);

    // Read inputs.
    Graph graph;
    const Id id0 = Read(runtime, graph, test.file0);
    const Id id1 = Read(runtime, graph, test.file1);

    // Compute differences.
    stg::diff::Outcomes outcomes;
    const auto comparison =
        diff::Compare(runtime, test.ignore, graph, id0, id1, outcomes);
    const bool same = comparison == diff::Comparison{};

    // Write SMALL reports.
    std::ostringstream output;
    if (!same) {
      NameCache names;
      const reporting::Options options{reporting::OutputFormat::SMALL};
      const reporting::Reporting reporting{graph, outcomes, options, names};
      Report(reporting, comparison, output);
    }

    // Check comparison outcome and report output.
    CHECK(same == test.expected_same);
    const std::ifstream expected_output_file(
        filename_to_path(test.expected_output));
    std::ostringstream expected_output;
    expected_output << expected_output_file.rdbuf();
    CHECK(output.str() == expected_output.str());
  }
}

struct ShortReportTestCase {
  const std::string name;
  const std::string file0;
  const std::string file1;
  const std::string expected_output;
};

TEST_CASE("short report") {
  const auto test = GENERATE(
      ShortReportTestCase(
          {"crc changes", "crc_0.stg", "crc_1.stg", "crc_changes_short_diff"}),
      ShortReportTestCase(
          {"only crc changes", "crc_only_0.stg", "crc_only_1.stg",
           "crc_only_changes_short_diff"}),
      ShortReportTestCase(
          {"offset changes", "offset_0.stg", "offset_1.stg",
           "offset_changes_short_diff"}),
      ShortReportTestCase(
          {"symbols added and removed", "added_removed_symbols_0.stg",
           "added_removed_symbols_1.stg", "added_removed_symbols_short_diff"}),
      ShortReportTestCase(
          {"symbols added and removed only", "added_removed_symbols_only_0.stg",
           "added_removed_symbols_only_1.stg",
           "added_removed_symbols_only_short_diff"}),
      ShortReportTestCase(
          {"enumerators added and removed", "added_removed_enumerators_0.stg",
           "added_removed_enumerators_1.stg",
           "added_removed_enumerators_short_diff"}));

  SECTION(test.name) {
    std::ostringstream os;
    Runtime runtime(os, false);

    // Read inputs.
    Graph graph;
    const Id id0 = Read(runtime, graph, test.file0);
    const Id id1 = Read(runtime, graph, test.file1);

    // Compute differences.
    stg::diff::Outcomes outcomes;
    const auto comparison =
        diff::Compare(runtime, {}, graph, id0, id1, outcomes);
    const bool same = comparison == diff::Comparison{};

    // Write SHORT reports.
    std::stringstream output;
    if (!same) {
      NameCache names;
      const reporting::Options options{reporting::OutputFormat::SHORT};
      const reporting::Reporting reporting{graph, outcomes, options, names};
      Report(reporting, comparison, output);
    }

    // Check comparison outcome and report output.
    CHECK(!same);
    const std::ifstream expected_output_file(
        filename_to_path(test.expected_output));
    std::ostringstream expected_output;
    expected_output << expected_output_file.rdbuf();
    CHECK(output.str() == expected_output.str());
  }
}

TEST_CASE("recursive naming") {
  std::ostringstream os;
  Runtime runtime(os, false);

  // Read inputs.
  Graph graph;
  const Id id0 = Read(runtime, graph, "recursive_naming_0.stg");
  const Id id1 = Read(runtime, graph, "recursive_naming_1.stg");

  // Compute differences.
  stg::diff::Outcomes outcomes;
  const auto comparison =
      diff::Compare(runtime, {}, graph, id0, id1, outcomes);
  const bool same = comparison == diff::Comparison{};

  // Write SHORT report.
  std::ostringstream output;
  if (!same) {
    NameCache names;
    const reporting::Options options{reporting::OutputFormat::SHORT};
    const reporting::Reporting reporting{graph, outcomes, options, names};
    Report(reporting, comparison, output);
  }

  // Check comparison outcome and report output.
  CHECK(!same);
  const std::ifstream expected_output_file(
      filename_to_path("recursive_naming_report"));
  std::ostringstream expected_output;
  expected_output << expected_output_file.rdbuf();
  CHECK(output.str() == expected_output.str());
}

TEST_CASE("fidelity diff") {
  std::ostringstream os;
  Runtime runtime(os, false);

  // Read inputs.
  Graph graph;
  const Id id0 = Read(runtime, graph, "fidelity_diff_0.stg");
  const Id id1 = Read(runtime, graph, "fidelity_diff_1.stg");

  // Compute fidelity diff.
  auto fidelity_diff = GetFidelityTransitions(graph, id0, id1);

  // Write fidelity diff report.
  std::ostringstream report;
  reporting::FidelityDiff(fidelity_diff, report);

  // Check report.
  const std::ifstream expected_report_file(
      filename_to_path("fidelity_diff_report"));
  std::ostringstream expected_report;
  expected_report << expected_report_file.rdbuf();
  CHECK(report.str() == expected_report.str());
}

}  // namespace
}  // namespace stg
