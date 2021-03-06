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

#include <xml/parse/type/connect.fwd.hpp>

#include <xml/parse/type/with_position_of_definition.hpp>
#include <xml/parse/util/position.fwd.hpp>

#include <util-generic/hash/std/pair.hpp>
#include <fhg/util/xml.fwd.hpp>

#include <we/type/net.hpp>
#include <we/type/property.hpp>

#include <string>
#include <tuple>

#include <boost/optional.hpp>

namespace xml
{
  namespace parse
  {
    namespace type
    {
      struct connect_type : with_position_of_definition
      {
      public:
        //! \note          place,       port,        PT||PT_READ
        typedef std::tuple<std::string, std::string, bool> unique_key_type;

        connect_type ( const util::position_type&
                     , const std::string& place
                     , const std::string& port
                     , const ::we::edge::type& direction
                     , const we::type::property::type& properties
                     = we::type::property::type()
                     );

        const std::string& place() const;
        const std::string& port() const;

        const ::we::edge::type& direction() const;

        connect_type with_place (std::string const& new_place) const;

        const we::type::property::type& properties() const;

        unique_key_type unique_key() const;

      private:
        std::string const _place;
        std::string const _port;

        ::we::edge::type const _direction;

        we::type::property::type _properties;
      };

      namespace dump
      {
        void dump (::fhg::util::xml::xmlstream&, const connect_type&);
      }
    }
  }
}
