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

#pragma once

#include <util-generic/testing/printer/generic.hpp>

#include <boost/preprocessor/punctuation/comma.hpp>

#include <utility>

FHG_BOOST_TEST_TEMPLATED_LOG_VALUE_PRINTER
  ( <typename First BOOST_PP_COMMA() typename Second>
  , std::pair<First BOOST_PP_COMMA() Second>
  , os
  , p
  )
{
  os << "<"
     << FHG_BOOST_TEST_PRINT_LOG_VALUE_HELPER (std::get<0> (p))
     << ", "
     << FHG_BOOST_TEST_PRINT_LOG_VALUE_HELPER (std::get<1> (p))
     << ">";
}
