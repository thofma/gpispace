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

#include <we/type/memory_buffer_info_t.hpp>
#include <we/type/value/show.hpp>

#include <util-generic/testing/random.hpp>

#include <boost/test/unit_test.hpp>
#include <cmath>

BOOST_AUTO_TEST_CASE (throw_if_the_alignment_is_not_a_power_of_2)
{
  expr::eval::context context;

  unsigned long const size
    {fhg::util::testing::random<unsigned long>()()};
  auto const size_expr (fhg::util::testing::random_identifier());
  context.bind_and_discard_ref ({size_expr}, size);
 
  unsigned long const exp
    (fhg::util::testing::random<unsigned long>{} (10, 2));
  unsigned long const alignment
    { fhg::util::testing::random<unsigned long>{}
        ((1ul << exp) - 1, (1ul << (exp - 1)) + 1)
    };
  auto const alignment_expr (fhg::util::testing::random_identifier());
    context.bind_and_discard_ref ({alignment_expr}, alignment);

  we::type::memory_buffer_info_t memory_buffer_info
    ("${" + size_expr + "}", "${" + alignment_expr + "}");

  BOOST_REQUIRE_EQUAL (memory_buffer_info.size (context), size);
  BOOST_REQUIRE_EXCEPTION 
    ( memory_buffer_info.alignment (context)
    , std::runtime_error
    , [&] (std::runtime_error const& exc)
      {
        return std::string (exc.what()).find
          ("Invalid alignment expression. "
           "The alignment should be a power of 2!"
          ) != std::string::npos;
      }
    );
} 

BOOST_AUTO_TEST_CASE (size_and_alignment_as_expressions)
{
  expr::eval::context context;

  unsigned long const size
    {fhg::util::testing::random<unsigned long>()()};
  auto const size_expr (fhg::util::testing::random_identifier());
  context.bind_and_discard_ref ({size_expr}, size);

  unsigned long const exp
    {fhg::util::testing::random<unsigned long>{} (10, 0)};
  unsigned long const alignment (1ul << exp);
  auto const alignment_expr (fhg::util::testing::random_identifier());
    context.bind_and_discard_ref ({alignment_expr}, alignment);

  we::type::memory_buffer_info_t memory_buffer_info
    ("${" + size_expr + "}", "${" + alignment_expr + "}");

  BOOST_REQUIRE_EQUAL (memory_buffer_info.size (context), size);
  BOOST_REQUIRE_EQUAL (memory_buffer_info.alignment (context), alignment);
}

BOOST_AUTO_TEST_CASE (size_and_alignment_as_constants)
{
  expr::eval::context context;

  unsigned long const size
    {fhg::util::testing::random<unsigned long>()()};

  unsigned long const exp
    {fhg::util::testing::random<unsigned long>{} (10, 0)};
  unsigned long const alignment (1ul << exp);

  std::ostringstream sstr_size;
  sstr_size << pnet::type::value::show (size);
  std::ostringstream sstr_alignment;
  sstr_alignment << pnet::type::value::show (alignment);
  we::type::memory_buffer_info_t memory_buffer_info
    (sstr_size.str(), sstr_alignment.str());

  BOOST_REQUIRE_EQUAL (memory_buffer_info.size (context), size);
  BOOST_REQUIRE_EQUAL (memory_buffer_info.alignment (context), alignment);
}
