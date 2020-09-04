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

#include <share/example/stream/test.hpp>

#include <sstream>

BOOST_AUTO_TEST_CASE (share_example_stream_one_module_call)
{
  share_example_stream_test::run
    ( "stream.one_module_call"
    , [] (unsigned long size_slot)
    {
      std::ostringstream topology_description;

      topology_description << "process_and_mark_free:2," << (size_slot + 1);

      return topology_description.str();
    }
    , std::chrono::milliseconds (25)
    , 12.0
    );
}
