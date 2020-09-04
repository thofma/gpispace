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

#include <boost/test/unit_test.hpp>

#include <we/type/activity.hpp>
#include <we/type/net.hpp>
#include <we/type/value/poke.hpp>

#include <boost/range/adaptor/map.hpp>

BOOST_AUTO_TEST_CASE (create_and_execute_cross_product)
{
  we::type::net_type net;

  we::place_id_type const pid_vid
    (net.add_place (place::type ("vid", std::string ("unsigned long"), boost::none)));

  pnet::type::signature::structure_type sig_store_fields;

  sig_store_fields.push_back
    (std::make_pair (std::string ("bid"), std::string ("unsigned long")));
  sig_store_fields.push_back
    (std::make_pair (std::string ("seen"), std::string ("bitset")));

  pnet::type::signature::structured_type const sig_store
    (std::make_pair (std::string ("store"), sig_store_fields));

  we::place_id_type const pid_store
    (net.add_place (place::type ("store", sig_store, boost::none)));

  we::type::transition_t trans_inner
    ( "trans_inner"
    , we::type::expression_t
      ( "${store.seen} := bitset_insert (${store.seen}, ${vid});"
        "${store.bid}  := ${store.bid}                         ;"
        "${pair.bid}   := ${store.bid}                         ;"
        "${pair.vid}   := ${vid}                               "
      )
    , we::type::expression_t ("!bitset_is_element (${store.seen}, ${vid})")
    , we::type::property::type()
    , we::priority_type()
    );

  pnet::type::signature::structure_type sig_pair_fields;

  sig_pair_fields.push_back
    (std::make_pair (std::string ("bid"), std::string ("unsigned long")));
  sig_pair_fields.push_back
    (std::make_pair (std::string ("vid"), std::string ("unsigned long")));

  pnet::type::signature::structured_type const sig_pair
    (std::make_pair (std::string ("pair"), sig_pair_fields));

  we::place_id_type const pid_pair
    (net.add_place (place::type("pair", sig_pair, boost::none)));

  we::port_id_type const port_id_vid
    ( trans_inner.add_port
        (we::type::port_t ("vid", we::type::PORT_IN, std::string("unsigned long")))
    );
  we::port_id_type const port_id_store_out
    ( trans_inner.add_port
        (we::type::port_t ("store", we::type::PORT_OUT, sig_store))
    );
  we::port_id_type const port_id_store_in
    ( trans_inner.add_port
        (we::type::port_t ("store", we::type::PORT_IN, sig_store))
    );
  we::port_id_type const& port_id_pair
    ( trans_inner.add_port
        (we::type::port_t ("pair", we::type::PORT_OUT, sig_pair))
    );

  we::transition_id_type const tid (net.add_transition (trans_inner));

  {
    we::type::property::type empty;

    net.add_connection (we::edge::PT, tid, pid_store, port_id_store_in, empty);
    net.add_connection (we::edge::TP, tid, pid_store, port_id_store_out, empty);
    net.add_connection (we::edge::PT_READ, tid, pid_vid, port_id_vid, empty);
    net.add_connection (we::edge::TP, tid, pid_pair, port_id_pair, empty);
  }

  net.put_value (pid_vid, 0UL);
  net.put_value (pid_vid, 1UL);

  {
    pnet::type::value::structured_type m;

    m.push_back (std::make_pair (std::string ("bid"), 0UL));
    m.push_back (std::make_pair (std::string ("seen"), bitsetofint::type(0)));

    net.put_value (pid_store, m);
  }

  {
    pnet::type::value::structured_type m;

    m.push_back (std::make_pair (std::string ("bid"), 1UL));
    m.push_back (std::make_pair (std::string ("seen"), bitsetofint::type(0)));

    net.put_value (pid_store, m);
  }

  we::type::transition_t tnet ( "tnet"
                              , net
                              , boost::none
                              , we::type::property::type()
                              , we::priority_type()
                              );
  tnet.add_port
    (we::type::port_t ("vid", we::type::PORT_IN, std::string ("unsigned long"), pid_vid));
  tnet.add_port
    (we::type::port_t ("store", we::type::PORT_IN, sig_store, pid_store));
  we::port_id_type const port_id_store_out_net
    (tnet.add_port
      (we::type::port_t ("store", we::type::PORT_OUT, sig_store, pid_store))
    );
  we::port_id_type const port_id_pair_net
    (tnet.add_port
      (we::type::port_t ("pair", we::type::PORT_OUT, sig_pair, pid_pair))
    );

  std::mt19937 engine;

    while ( net.fire_expressions_and_extract_activity_random
              ( engine
              , [] (pnet::type::value::value_type const&, pnet::type::value::value_type const&)
                {
                  throw std::logic_error ("got unexpected workflow_response");
                }
              )
          )
    {
      BOOST_FAIL ("BUMMER: no sub activity shall be created");
    }

  std::unordered_map
    < we::port_id_type
    , std::list<pnet::type::value::value_type>
    > values_by_port_id;

  for ( auto const& token
      : net.get_token (pid_store) | boost::adaptors::map_values
      )
  {
    values_by_port_id[port_id_store_out_net].push_back (token);
  }
  for ( auto const& token
      : net.get_token (pid_pair) | boost::adaptors::map_values
      )
  {
    values_by_port_id[port_id_pair_net].push_back (token);
  }

  BOOST_REQUIRE_EQUAL (values_by_port_id.size(), 2);
  BOOST_REQUIRE_EQUAL (values_by_port_id.at (port_id_store_out_net).size(), 2);
  BOOST_REQUIRE_EQUAL (values_by_port_id.at (port_id_pair_net).size(), 4);

  for (unsigned long bid (0); bid < 2; ++bid)
  {
    bitsetofint::type bs;

    for (unsigned long vid (0); vid < 2; ++vid)
    {
      pnet::type::value::value_type s;
      pnet::type::value::poke ("bid", s, bid);
      pnet::type::value::poke ("vid", s, vid);

      BOOST_REQUIRE
        ( std::find ( values_by_port_id.at (port_id_pair_net).begin()
                    , values_by_port_id.at (port_id_pair_net).end()
                    , s
                    )
        != values_by_port_id.at (port_id_pair_net).end()
        );

      bs.ins (vid);
    }

    pnet::type::value::value_type p;
    pnet::type::value::poke ("bid", p, bid);
    pnet::type::value::poke ("seen", p, bs);

    BOOST_REQUIRE
      ( std::find ( values_by_port_id.at (port_id_store_out_net).begin()
                  , values_by_port_id.at (port_id_store_out_net).end()
                  , p
                  )
      != values_by_port_id.at (port_id_store_out_net).end()
      );
  }
}
