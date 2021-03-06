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

#include <xml/parse/type/connect.hpp>
#include <xml/parse/type/function.hpp>
#include <xml/parse/type/net.fwd.hpp>
#include <xml/parse/type/place_map.hpp>
#include <xml/parse/type/require.hpp>
#include <xml/parse/type/response.hpp>
#include <xml/parse/type/eureka.hpp>
#include <xml/parse/type/struct.hpp>
#include <xml/parse/type/use.hpp>
#include <xml/parse/type/with_position_of_definition.hpp>
#include <xml/parse/util/position.fwd.hpp>

#include <we/type/net.fwd.hpp>

#include <boost/optional.hpp>

namespace xml
{
  namespace parse
  {
    namespace type
    {
      struct transition_type : with_position_of_definition
      {
      public:
        typedef std::string unique_key_type;

        typedef fhg::pnet::util::unique<connect_type> connections_type;
        using responses_type = fhg::pnet::util::unique<response_type>;
        using eurekas_type = fhg::pnet::util::unique<eureka_type>;
        using place_maps_type = fhg::pnet::util::unique<place_map_type>;

        typedef boost::variant <function_type, use_type>
          function_or_use_type;

        transition_type ( const util::position_type&
                        , const function_or_use_type&
                        , const std::string& name
                        , const connections_type& connections
                        , responses_type const&
                        , eurekas_type const&
                        , const place_maps_type& place_map
                        , const structs_type& structs
                        , const conditions_type&
                        , const requirements_type& requirements
                        , const boost::optional<we::priority_type>& priority
                        , const boost::optional<bool>& finline
                        , const we::type::property::type& properties
                        );
        transition_type add_prefix (std::string const&) const;
        transition_type remove_prefix (std::string const&) const;

        const function_or_use_type& function_or_use() const;
        function_or_use_type& function_or_use();

        function_type const& resolved_function() const;

        const std::string& name() const;

        const connections_type& connections() const;
        responses_type const& responses() const;
        eurekas_type const& eurekas() const;
        const place_maps_type& place_map() const;

        // ***************************************************************** //

        const conditions_type& conditions() const;

        // ***************************************************************** //

        void resolve ( const state::type & state
                     , const xml::parse::structure_type_util::forbidden_type & forbidden
                     );
        void resolve ( const xml::parse::structure_type_util::set_type & global
                     , const state::type & state
                     , const xml::parse::structure_type_util::forbidden_type & forbidden
                     );

        // ***************************************************************** //

        void specialize ( const type::type_map_type & map
                        , const type::type_get_type & get
                        , const xml::parse::structure_type_util::set_type & known_structs
                        , state::type & state
                        );

        // ***************************************************************** //

        void type_check (response_type const&, state::type const&) const;
        void type_check (const connect_type&, const state::type&, net_type const& parent) const;
        void type_check (const state::type & state, net_type const& parent) const;
        void type_check (eureka_type const&, state::type const&) const;

        void resolve_function_use_recursive
          (std::unordered_map<std::string, function_type const&> known);
        void resolve_types_recursive
          (std::unordered_map<std::string, pnet::type::signature::signature_type> known);

        const we::type::property::type& properties() const;
        we::type::property::type& properties();

        const unique_key_type& unique_key() const;

      private:
        function_or_use_type _function_or_use;

        std::string const _name;

        //! \note renaming for prefix
        friend struct net_type;
        connections_type _connections;
        responses_type _responses;
        eurekas_type _eurekas;
        place_maps_type _place_map;

        bool is_connect_tp_many (const we::edge::type, const std::string &) const;
        //! \todo All below should be private with accessors.
      public:
        structs_type structs;

      private:
        conditions_type _conditions;

      public:
        requirements_type requirements;

        boost::optional<we::priority_type> priority;
        boost::optional<bool> finline;

      private:
        we::type::property::type _properties;
      };

      // ******************************************************************* //

      void transition_synthesize
        ( transition_type const&
        , const state::type & state
        , we::type::net_type& we_net
        , const place_map_map_type & pids
        );

      // ******************************************************************* //

      namespace dump
      {
        void dump ( ::fhg::util::xml::xmlstream & s
                  , const transition_type & t
                  );
      }
    }
  }
}
