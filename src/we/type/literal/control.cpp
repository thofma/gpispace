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

#include <we/type/literal/control.hpp>

#include <iostream>
#include <string>

namespace we
{
  namespace type
  {
    namespace literal
    {
      bool operator== (const control&, const control&)
      {
        return true;
      }

      std::ostream& operator<< (std::ostream& os, const control&)
      {
        return os << std::string("[]");
      }

      std::size_t hash_value (const control&)
      {
        return 42;
      }

      bool operator< (const control&, const control&)
      {
        return false;
      }
    }
  }
}
