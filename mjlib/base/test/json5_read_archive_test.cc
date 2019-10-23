// Copyright 2019 Josh Pieper, jjp@pobox.com.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "mjlib/base/json5_read_archive.h"

#include <boost/test/auto_unit_test.hpp>

#include "mjlib/base/test/all_types_struct.h"

using namespace mjlib;
using base::Json5ReadArchive;
using DUT = Json5ReadArchive;
using base::test::AllTypesTest;

BOOST_AUTO_TEST_CASE(Json5ReadValidNumbers) {
  BOOST_TEST(DUT::Read<int>("2") == 2);
  BOOST_TEST(DUT::Read<uint64_t>("18446744073709551615") ==
             std::numeric_limits<uint64_t>::max());
  BOOST_TEST(DUT::Read<int64_t>("-9223372036854775808") ==
             std::numeric_limits<int64_t>::min());
  BOOST_TEST(DUT::Read<int64_t>("9223372036854775807") ==
             std::numeric_limits<int64_t>::max());

  BOOST_TEST(DUT::Read<double>("0") == 0.0);
  BOOST_TEST(DUT::Read<double>("0.0") == 0.0);
  BOOST_TEST(DUT::Read<double>("+0.0") == 0.0);
  BOOST_TEST(DUT::Read<double>("-0.0") == 0.0);

  BOOST_TEST(DUT::Read<double>("1") == 1.0);
  BOOST_TEST(DUT::Read<double>("356") == 356.0);
  BOOST_TEST(DUT::Read<double>("1.2") == 1.2);
  BOOST_TEST(DUT::Read<double>("+1.2") == 1.2);
  BOOST_TEST(DUT::Read<double>("-1.2") == -1.2);
  BOOST_TEST(DUT::Read<double>("1.2e3") == 1.2e3);
  BOOST_TEST(DUT::Read<double>("1.2e-3") == 1.2e-3);
  BOOST_TEST(DUT::Read<double>("1.2e-31") == 1.2e-31);
  BOOST_TEST(DUT::Read<double>("13.21e-31") == 13.21e-31);
  BOOST_TEST(DUT::Read<double>(".123") == 0.123);
  BOOST_TEST(DUT::Read<double>("Infinity") ==
             std::numeric_limits<double>::infinity());
  BOOST_TEST(DUT::Read<double>("-Infinity") ==
             -std::numeric_limits<double>::infinity());
  BOOST_TEST(!std::isfinite(DUT::Read<double>("NaN")));

  BOOST_TEST(DUT::Read<int>("0x10") == 16);
  BOOST_TEST(DUT::Read<int>("-0x10") == -16);
  BOOST_TEST(DUT::Read<int>("0o10") == 8);
  BOOST_TEST(DUT::Read<int>("-0o10") == -8);
  BOOST_TEST(DUT::Read<int>("0b10") == 2);
  BOOST_TEST(DUT::Read<int>("-0b10") == -2);
}

BOOST_AUTO_TEST_CASE(Json5ReadValidStrings) {
  BOOST_TEST(DUT::Read<std::string>("\"hello\"") == "hello");
  BOOST_TEST(DUT::Read<std::string>(
                 "\"\\\\\\b\\f\\n\\r\\t\\v\\x20\\'\\\"\"") ==
             "\\\b\f\n\r\t\v '\"");
}

BOOST_AUTO_TEST_CASE(Json5ReadTime) {
  BOOST_TEST(DUT::Read<boost::posix_time::ptime>(
                 "\"2002-01-20 23:59:59.000\"") ==
             boost::posix_time::time_from_string("2002-01-20 23:59:59.000"));
  BOOST_TEST(DUT::Read<boost::posix_time::time_duration>(
                 "\"23:59:59.000\"") ==
             boost::posix_time::duration_from_string("23:59:59.000"));
}

BOOST_AUTO_TEST_CASE(Json5ReadOptional) {
  BOOST_TEST((DUT::Read<std::optional<int>>("null") == std::optional<int>{}));
  BOOST_TEST((*DUT::Read<std::optional<int>>("1234") == 1234));
}

namespace {
struct SimpleStruct {
  int a = -1;

  template <typename Archive>
  void Serialize(Archive* ar) {
    ar->Visit(MJ_NVP(a));
  }

  bool operator==(const SimpleStruct& rhs) const {
    return a == rhs.a;
  }
};
}

BOOST_AUTO_TEST_CASE(Json5ReadSerializable) {
  BOOST_TEST((DUT::Read<SimpleStruct>("{a:3}") ==
              SimpleStruct{3}));
}

BOOST_AUTO_TEST_CASE(Json5ReadVector) {
  BOOST_TEST(DUT::Read<std::vector<int>>("[]") == std::vector<int>());
  BOOST_TEST(DUT::Read<std::vector<int>>("[1]") == std::vector<int>({1}));
  BOOST_TEST(DUT::Read<std::vector<int>>("[1,]") == std::vector<int>({1}));
  BOOST_TEST(DUT::Read<std::vector<int>>("[ 1 , 4 , 5  ]") ==
             std::vector<int>({1, 4, 5}));

  BOOST_TEST(DUT::Read<std::vector<SimpleStruct>>("[{a : 1}, {a : 2},]") ==
             std::vector<SimpleStruct>({ {1}, {2}}));
}

BOOST_AUTO_TEST_CASE(Json5ReadBytes) {
  BOOST_TEST(DUT::Read<base::Bytes>("[20, 21, 22]") ==
             base::Bytes({20, 21, 22}));
}

BOOST_AUTO_TEST_CASE(Json5ReadArray) {
  const auto expected = std::array<int, 3>{{3, 4, 5}};
  const auto actual = DUT::Read<std::array<int, 3>>("[3, 4, 5]");
  BOOST_TEST(actual == expected);
}

namespace {
constexpr const char* kAllTypes = R"XXX(
{
  "value_bool" : true,
  "value_i8" : -6,
  "value_i16" : -7,
  "value_i32" : -8,
  "value_i64" : -9,
  "value_u8" : 10,
  "value_u16" : 11,
  "value_u32" : 12,
  "value_u64" : 13,
  "value_f32" : 14.0,
  "value_f64" : 15.0,
  "value_bytes" : [ 2, 4, 5 ],
  "value_str" : "hello",
  "value_object" : { "value_u32" : 5 },
  "value_enum" : "kNextValue",
  "value_array" : [ {}, {} ],
  "value_optional" : 42,
  "value_timestamp" : "2005-01-20 23:59:59.000",
  "value_duration" : "13:59:59.000",
}
)XXX";
}

BOOST_AUTO_TEST_CASE(Json5AllTypes) {
  AllTypesTest all_types;
  std::istringstream istr(kAllTypes);
  DUT dut(istr);
  dut.Accept(&all_types);
  BOOST_TEST(all_types.value_bool == true);
  BOOST_TEST(all_types.value_i8 == -6);
  BOOST_TEST(all_types.value_i16 == -7);
  BOOST_TEST(all_types.value_i32 == -8);
  BOOST_TEST(all_types.value_i64 == -9);
  BOOST_TEST(all_types.value_u8 == 10);
  BOOST_TEST(all_types.value_u16 == 11);
  BOOST_TEST(all_types.value_u32 == 12);
  BOOST_TEST(all_types.value_u64 == 13);
  BOOST_TEST(all_types.value_f32 == 14.0);
  BOOST_TEST(all_types.value_f64 == 15.0);
  BOOST_TEST(all_types.value_bytes == base::Bytes({2,4,5}));
  BOOST_TEST(all_types.value_str == "hello");
  BOOST_TEST(all_types.value_object.value_u32 == 5);
  BOOST_TEST((all_types.value_enum == base::test::TestEnumeration::kNextValue));
  BOOST_TEST(all_types.value_array.size() == 2);
  BOOST_TEST(*all_types.value_optional == 42);
  BOOST_TEST(all_types.value_timestamp ==
             boost::posix_time::time_from_string("2005-01-20 23:59:59.000"));
  BOOST_TEST(all_types.value_duration ==
             boost::posix_time::duration_from_string("13:59:59.000"));
}

BOOST_AUTO_TEST_CASE(Json5ReadReorderFields) {
  const auto result = DUT::Read<AllTypesTest>("{value_u8 : 9, value_i8 : -4}");
  BOOST_TEST(result.value_u8 == 9);
  BOOST_TEST(result.value_i8 == -4);
}

namespace {
struct Empty {
  template <typename Archive>
  void Serialize(Archive*) {}
};
}

BOOST_AUTO_TEST_CASE(Json5IgnoreField) {
  DUT::Read<Empty>(kAllTypes);
}

BOOST_AUTO_TEST_CASE(Json5ErrorMessage) {
  auto make_predicate = [](std::string expected) {
    return [expected](const mjlib::base::system_error& error) {
      const bool good =
          std::string(error.what()).find(expected) != std::string::npos;
      if (!good) {
        std::cerr << "Error: \n" << error.what() <<
            "\n does not have '" << expected << "'\n";
      }
      return good;
    };
  };
  BOOST_CHECK_EXCEPTION(
      DUT::Read<AllTypesTest>("a"),
      mjlib::base::system_error,
      make_predicate("1:1 Didn't find expected '{'"));
  BOOST_CHECK_EXCEPTION(
      DUT::Read<AllTypesTest>("  a"),
      mjlib::base::system_error,
      make_predicate("1:3 Didn't find expected '{'"));
  BOOST_CHECK_EXCEPTION(
      DUT::Read<AllTypesTest>("\n   a"),
      mjlib::base::system_error,
      make_predicate("2:4 Didn't find expected '{'"));
}
