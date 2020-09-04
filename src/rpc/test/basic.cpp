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

#include <rpc/remote_tcp_endpoint.hpp>
#include <rpc/remote_function.hpp>
#include <rpc/service_dispatcher.hpp>
#include <rpc/service_handler.hpp>
#include <rpc/service_tcp_provider.hpp>

#include <util-generic/connectable_to_address_string.hpp>
#include <util-generic/finally.hpp>
#include <util-generic/latch.hpp>
#include <util-generic/scoped_boost_asio_io_service_with_threads.hpp>
#include <util-generic/testing/flatten_nested_exceptions.hpp>
#include <util-generic/testing/printer/future.hpp>
#include <util-generic/testing/printer/tuple.hpp>
#include <util-generic/testing/random.hpp>
#include <util-generic/testing/require_exception.hpp>

#include <boost/serialization/list.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>

//! \todo cross-process test cases?

namespace protocol
{
  FHG_RPC_FUNCTION_DESCRIPTION (ping, int (int));
}

BOOST_AUTO_TEST_CASE (nonexist_throws)
{
  fhg::rpc::service_dispatcher service_dispatcher;
  fhg::util::scoped_boost_asio_io_service_with_threads io_service_server (1);
  fhg::rpc::service_tcp_provider const server
    {io_service_server, service_dispatcher};

  fhg::util::scoped_boost_asio_io_service_with_threads io_service_client (1);
  fhg::rpc::remote_tcp_endpoint client
    ( io_service_client
    , fhg::util::connectable_to_address_string (server.local_endpoint())
    );

  std::future<int> pong
    (fhg::rpc::remote_function<protocol::ping> (client) (0));

  fhg::util::testing::require_exception
    ( [&] { pong.get(); }
    , fhg::rpc::error::unknown_function (typeid (protocol::ping).name())
    );
}

BOOST_AUTO_TEST_CASE (int_ping_client_does_automatic_argument_conversion)
{
  fhg::rpc::service_dispatcher service_dispatcher;
  fhg::rpc::service_handler<protocol::ping> start_service
    ( service_dispatcher
    , [] (int i) { return i + 1; }
    );
  fhg::util::scoped_boost_asio_io_service_with_threads io_service_server (1);
  fhg::rpc::service_tcp_provider const server
    {io_service_server, service_dispatcher};

  fhg::util::scoped_boost_asio_io_service_with_threads io_service_client (1);
  fhg::rpc::remote_tcp_endpoint client
    ( io_service_client
    , fhg::util::connectable_to_address_string (server.local_endpoint())
    );

  //! \note here: mismatch int vs char
  char const s (fhg::util::testing::random<char>{}());
  std::future<int> pong (fhg::rpc::remote_function<protocol::ping> (client) (s));

  BOOST_REQUIRE_EQUAL ( pong.wait_for (std::chrono::seconds (10))
                      , std::future_status::ready
                      );

  BOOST_REQUIRE_EQUAL (pong.get(), s + 1);
}

BOOST_AUTO_TEST_CASE (int_ping)
{
  fhg::rpc::service_dispatcher service_dispatcher;
  fhg::rpc::service_handler<protocol::ping> start_service
    ( service_dispatcher
    , [] (int i) { return i + 1; }
    );
  fhg::util::scoped_boost_asio_io_service_with_threads io_service_server (1);
  fhg::rpc::service_tcp_provider const server
    {io_service_server, service_dispatcher};

  fhg::util::scoped_boost_asio_io_service_with_threads io_service_client (1);
  fhg::rpc::remote_tcp_endpoint client
    ( io_service_client
    , fhg::util::connectable_to_address_string (server.local_endpoint())
    );

  int const s (fhg::util::testing::random<int>{}());
  std::future<int> pong (fhg::rpc::remote_function<protocol::ping> (client) (s));

  BOOST_REQUIRE_EQUAL ( pong.wait_for (std::chrono::seconds (10))
                      , std::future_status::ready
                      );

  BOOST_REQUIRE_EQUAL (pong.get(), s + 1);
}

BOOST_AUTO_TEST_CASE (int_ping_sync)
{
  fhg::rpc::service_dispatcher service_dispatcher;
  fhg::rpc::service_handler<protocol::ping> start_service
    ( service_dispatcher
    , [] (int i) { return i + 1; }
    );
  fhg::util::scoped_boost_asio_io_service_with_threads io_service_server (1);
  fhg::rpc::service_tcp_provider const server
    {io_service_server, service_dispatcher};

  fhg::util::scoped_boost_asio_io_service_with_threads io_service_client (1);
  fhg::rpc::remote_tcp_endpoint client
    ( io_service_client
    , fhg::util::connectable_to_address_string (server.local_endpoint())
    );

  int const s (fhg::util::testing::random<int>{}());
  BOOST_REQUIRE_EQUAL (fhg::rpc::sync_remote_function<protocol::ping> (client) (s), s + 1);
}

namespace
{
  struct user_defined_type
  {
    std::string foo;
    std::list<std::string> bar;
    bool operator== (user_defined_type const& rhs) const
    {
      return std::tie (foo, bar) == std::tie (rhs.foo, rhs.bar);
    }
  };
}
FHG_BOOST_TEST_LOG_VALUE_PRINTER (user_defined_type, os, udt)
{
  os << udt.foo << ", ";
  FHG_BOOST_TEST_PRINT_LOG_VALUE_HELPER (udt.bar);
}
namespace boost
{
  namespace serialization
  {
    template<typename Archive>
      void serialize (Archive& ar, user_defined_type& udt, const unsigned int)
    {
      ar & udt.foo;
      ar & udt.bar;
    }
  }
}

namespace protocol
{
  FHG_RPC_FUNCTION_DESCRIPTION (udt, user_defined_type (user_defined_type));
}

BOOST_AUTO_TEST_CASE (user_defined_type_echo)
{
  fhg::rpc::service_dispatcher service_dispatcher;
  fhg::rpc::service_handler<protocol::udt> start_service
    ( service_dispatcher
    , [] (user_defined_type x) { return x; }
    );
  fhg::util::scoped_boost_asio_io_service_with_threads io_service_server (1);
  fhg::rpc::service_tcp_provider const server
    {io_service_server, service_dispatcher};

  fhg::util::scoped_boost_asio_io_service_with_threads io_service_client (1);
  fhg::rpc::remote_tcp_endpoint client
    ( io_service_client
    , fhg::util::connectable_to_address_string (server.local_endpoint())
    );
  fhg::rpc::sync_remote_function<protocol::udt> echo (client);

  user_defined_type const udt {"baz", {"brunz", "buu"}};
  BOOST_REQUIRE_EQUAL (echo (udt), udt);
}

BOOST_AUTO_TEST_CASE (slow_parallel_pings)
{
  fhg::util::scoped_boost_asio_io_service_with_threads io_service (10);

  fhg::util::latch function_calls_placed (1);

  fhg::rpc::service_dispatcher service_dispatcher;
  fhg::rpc::service_handler<protocol::ping> start_service
    ( service_dispatcher
    , [&function_calls_placed] (int i)
      {
        // Blocking. Fine: Single-threaded io_service only used by server.
        function_calls_placed.wait();
        return i + 1;
      }
    );
  fhg::util::scoped_boost_asio_io_service_with_threads io_service_server (1);
  fhg::rpc::service_tcp_provider const server
    {io_service_server, service_dispatcher};

  fhg::rpc::remote_tcp_endpoint client
    ( io_service
    , fhg::util::connectable_to_address_string (server.local_endpoint())
    );

  fhg::rpc::remote_function<protocol::ping> ping (client);

  std::vector<std::future<int>> futures;
  for (int i (0); i < 10000; ++i)
  {
    futures.emplace_back (ping (i));
  }

  function_calls_placed.count_down();

  int i (0);
  for (auto& future : futures)
  {
    BOOST_REQUIRE_EQUAL (future.get(), ++i);
  }
}

BOOST_AUTO_TEST_CASE (deferred_dispatcher)
{
  fhg::util::scoped_boost_asio_io_service_with_threads io_service (1);

  fhg::rpc::service_tcp_provider_with_deferred_dispatcher server
    {io_service};

  fhg::util::scoped_boost_asio_io_service_with_threads io_service_client (1);
  fhg::rpc::remote_tcp_endpoint client
    ( io_service_client
    , fhg::util::connectable_to_address_string (server.local_endpoint())
    );

  int const s (fhg::util::testing::random<int>{}());
  std::future<int> pong (fhg::rpc::remote_function<protocol::ping> (client) (s));

  BOOST_REQUIRE_EQUAL ( pong.wait_for (std::chrono::seconds (1))
                      , std::future_status::timeout
                      );

  fhg::rpc::service_dispatcher service_dispatcher;
  fhg::rpc::service_handler<protocol::ping> start_service
    ( service_dispatcher
    , [] (int i) { return i + 1; }
    );

  server.set_dispatcher (&service_dispatcher);

  BOOST_REQUIRE_EQUAL (pong.get(), s + 1);
}

BOOST_AUTO_TEST_CASE
  (uniqueness_of_function_name_is_ensured_by_service_dispatcher)
{
  fhg::rpc::service_dispatcher service_dispatcher;
  fhg::rpc::service_handler<protocol::ping> start_service
    ( service_dispatcher
    , [] (int i) { return i + 1; }
    );

  fhg::util::testing::require_exception
    ( [&service_dispatcher]
      {
        fhg::rpc::service_handler<protocol::ping> duplicate
          ( service_dispatcher
          , [] (int i) { return i + 1; }
          );
      }
    , fhg::rpc::error::duplicate_function (typeid (protocol::ping).name())
    );
}

BOOST_AUTO_TEST_CASE (dispatcher_can_be_extended_after_provider_startup)
{
  fhg::rpc::service_dispatcher service_dispatcher;
  fhg::util::scoped_boost_asio_io_service_with_threads io_service_server (1);
  fhg::rpc::service_tcp_provider const server
    {io_service_server, service_dispatcher};

  fhg::util::scoped_boost_asio_io_service_with_threads io_service_client (1);
  fhg::rpc::remote_tcp_endpoint client
    ( io_service_client
    , fhg::util::connectable_to_address_string (server.local_endpoint())
    );

  {
    std::future<int> pong
      (fhg::rpc::remote_function<protocol::ping> (client) (0));

    fhg::util::testing::require_exception
      ( [&] { pong.get(); }
      , fhg::rpc::error::unknown_function (typeid (protocol::ping).name())
      );
  }

  {
    fhg::rpc::service_handler<protocol::ping> start_service
      ( service_dispatcher
      , [] (int i) { return i + 1; }
      );

    int const s (fhg::util::testing::random<int>{}());

    BOOST_REQUIRE_EQUAL
      (fhg::rpc::sync_remote_function<protocol::ping> {client} (s), s + 1);
  }

  {
    std::future<int> pong
      (fhg::rpc::remote_function<protocol::ping> (client) (0));

    fhg::util::testing::require_exception
      ( [&] { pong.get(); }
      , fhg::rpc::error::unknown_function (typeid (protocol::ping).name())
      );
  }
}

namespace protocol
{
  FHG_RPC_FUNCTION_DESCRIPTION (wait, void());
}

//! \note issue #15
BOOST_DATA_TEST_CASE
  ( provider_ensures_ongoing_calls_are_finished_before_unwinding
  , std::vector<int> ({1, 2})
  , number_of_server_threads
  )
{
  fhg::util::latch block_call (1);
  fhg::util::latch server_set_up (1);
  fhg::util::latch provider_will_destruct (1);
  fhg::util::latch server_should_tear_down (1);
  bool service_tcp_provider_destructed = false;

  std::pair<std::string, unsigned short> address;

  auto server_finished
    ( std::async
        ( std::launch::async
        , [&]
          {
            fhg::rpc::service_dispatcher service_dispatcher;
            fhg::rpc::service_handler<protocol::wait> service_handler
              ( service_dispatcher
              , [&]
                {
                  server_should_tear_down.count_down();
                  // Blocking. Fine: single-threaded io_service only
                  // used by server.
                  block_call.wait();
                }
              );

            fhg::util::scoped_boost_asio_io_service_with_threads
              io_service_server (number_of_server_threads);
            FHG_UTIL_FINALLY ([&]{ service_tcp_provider_destructed = true; });
            fhg::rpc::service_tcp_provider const server
              {io_service_server, service_dispatcher};
            provider_will_destruct.count_down();

            address
              = fhg::util::connectable_to_address_string (server.local_endpoint());

            server_set_up.count_down();
            server_should_tear_down.wait();
          }
        )
    );

  server_set_up.wait();

  fhg::util::scoped_boost_asio_io_service_with_threads io_service_client (1);
  fhg::rpc::remote_tcp_endpoint client (io_service_client, address);

  auto call_returned
    ( std::async
        ( std::launch::async
        , [&]
          {
            fhg::rpc::sync_remote_function<protocol::wait> {client}();
          }
        )
    );

  provider_will_destruct.wait();

  BOOST_REQUIRE_EQUAL
    ( server_finished.wait_for (std::chrono::seconds (2))
    , std::future_status::timeout
    );
  BOOST_REQUIRE_EQUAL (service_tcp_provider_destructed, false);

  block_call.count_down();

  server_finished.get();
  BOOST_REQUIRE_EQUAL (service_tcp_provider_destructed, true);
  call_returned.get();
}
