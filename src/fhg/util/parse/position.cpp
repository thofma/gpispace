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

#include <fhg/util/parse/position.hpp>

#include <fhg/util/parse/error.hpp>
#include <fhg/util/parse/require.hpp>

#include <sstream>

namespace fhg
{
  namespace util
  {
    namespace parse
    {
      position_string::position_string (const std::string& input)
        : _k (0)
        , _pos (input.begin())
        , _begin (input.begin())
        , _end (input.end())
      {}
      position_string::position_string ( const std::string::const_iterator& begin
                                       , const std::string::const_iterator& end
                                       )
        : _k (0)
        , _pos (begin)
        , _begin (begin)
        , _end (end)
      {}

      std::string position_string::error_message (const std::string& message) const
      {
        std::ostringstream oss;

        oss << "PARSE ERROR [" << eaten() << "]: " << message << std::endl;
        oss << std::string (_begin, _pos) << ' '
            << std::string (_pos, _end) << std::endl;
        oss << std::string (eaten(), ' ') << "^" << std::endl;

        return oss.str();
      }
    }
  }
}

namespace fhg
{
  namespace util
  {
    namespace parse
    {
      position_vector_of_char::position_vector_of_char (const std::vector<char>& input)
        : _k (0)
        , _pos (input.begin())
        , _begin (input.begin())
        , _end (input.end())
      {}
      position_vector_of_char::position_vector_of_char ( const std::vector<char>::const_iterator &begin
                                                       , const std::vector<char>::const_iterator &end
                                                       )
        : _k (0)
        , _pos (begin)
        , _begin (begin)
        , _end (end)
      {}

      std::string position_vector_of_char::error_message (const std::string& message) const
      {
        std::ostringstream oss;

        oss << "PARSE ERROR [" << eaten() << "]: " << message << std::endl;
        oss << std::string (_begin, _pos) << ' '
            << std::string (_pos, _end) << std::endl;
        oss << std::string (eaten(), ' ') << "^" << std::endl;

        return oss.str();
      }
    }
  }
}
