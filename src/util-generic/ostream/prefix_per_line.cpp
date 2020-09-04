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

#include <util-generic/ostream/prefix_per_line.hpp>

#include <iostream>
#include <string>

namespace fhg
{
  namespace util
  {
    namespace ostream
    {
      prefix_per_line::prefix_per_line (std::string prefix, std::ostream& os)
        : line_by_line
          ( [&os, prefix] (std::string const& line)
            {
              os << prefix << line << std::endl;
            }
          )
        , std::ostream (this)
      {}
    }
  }
}
