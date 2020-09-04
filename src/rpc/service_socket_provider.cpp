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

#include <rpc/detail/service_stream_provider.ipp>
#include <rpc/service_socket_provider.hpp>

namespace fhg
{
  namespace rpc
  {
    namespace detail
    {
      template struct service_stream_provider
        <boost::asio::local::stream_protocol, service_socket_provider_traits>;
      template struct service_stream_provider_with_deferred_start
        <boost::asio::local::stream_protocol, service_socket_provider_traits>;
      template struct service_stream_provider_with_deferred_dispatcher
        <boost::asio::local::stream_protocol, service_socket_provider_traits>;
    }
  }
}
