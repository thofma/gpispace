// This file is part of GPI-Space.
// Copyright (C) 2020 Fraunhofer ITWM
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include <xml/parse/type/memory_buffer.hpp>

#include <fhg/util/xml.hpp>
#include <util-generic/testing/flatten_nested_exceptions.hpp>
#include <util-generic/testing/printer/optional.hpp>
#include <util-generic/testing/random/string.hpp>

#include <boost/format.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE (name_is_stored)
{
  std::string const name (fhg::util::testing::random_string());

  BOOST_REQUIRE_EQUAL
    ( name
    , xml::parse::type::memory_buffer_type
      ( xml::parse::util::position_type
        (nullptr, nullptr, fhg::util::testing::random_string())
      , name
      , fhg::util::testing::random_string()
      , fhg::util::testing::random_string()
      , boost::none
      , we::type::property::type()
      ).name()
    );
}

BOOST_AUTO_TEST_CASE (size_is_stored)
{
  std::string const size (fhg::util::testing::random_string());

  BOOST_REQUIRE_EQUAL
    ( size
    , xml::parse::type::memory_buffer_type
      ( xml::parse::util::position_type
        (nullptr, nullptr, fhg::util::testing::random_string())
      , fhg::util::testing::random_string()
      , size
      , fhg::util::testing::random_string()
      , boost::none
      , we::type::property::type()
      ).size()
    );
}

BOOST_AUTO_TEST_CASE (alignment_is_stored)
{
  auto const alignment (fhg::util::testing::random_string());

  BOOST_REQUIRE_EQUAL
    ( alignment
    , xml::parse::type::memory_buffer_type
      ( xml::parse::util::position_type
        (nullptr, nullptr, fhg::util::testing::random_string())
      , fhg::util::testing::random_string()
      , fhg::util::testing::random_string()
      , alignment
      , boost::none
      , we::type::property::type()
      ).alignment()
    );
}

namespace
{
  std::vector<boost::optional<bool>> tribool_values()
  {
    return {{false, false}, {true, true}, {true, false}};
  }
}

BOOST_DATA_TEST_CASE (read_only_is_stored, tribool_values(), read_only)
{
  BOOST_REQUIRE_EQUAL
    ( read_only
    , xml::parse::type::memory_buffer_type
        ( xml::parse::util::position_type
          (nullptr, nullptr, fhg::util::testing::random_string())
        , fhg::util::testing::random_string()
        , fhg::util::testing::random_string()
        , fhg::util::testing::random_string()
        , read_only
        , we::type::property::type()
        ).read_only()
      );
}

BOOST_AUTO_TEST_CASE (name_is_unique_key)
{
  std::string const name (fhg::util::testing::random_string());

  BOOST_REQUIRE_EQUAL
    ( name
    , xml::parse::type::memory_buffer_type
      ( xml::parse::util::position_type
        (nullptr, nullptr, fhg::util::testing::random_string())
      , name
      , fhg::util::testing::random_string()
      , fhg::util::testing::random_string()
      , boost::none
      , we::type::property::type()
      ).unique_key()
    );
}

BOOST_DATA_TEST_CASE (dump, tribool_values(), read_only)
{
  std::string const name (fhg::util::testing::random_identifier());
  std::string const size (fhg::util::testing::random_string_without_zero());
  std::string const alignment
    (fhg::util::testing::random_string_without_zero());

  xml::parse::type::memory_buffer_type mb
    ( xml::parse::util::position_type
        (nullptr, nullptr, fhg::util::testing::random_string())
    , name
    , size
    , alignment
    , read_only
    , we::type::property::type()
    );

  std::ostringstream oss;

  fhg::util::xml::xmlstream s (oss);

  xml::parse::type::dump::dump (s, mb);

  const std::string expected
    ( ( boost::format (R"EOS(<memory-buffer name="%1%"%3%>
  <size>%2%</size>
  <alignment>%4%</alignment>
</memory-buffer>)EOS")
      % name
      % size
      % ( !boost::get_pointer (read_only) ? std::string()
        : ( boost::format (R"EOS( read-only="%1%")EOS")
          % (*read_only ? "true" : "false")
          ).str()
        )
      % alignment
      ).str()
    );

  BOOST_REQUIRE_EQUAL (expected, oss.str());
}
