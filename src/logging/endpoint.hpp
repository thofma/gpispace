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

#include <logging/tcp_endpoint.hpp>
#include <logging/socket_endpoint.hpp>

#include <boost/optional.hpp>
#include <boost/variant.hpp>

#include <stdexcept>
#include <string>
#include <vector>

namespace fhg
{
  namespace logging
  {
    namespace error
    {
      struct default_constructed_endpoint_used_for_non_deserialization
        : std::logic_error
      {
        default_constructed_endpoint_used_for_non_deserialization();
      };
      struct no_possible_matching_endpoint : std::runtime_error
      {
        no_possible_matching_endpoint (std::string);
      };
      struct unexpected_token : std::invalid_argument
      {
        unexpected_token (std::string);
      };
    }

    //! \todo Actually part of fhg::rpc.
    struct endpoint
    {
      endpoint (tcp_endpoint);
      endpoint (socket_endpoint);
      endpoint (tcp_endpoint, socket_endpoint);
      endpoint (std::string combined_string);

      boost::optional<tcp_endpoint> as_tcp;
      boost::optional<socket_endpoint> as_socket;

      std::string to_string() const;

      boost::variant<tcp_endpoint, socket_endpoint> best (std::string host) const;

      endpoint() = default;
      endpoint (endpoint const&) = default;
      endpoint (endpoint&&) = default;
      endpoint& operator= (endpoint const&) = default;
      endpoint& operator= (endpoint&&) = default;
      ~endpoint() = default;

      template<typename Archive>
        void serialize (Archive&, endpoint&, unsigned int);
    };

    void validate
      (boost::any&, std::vector<std::string> const&, endpoint*, int);
  }
}

#include <logging/endpoint.ipp>
