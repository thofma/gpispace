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

#include <we/expr/eval/context.hpp>
#include <we/type/expression.hpp>
#include <we/type/id.hpp>
#include <we/type/module_call.hpp>
#include <we/type/net.fwd.hpp>
#include <we/type/port.hpp>
#include <we/type/property.hpp>
#include <we/type/requirement.hpp>
#include <we/type/value.hpp>
#include <we/type/eureka.hpp>

#include <boost/optional.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/variant.hpp>
#include <boost/variant.hpp>
#include <boost/variant/recursive_wrapper.hpp>

#include <unordered_map>
#include <unordered_set>

namespace we
{
  namespace type
  {
    using preference_t = std::string;
    using multi_module_call_t = std::unordered_map < preference_t
                                                   , module_call_t
                                                   >;

    struct transition_t
    {
    private:
      typedef boost::variant< module_call_t
                            , multi_module_call_t
                            , expression_t
                            , boost::recursive_wrapper<we::type::net_type>
                            > data_type;

      typedef std::pair< we::place_id_type
                       , we::type::property::type
                       > pid_with_prop_t;

    public:
      typedef std::pair< we::port_id_type
                       , we::type::property::type
                       > port_id_with_prop_t;
      typedef std::unordered_map<we::port_id_type, port_t> port_map_t;

      transition_t();

      template <typename Type>
      transition_t ( const std::string& name
                   , Type const& typ
                   , boost::optional<expression_t> const& _condition
                   , const we::type::property::type& prop
                   , we::priority_type priority
                   , boost::optional<eureka_id_type> const&
                     _eureka_id = boost::none
                   , const std::list<we::type::preference_t>&
                     preferences = {}
                   )
        try
          :  name_  (name)
          ,  data_  (typ)
              //!  \todo  better  check  user  input  earlier!?
          ,  condition_
              (  (!_condition  ||  _condition.get().ast().is_const_true())
              ?  boost::none  :  _condition
              )
          ,  _ports_input()
          ,  _ports_output()
          ,  _ports_tunnel()
          ,  port_id_counter_  (0)
          ,  prop_(prop)
          ,  _requirements()
          ,  _preferences  (preferences)
          ,  _priority  (priority)
          ,  eureka_id_ (_eureka_id)
      {
        if (preferences.size() && data_.type() != typeid (multi_module_call_t))
        {
          throw std::runtime_error
            ("preferences defined without multiple modules with target");
        }
      }
      catch (...)
      {
        std::throw_with_nested
          (std::runtime_error ("Failed to create transition '" + name + "'"));
      }

      const std::string& name() const;

      const data_type& data() const;
      we::type::net_type& mutable_net();

      boost::optional<const expression_t&> expression() const;
      boost::optional<const we::type::net_type&> net() const;
      boost::optional<const module_call_t&> module_call() const;

      boost::optional<expression_t> const& condition() const;
      boost::optional<eureka_id_type> const& eureka_id() const;

      we::port_id_type add_port (port_t const&);

      we::port_id_type input_port_by_name (const std::string&) const;
      const we::port_id_type& output_port_by_name (const std::string&) const;

      port_map_t const& ports_input() const;
      port_map_t const& ports_output() const;
      port_map_t const& ports_tunnel() const;

      const we::type::property::type& prop() const;

      std::list<we::type::requirement_t> const& requirements() const;
      std::list<we::type::preference_t> const& preferences() const;
      void add_requirement (we::type::requirement_t const&);

      we::priority_type priority() const;

      void set_property ( property::path_type const& path
                        , property::value_type const& value
                        )
      {
        prop_.set (path, value);
      }

    private:
      std::string name_;
      data_type data_;
      boost::optional<expression_t> condition_;

      port_map_t _ports_input;
      port_map_t _ports_output;
      port_map_t _ports_tunnel;
      we::port_id_type port_id_counter_;

      we::type::property::type prop_;

      std::list<we::type::requirement_t> _requirements;
      std::list<we::type::preference_t> _preferences;
      we::priority_type _priority;

      boost::optional<eureka_id_type> eureka_id_;

      friend class boost::serialization::access;
      template <typename Archive>
      void serialize(Archive & ar, const unsigned int)
      {
        ar & BOOST_SERIALIZATION_NVP(name_);
        ar & BOOST_SERIALIZATION_NVP(data_);
        ar & BOOST_SERIALIZATION_NVP(condition_);
        ar & BOOST_SERIALIZATION_NVP(_ports_input);
        ar & BOOST_SERIALIZATION_NVP(_ports_output);
        ar & BOOST_SERIALIZATION_NVP(_ports_tunnel);
        ar & BOOST_SERIALIZATION_NVP(port_id_counter_);
        ar & BOOST_SERIALIZATION_NVP(prop_);
        ar & BOOST_SERIALIZATION_NVP(_requirements);
        ar & BOOST_SERIALIZATION_NVP(_preferences);
        ar & BOOST_SERIALIZATION_NVP(_priority);
        ar & BOOST_SERIALIZATION_NVP(eureka_id_);
      }
    };
  }
}
