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

#include <xml/parse/type/net.fwd.hpp>

#include <xml/parse/type/function.hpp>
#include <xml/parse/type/place.hpp>
#include <xml/parse/type/place_map.hpp>
#include <xml/parse/type/specialize.hpp>
#include <xml/parse/type/template.hpp>
#include <xml/parse/type/transition.hpp>
#include <xml/parse/type/with_position_of_definition.hpp>
#include <xml/parse/type_map_type.hpp>
#include <xml/parse/util/position.fwd.hpp>

#include <xml/parse/util/unique.hpp>

#include <xml/parse/type/dumps.hpp>

#include <we/type/id.hpp>

namespace xml
{
  namespace parse
  {
    namespace type
    {
      struct net_type : with_position_of_definition
      {
      public:
        using functions_type = fhg::pnet::util::unique<function_type>;
        typedef fhg::pnet::util::unique<place_type> places_type;
        using specializes_type = fhg::pnet::util::unique<specialize_type>;
        using templates_type = fhg::pnet::util::unique<tmpl_type>;
        using transitions_type = fhg::pnet::util::unique<transition_type>;

        net_type ( const util::position_type&
                 , functions_type
                 , places_type
                 , specializes_type
                 , templates_type
                 , transitions_type
                 , structs_type
                 , we::type::property::type
                 );

        const we::type::property::type& properties() const;

        // ***************************************************************** //

        const functions_type& functions() const;
        const places_type& places() const;
        const specializes_type& specializes() const;
        const templates_type& templates() const;
        const transitions_type& transitions() const;
        //! \note find_module_calls needs to modify transitions when diving
        transitions_type& transitions();

        // ***************************************************************** //

        boost::optional<tmpl_type const&>
          get_template (const std::string& name) const;

        // ***************************************************************** //

        void type_map_apply ( const type::type_map_type & outer_map
                            , type::type_map_type & inner_map
                            );

        void specialize ( const type::type_map_type & map
                        , const type::type_get_type & get
                        , const xml::parse::structure_type_util::set_type & known_structs
                        , state::type & state
                        );

        // ***************************************************************** //

        void type_check (const state::type & state) const;

        void resolve_function_use_recursive
          (std::unordered_map<std::string, function_type const&> known);
        void resolve_types_recursive
          (std::unordered_map<std::string, pnet::type::signature::signature_type> known);

        void set_prefix (const std::string & prefix);
        void remove_prefix (const std::string & prefix);

      private:
        functions_type _functions;
        places_type _places;
        specializes_type _specializes;
        templates_type _templates;
        transitions_type _transitions;

        //! \todo Everything below should be private with accessors.
      public:
        structs_type structs;
        bool contains_a_module_call;

      private:
        we::type::property::type _properties;
      };

      // ******************************************************************* //

      std::unordered_map<std::string, we::place_id_type>
      net_synthesize ( we::type::net_type& we_net
                     , const place_map_map_type & place_map_map
                     , const net_type & net
                     , const state::type & state
                     );

      namespace dump
      {
        void dump ( ::fhg::util::xml::xmlstream & s
                  , const net_type & net
                  );
      } // namespace dump
    }
  }
}
