// Copyright 2020 Josh Pieper, jjp@pobox.com.  All rights reserved.
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

#include "mjlib/telemetry/file_writer.h"

#include <sstream>
#include <string>

#include <fmt/format.h>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/test/auto_unit_test.hpp>

#include "mjlib/base/temporary_file.h"

using mjlib::telemetry::FileWriter;

namespace {
std::string Contents(const std::string& filename) {
  std::ifstream inf(filename);
  std::ostringstream ostr;
  ostr << inf.rdbuf();
  return ostr.str();
}

template <typename Array>
std::string MakeString(const Array& array) {
  return std::string(array, sizeof(array) - 1);
}

boost::posix_time::ptime MakeTimestamp(const std::string& str) {
  return boost::posix_time::time_from_string(str);
}

constexpr char kEmptyLog[] =
          "TLOG0003\x00"  // file header
          "\x03\x0e"  // block type kIndex, size=14
          "\x00\x00"  // flags=0 nelements=0
          "\x10\x00\x00\x00"  // size=16
          "TLOGIDEX";  // trailer
}

BOOST_AUTO_TEST_CASE(FileWriterHeaderTest) {
  mjlib::base::TemporaryFile temp;

  {
    FileWriter dut;
    BOOST_TEST(!dut.IsOpen());
    dut.Open(temp.native());
    BOOST_TEST(dut.IsOpen());
    dut.Close();
  }

  const auto contents = Contents(temp.native());
  BOOST_TEST(contents == MakeString(kEmptyLog));
}

BOOST_AUTO_TEST_CASE(FileWriterDestructorTest) {
  // The destructor should also be enough to flush the file.
  mjlib::base::TemporaryFile temp;

  {
    FileWriter dut;
    dut.Open(temp.native());
  }

  BOOST_TEST(Contents(temp.native()) == MakeString(kEmptyLog));
}

BOOST_AUTO_TEST_CASE(FileWriterIdentifierAllocate) {
  FileWriter dut;

  const auto id1 = dut.AllocateIdentifier("test1");
  const auto id2 = dut.AllocateIdentifier("test2");
  const auto id3 = dut.AllocateIdentifier("test3");

  // They should all be unique.
  BOOST_TEST(id1 != id2);
  BOOST_TEST(id2 != id3);
  BOOST_TEST(id1 != id3);

  // If we ask for the same name, we should get the same id back.
  const auto id2_copy = dut.AllocateIdentifier("test2");
  BOOST_TEST(id2_copy == id2);
}

BOOST_AUTO_TEST_CASE(FileWriterReserveSchema) {
  FileWriter dut;
  {
    const bool success = dut.ReserveIdentifier("test", 1);
    BOOST_TEST(!!success);
  }
  {
    const bool success = dut.ReserveIdentifier("test3", 3);
    BOOST_TEST(!!success);
  }

  std::vector<FileWriter::Identifier> ids;
  for (int i = 0; i < 20; i++) {
    const auto id = dut.AllocateIdentifier(fmt::format("auto{}", i));
    ids.push_back(id);
  }

  std::sort(ids.begin(), ids.end());
  // All the IDs we got back should be unique, and none should match
  // the ones we pre-reserved.
  for (size_t i = 0; i < ids.size(); i++) {
    BOOST_TEST(ids[i] != 1);
    BOOST_TEST(ids[i] != 3);
    if (i > 0) {
      BOOST_TEST(ids[i] != ids[i - 1]);
    }
  }
}

BOOST_AUTO_TEST_CASE(FileWriterWriteSchema) {
  mjlib::base::TemporaryFile temp;

  {
    FileWriter dut{temp.native()};
    const auto id = dut.AllocateIdentifier("test");
    dut.WriteSchema(id, "testschema");
  }

  const char expected[] =
      "TLOG0003\x00"  // file header
      "\x01\x11"  // BlockType - Schema, size=17
        "\x01\x00"  // id=1, flags = 0
        "\x04test"  // name
        "testschema"  // schema
      "\x03\x1f"  // block type kIndex, size=31
      "\x00\x01"  // flags=0 nelements=1
        "\x01" // id
          "\x09\x00\x00\x00\x00\x00\x00\x00"  // schema location
          "\xff\xff\xff\xff\xff\xff\xff\xff"  // final record
        "\x21\x00\x00\x00"
        "TLOGIDEX";
  const auto contents = Contents(temp.native());
  BOOST_TEST(contents == MakeString(expected));
}

namespace {
constexpr char kTestPrefix[] =
    "TLOG0003\x00"  // file header
    "\x01\x11"  // BlockType - Schema, size=17
    "\x01\x00"  // id=1, flags = 0
        "\x04test"  // name
    "testschema"  // schema
    ;
}

BOOST_AUTO_TEST_CASE(FileWriterWriteDataUncompressed) {
  mjlib::base::TemporaryFile temp;

  {
    FileWriter dut{temp.native(), []() {
        FileWriter::Options options;
        options.default_compression = false;
        return options;
      }()};
    const auto id = dut.AllocateIdentifier("test");
    dut.WriteSchema(id, "testschema");
    dut.WriteData(MakeTimestamp("2020-03-10 00:00:00"), id, "testdata");
  }

  const char suffix[] =
      "\x02\x13"  // BlockType = Data, size=19
      "\x01\x03"  // id=1, flags= (timestamp)
        "\x00"  // previous offset
        "\x00\x20\x07\xcd\x74\xa0\x05\x00"  // timestamp
        "testdata"

      "\x03\x1f"  // block type kIndex, size=31
      "\x00\x01"  // flags=0 nelements=1
        "\x01" // id
          "\x09\x00\x00\x00\x00\x00\x00\x00"  // schema location
          "\x1c\x00\x00\x00\x00\x00\x00\x00"  // final record
        "\x21\x00\x00\x00"
        "TLOGIDEX";
      ;

  const std::string expected =
      MakeString(kTestPrefix) +
      MakeString(suffix);

  const auto contents = Contents(temp.native());
  BOOST_TEST(contents == expected);
}

BOOST_AUTO_TEST_CASE(FileWriterWriteBlock) {
  // Test WriteBlock and GetBuffer
  mjlib::base::TemporaryFile temp;

  {
    FileWriter dut{temp.native()};
    const auto id = dut.AllocateIdentifier("test");
    dut.WriteSchema(id, "testschema");

    auto buffer = dut.GetBuffer();
    buffer->write({"\x01\x00test", 6});

    dut.WriteBlock(mjlib::telemetry::Format::BlockType::kData, std::move(buffer));
  }

  const char suffix[] =
      "\x02\x06\x01\x00test"
      "\x03\x1f"  // block type kIndex, size=31
      "\x00\x01"  // flags=0 nelements=1
        "\x01" // id
          "\x09\x00\x00\x00\x00\x00\x00\x00"  // schema location
          "\xff\xff\xff\xff\xff\xff\xff\xff"  // final record
        "\x21\x00\x00\x00"
        "TLOGIDEX";
      ;

  const std::string expected =
      MakeString(kTestPrefix) +
      MakeString(suffix);

  const auto contents = Contents(temp.native());
  BOOST_TEST(contents == expected);
}