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

#include <fhg/util/read_bool.hpp>

#include <fhg/util/parse/position.hpp>
#include <fhg/util/parse/require.hpp>

#include <algorithm>
#include <iterator>
#include <stdexcept>

namespace fhg
{
  namespace util
  {
    bool read_bool (const std::string& _inp)
    {
      std::string inp;

      std::transform ( _inp.begin(), _inp.end()
                     , std::back_inserter (inp), tolower
                     );

      parse::position_string pos (inp);

      return parse::require::boolean (pos);
    }
  }
}
