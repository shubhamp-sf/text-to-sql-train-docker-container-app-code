/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <thrift/compiler/ast/t_node.h>

#include <array>
#include <memory>
#include <string>
#include <vector>

#include <folly/portability/GMock.h>
#include <folly/portability/GTest.h>

namespace apache {
namespace thrift {
namespace compiler {
namespace {

std::vector<std::string> capture_span(alias_span span) {
  return std::vector<std::string>(span.begin(), span.end());
}

TEST(TNodeTest, AliasSpan) {
  // Fails to work with string literal directly
  // Uncomment next line to produce expectd compile time error
  // EXPECT_THAT(capture_span("hi"), ::testing::ElementsAre("hi"));

  // Works with string litteral if wrapped in {}, via a conversion to a string
  // temporary.
  EXPECT_THAT(capture_span({"hi"}), ::testing::ElementsAre("hi"));

  EXPECT_THAT(capture_span({"hi", "bye"}), ::testing::ElementsAre("hi", "bye"));

  std::vector<std::string> vec = {"1", "2", "3"};
  alias_span span = vec;
  EXPECT_THAT(span, ::testing::ElementsAre("1", "2", "3"));

  std::array<std::string, 2> array = {{"a", "b"}};
  span = array;
  EXPECT_THAT(span, ::testing::ElementsAre("a", "b"));

  std::string str = "z";
  span = str;
  EXPECT_THAT(span, ::testing::ElementsAre("z"));
}

} // namespace
} // namespace compiler
} // namespace thrift
} // namespace apache
