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

#include <rpc/locked_with_info_file.hpp>
#include <rpc/remote_function.hpp>
#include <rpc/service_dispatcher.hpp>
#include <rpc/service_handler.hpp>

#include <util-generic/cxx14/make_unique.hpp>
#include <util-generic/read_file.hpp>
#include <util-generic/scoped_boost_asio_io_service_with_threads.hpp>
#include <util-generic/temporary_file.hpp>
#include <util-generic/temporary_path.hpp>
#include <util-generic/testing/flatten_nested_exceptions.hpp>
#include <util-generic/testing/random.hpp>
#include <util-generic/testing/require_exception.hpp>

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/thread/scoped_thread.hpp>

#include <future>
#include <fstream>

BOOST_AUTO_TEST_CASE (two_server_on_same_lock_is_impossible)
{
  fhg::rpc::service_dispatcher service_dispatcher;

  boost::filesystem::path const path
    {boost::filesystem::absolute (boost::filesystem::unique_path())};

  fhg::util::filesystem_lock_directory const lock (path);

  fhg::util::scoped_boost_asio_io_service_with_threads io_service (1);
  fhg::rpc::locked_with_info_file::server const server
    {io_service, service_dispatcher, lock};

  fhg::util::testing::require_exception
    ( [&io_service, &service_dispatcher, &lock]
      {
        fhg::rpc::locked_with_info_file::server
          {io_service, service_dispatcher, lock};
      }
    , fhg::util::testing::make_nested
        ( fhg::rpc::locked_with_info_file::error::failed_to_create_server (path)
        , std::logic_error
            ( ( boost::format ("Temporary file %1% already exists.")
              % (path / "server")
              ).str()
            )
        )
    );
}

BOOST_AUTO_TEST_CASE (create_client_on_nonexisting_path_fails)
{
  boost::filesystem::path const path {boost::filesystem::unique_path()};

  fhg::util::testing::require_exception
    ( [&path]
      {
        fhg::util::scoped_boost_asio_io_service_with_threads io_service (1);
        fhg::rpc::locked_with_info_file::client {io_service, path};
      }
    , fhg::util::testing::make_nested
        ( fhg::rpc::locked_with_info_file::error::failed_to_create_client (path)
        , std::runtime_error
            ( ( boost::format ("could not open %1%")
              % (path / "server")
              ).str()
            )
        )
    );
}

BOOST_AUTO_TEST_CASE (create_client_on_bad_description_fails)
{
  fhg::util::temporary_path const path {boost::filesystem::unique_path()};
  fhg::util::scoped_file_with_content const file
    {static_cast<boost::filesystem::path> (path) / "server", ""};

  fhg::util::testing::require_exception
    ( [&path]
      {
        fhg::util::scoped_boost_asio_io_service_with_threads io_service (1);
        fhg::rpc::locked_with_info_file::client
          {io_service, static_cast<boost::filesystem::path> (path)};
      }
    , fhg::util::testing::make_nested
        ( fhg::rpc::locked_with_info_file::error::failed_to_create_client (path)
        , fhg::rpc::locked_with_info_file::error::bad_server_description()
        )
    );
}

BOOST_AUTO_TEST_CASE (create_client_on_outdated_description_fails)
{
  fhg::util::scoped_boost_asio_io_service_with_threads io_service (1);

  fhg::rpc::service_dispatcher service_dispatcher;

  boost::filesystem::path const server_path {boost::filesystem::unique_path()};

  fhg::util::filesystem_lock_directory const lock (server_path);

  std::unique_ptr<fhg::rpc::locked_with_info_file::server> server
    (fhg::util::cxx14::make_unique<fhg::rpc::locked_with_info_file::server>
      (io_service, service_dispatcher, lock)
    );

  fhg::util::temporary_path const copy_path {boost::filesystem::unique_path()};
  fhg::util::scoped_file_with_content const copy_server_file
    { static_cast<boost::filesystem::path> (copy_path) / "server"
    , fhg::util::read_file (server_path / "server")
    };

  server.reset();

  fhg::util::testing::require_exception
    ( [&io_service, &copy_path]
      {
        fhg::rpc::locked_with_info_file::client client {io_service, copy_path};
      }
    , fhg::rpc::locked_with_info_file::error::failed_to_create_client (copy_path)
    );
}

namespace protocol
{
  FHG_RPC_FUNCTION_DESCRIPTION (ping, int (int));
}

BOOST_AUTO_TEST_CASE (client_can_communicate_with_server)
{
  fhg::rpc::service_dispatcher service_dispatcher;
  fhg::rpc::service_handler<protocol::ping> const handler_ping
    ( service_dispatcher
    , [] (int i) { return i + 1; }
    );

  boost::filesystem::path const path {boost::filesystem::unique_path()};

  fhg::util::filesystem_lock_directory const lock (path);

  fhg::util::scoped_boost_asio_io_service_with_threads io_service_server (1);
  fhg::rpc::locked_with_info_file::server const server
    {io_service_server, service_dispatcher, lock};

  fhg::util::scoped_boost_asio_io_service_with_threads io_service_client (1);
  fhg::rpc::locked_with_info_file::client client {io_service_client, path};

  int const s {fhg::util::testing::random<int>()()};

  BOOST_REQUIRE_EQUAL
    (s + 1, fhg::rpc::sync_remote_function<protocol::ping> {client} (s));
}
