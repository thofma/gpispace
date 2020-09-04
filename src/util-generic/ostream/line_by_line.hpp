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

#include <functional>
#include <streambuf>
#include <string>

namespace fhg
{
  namespace util
  {
    namespace ostream
    {
      class line_by_line : public std::streambuf
      {
      public:
        line_by_line (std::function<void (std::string const&)> const& callback)
          : std::streambuf()
          , _callback (callback)
        {}
        ~line_by_line()
        {
          if (!_buffer.empty())
          {
            _callback (_buffer);
          }
        }

        int_type overflow (int_type i)
        {
          auto const c (traits_type::to_char_type (i));

          if (c == '\n')
          {
            _callback (_buffer);

            _buffer.clear();
          }
          else
          {
            _buffer += c;
          }

          return i;
        }

      private:
        std::string _buffer;
        std::function<void (std::string const&)> _callback;
      };
    }
  }
}
