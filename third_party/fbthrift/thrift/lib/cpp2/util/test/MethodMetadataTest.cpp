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

#include <gtest/gtest.h>
#include <thrift/lib/cpp2/util/MethodMetadata.h>

using namespace ::testing;
using namespace apache::thrift;

TEST(MethodMetadataTest, Operations) {
  MethodMetadata::Data data("123", FunctionQualifier::NONE);

  MethodMetadata a = MethodMetadata::from_static(&data);
  EXPECT_FALSE(a.isOwning());
  EXPECT_EQ(a.name_str(), "123");
  EXPECT_EQ(a.name_view(), "123");

  MethodMetadata b = a.getCopy();
  EXPECT_FALSE(b.isOwning());
  EXPECT_EQ(b.name_str(), "123");
  EXPECT_EQ(b.name_view(), "123");

  data = MethodMetadata::Data("234", FunctionQualifier::NONE);
  EXPECT_FALSE(a.isOwning());
  EXPECT_EQ(a.name_str(), "234");
  EXPECT_EQ(a.name_view(), "234");

  {
    std::string tmp{"hello"};
    MethodMetadata aa(tmp);
    a = std::move(aa);
  }
  EXPECT_TRUE(a.isOwning());
  EXPECT_EQ(a.name_str(), "hello");
  EXPECT_EQ(a.name_view(), "hello");

  {
    std::string_view tmp = "world";
    MethodMetadata aa(tmp);
    a = std::move(aa);
  }
  EXPECT_TRUE(a.isOwning());
  EXPECT_EQ(a.name_str(), "world");
  EXPECT_EQ(a.name_view(), "world");

  {
    const char* tmp = "there";
    MethodMetadata aa(tmp);
    a = std::move(aa);
  }
  EXPECT_TRUE(a.isOwning());
  EXPECT_EQ(a.name_str(), "there");
  EXPECT_EQ(a.name_view(), "there");

  {
    folly::StringPiece tmp = "here";
    MethodMetadata aa(tmp);
    a = std::move(aa);
  }
  EXPECT_TRUE(a.isOwning());
  EXPECT_EQ(a.name_str(), "here");
  EXPECT_EQ(a.name_view(), "here");

  MethodMetadata c(a.getCopy());
  EXPECT_TRUE(c.isOwning());
  EXPECT_EQ(c.name_str(), "here");
  EXPECT_EQ(c.name_view(), "here");
}
