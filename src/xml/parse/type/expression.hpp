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

#include <xml/parse/type/expression.fwd.hpp>

#include <xml/parse/type/with_position_of_definition.hpp>
#include <xml/parse/util/position.hpp>

#include <fhg/util/xml.fwd.hpp>

#include <string>
#include <list>

namespace xml
{
  namespace parse
  {
    namespace type
    {
      //! \todo Move this into class scope.
      typedef std::list<std::string> expressions_type;

      struct expression_type : with_position_of_definition
      {
      public:
        expression_type (const util::position_type&, const expressions_type&);

        std::string expression() const;

      private:
        expressions_type _expressions;
      };

      namespace dump
      {
        void dump ( ::fhg::util::xml::xmlstream & s
                  , const expression_type & e
                  );
      }
    }
  }
}
