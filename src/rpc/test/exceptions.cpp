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
#include <util-generic/scoped_boost_asio_io_service_with_threads.hpp>
#include <util-generic/testing/flatten_nested_exceptions.hpp>
#include <util-generic/testing/printer/tuple.hpp>
#include <util-generic/testing/random.hpp>
#include <util-generic/testing/require_exception.hpp>

#include <boost/serialization/list.hpp>
#include <boost/test/unit_test.hpp>

namespace protocol
{
  FHG_RPC_FUNCTION_DESCRIPTION (require_lt_0, void (int));
}

BOOST_AUTO_TEST_CASE (std_exceptions)
{
  fhg::rpc::service_dispatcher service_dispatcher;
  fhg::rpc::service_handler<protocol::require_lt_0> start_service
    ( service_dispatcher
    , [] (int i)
      {
        if (i >= 0)
        {
          throw std::invalid_argument (std::to_string (i) + " >= 0");
        }
      }
    );
  fhg::util::scoped_boost_asio_io_service_with_threads io_service_server (1);
  fhg::rpc::service_tcp_provider const server
    {io_service_server, service_dispatcher};

  fhg::util::scoped_boost_asio_io_service_with_threads io_service_client (1);
  fhg::rpc::remote_tcp_endpoint endpoint
    ( io_service_client
    , fhg::util::connectable_to_address_string (server.local_endpoint())
    );
  fhg::rpc::sync_remote_function<protocol::require_lt_0> require_lt_0 (endpoint);

  require_lt_0 (-1);
  fhg::util::testing::require_exception
    ( [&] { require_lt_0 (1); }
    , std::invalid_argument ("1 >= 0")
    );
}

namespace
{
  struct user_defined_exception
  {
    int dummy;
    user_defined_exception (int d)
      : dummy (d)
    {}
    //! \note Workaround for https://gcc.gnu.org/bugzilla/show_bug.cgi?id=62154
    virtual ~user_defined_exception() = default;
    user_defined_exception (user_defined_exception const&) = default;

    std::string what() const
    {
      return "dummy = " + std::to_string (dummy);
    }

    static boost::optional<std::string> from_exception_ptr
      (std::exception_ptr ex_ptr)
    {
      try
      {
        std::rethrow_exception (ex_ptr);
      }
      catch (user_defined_exception const& ex)
      {
        return std::to_string (ex.dummy);
      }
      catch (...)
      {
        return boost::none;
      }
    }
    [[noreturn]] static void throw_with_nested (std::string serialized)
    {
      std::throw_with_nested (user_defined_exception (std::stoi (serialized)));
    }
    static std::exception_ptr to_exception_ptr (std::string serialized)
    {
      return std::make_exception_ptr
        (user_defined_exception (std::stoi (serialized)));
    }
  };
  struct user_defined_std_runtime_error : std::runtime_error
  {
    int dummy;
    user_defined_std_runtime_error (int d)
      : std::runtime_error ("dummy = " + std::to_string (d))
      , dummy (d)
    {}

    static boost::optional<std::string> from_exception_ptr
      (std::exception_ptr ex_ptr)
    {
      try
      {
        std::rethrow_exception (ex_ptr);
      }
      catch (user_defined_std_runtime_error const& ex)
      {
        return std::to_string (ex.dummy);
      }
      catch (...)
      {
        return boost::none;
      }
    }
    [[noreturn]] static void throw_with_nested (std::string serialized)
    {
      std::throw_with_nested (user_defined_exception (std::stoi (serialized)));
    }
    static std::exception_ptr to_exception_ptr (std::string serialized)
    {
      return std::make_exception_ptr
        (user_defined_std_runtime_error (std::stoi (serialized)));
    }
  };
}

namespace protocol
{
  FHG_RPC_FUNCTION_DESCRIPTION (throw_exception, void (bool, int));
}

BOOST_AUTO_TEST_CASE (user_defined_exceptions)
{
  fhg::rpc::service_dispatcher service_dispatcher
    ( { { "ude"
        , { &user_defined_exception::from_exception_ptr
          , &user_defined_exception::to_exception_ptr
          , &user_defined_exception::throw_with_nested
          }
        }
      , { "udsrte"
        , { &user_defined_std_runtime_error::from_exception_ptr
          , &user_defined_std_runtime_error::to_exception_ptr
          , &user_defined_std_runtime_error::throw_with_nested
          }
        }
      }
    );
  fhg::rpc::service_handler<protocol::throw_exception> start_service
    ( service_dispatcher
    , [] (bool std_runtime_error_based, int dummy)
      {
        if (std_runtime_error_based)
        {
          throw user_defined_std_runtime_error (dummy);
        }
        else
        {
          throw user_defined_exception (dummy);
        }
      }
    );
  fhg::util::scoped_boost_asio_io_service_with_threads io_service_server (1);
  fhg::rpc::service_tcp_provider const server
    {io_service_server, service_dispatcher};

  fhg::util::scoped_boost_asio_io_service_with_threads io_service_client (1);
  fhg::rpc::remote_tcp_endpoint endpoint
    ( io_service_client
    , fhg::util::connectable_to_address_string (server.local_endpoint())
    , { { "ude", { &user_defined_exception::from_exception_ptr
                 , &user_defined_exception::to_exception_ptr
                 , &user_defined_exception::throw_with_nested
                 }
        }
      , { "udsrte", { &user_defined_std_runtime_error::from_exception_ptr
                    , &user_defined_std_runtime_error::to_exception_ptr
                    , &user_defined_std_runtime_error::throw_with_nested
                    }
        }
      }
    );
  fhg::rpc::sync_remote_function<protocol::throw_exception> throw_exception
    (endpoint);

  {
    int const s (fhg::util::testing::random<int>{}());
    fhg::util::testing::require_exception
      ( [&] { throw_exception (false, s); }
      , user_defined_exception (s)
      );
  }
  {
    int const s (fhg::util::testing::random<int>{}());
    fhg::util::testing::require_exception
      ( [&] { throw_exception (true, s); }
      , user_defined_std_runtime_error (s)
      );
  }
}

namespace protocol
{
  FHG_RPC_FUNCTION_DESCRIPTION (throw_udsre_exception, void (int));
}

BOOST_AUTO_TEST_CASE (user_defined_std_exceptions_are_downcast_automatically_server)
{
  fhg::rpc::service_dispatcher service_dispatcher;
  fhg::rpc::service_handler<protocol::throw_udsre_exception> start_service
    ( service_dispatcher
    , [] (int dummy)
      {
        throw user_defined_std_runtime_error (dummy);
      }
    );
  fhg::util::scoped_boost_asio_io_service_with_threads io_service_server (1);
  fhg::rpc::service_tcp_provider const server
    {io_service_server, service_dispatcher};

  fhg::util::scoped_boost_asio_io_service_with_threads io_service_client (1);
  fhg::rpc::remote_tcp_endpoint endpoint
    ( io_service_client
    , fhg::util::connectable_to_address_string (server.local_endpoint())
    );
  fhg::rpc::sync_remote_function<protocol::throw_udsre_exception> throw_exception
    (endpoint);

  {
    int const s (fhg::util::testing::random<int>{}());
    fhg::util::testing::require_exception
      ( [&] { throw_exception (s); }
      , std::runtime_error ("dummy = " + std::to_string (s))
      );
  }
}

BOOST_AUTO_TEST_CASE (user_defined_std_exceptions_are_downcast_automatically_client)
{
  fhg::rpc::service_dispatcher service_dispatcher
    ({{ "udsrte"
      , { &user_defined_std_runtime_error::from_exception_ptr
        , &user_defined_std_runtime_error::to_exception_ptr
        , &user_defined_std_runtime_error::throw_with_nested
        }
      }
     }
    );
  fhg::rpc::service_handler<protocol::throw_udsre_exception> start_service
    ( service_dispatcher
    , [] (int dummy)
      {
        throw user_defined_std_runtime_error (dummy);
      }
    );
  fhg::util::scoped_boost_asio_io_service_with_threads io_service_server (1);
  fhg::rpc::service_tcp_provider const server
    {io_service_server, service_dispatcher};

  fhg::util::scoped_boost_asio_io_service_with_threads io_service_client (1);
  fhg::rpc::remote_tcp_endpoint endpoint
    ( io_service_client
    , fhg::util::connectable_to_address_string (server.local_endpoint())
    );
  fhg::rpc::sync_remote_function<protocol::throw_udsre_exception> throw_exception
    (endpoint);

  {
    int const s (fhg::util::testing::random<int>{}());
    fhg::util::testing::require_exception
      ( [&] { throw_exception (s); }
      , std::runtime_error ("dummy = " + std::to_string (s))
      );
  }
}

namespace protocol
{
  FHG_RPC_FUNCTION_DESCRIPTION (throw_nested_exception, void (bool, int));
}

BOOST_AUTO_TEST_CASE (nested_exceptions)
{
  fhg::rpc::service_dispatcher service_dispatcher
    ({{ "ude"
      , { &user_defined_exception::from_exception_ptr
        , &user_defined_exception::to_exception_ptr
        , &user_defined_exception::throw_with_nested
        }
      }
     }
    );
  fhg::rpc::service_handler<protocol::throw_nested_exception> start_service
    ( service_dispatcher
    , [] (bool std_logic_error, int dummy)
      {
        try
        {
          if (std_logic_error)
          {
            throw std::invalid_argument (std::to_string (dummy));
          }
          else
          {
            throw user_defined_exception (dummy);
          }
        }
        catch (...)
        {
          std::throw_with_nested (std::runtime_error ("failing succeeded"));
        }
      }
    );
  fhg::util::scoped_boost_asio_io_service_with_threads io_service_server (1);
  fhg::rpc::service_tcp_provider const server
    {io_service_server, service_dispatcher};

  fhg::util::scoped_boost_asio_io_service_with_threads io_service_client (1);
  fhg::rpc::remote_tcp_endpoint endpoint
    ( io_service_client
    , fhg::util::connectable_to_address_string (server.local_endpoint())
    , { { "ude", { &user_defined_exception::from_exception_ptr
                 , &user_defined_exception::to_exception_ptr
                 , &user_defined_exception::throw_with_nested
                 }
        }
      }
    );
  fhg::rpc::sync_remote_function<protocol::throw_nested_exception> throw_exception
    (endpoint);

  {
    int const s (fhg::util::testing::random<int>{}());
    try
    {
      throw_exception (false, s);
    }
    catch (std::runtime_error const& ex)
    {
      BOOST_REQUIRE_EQUAL (ex.what(), "failing succeeded");
      try
      {
        std::rethrow_if_nested (ex);
        BOOST_FAIL ("requires top level to be nesting");
      }
      catch (user_defined_exception const& ude)
      {
        BOOST_REQUIRE_EQUAL (ude.dummy, s);
        BOOST_REQUIRE_EQUAL (ude.what(), "dummy = " + std::to_string (s));
      }
    }
  }
  {
    int const s (fhg::util::testing::random<int>{}());
    try
    {
      throw_exception (true, s);
    }
    catch (std::runtime_error const& ex)
    {
      BOOST_REQUIRE_EQUAL (ex.what(), "failing succeeded");
      try
      {
        std::rethrow_if_nested (ex);
        BOOST_FAIL ("requires top level to be nesting");
      }
      catch (std::invalid_argument const& nested)
      {
        BOOST_REQUIRE_EQUAL (nested.what(), std::to_string (s));
      }
    }
  }
}
