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

#include <fhgcom/header.hpp>
#include <fhgcom/message.hpp>

#include <fhg/util/thread/event.hpp>

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/strand.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <boost/variant/variant.hpp>

#include <algorithm>
#include <cstddef>
#include <exception>
#include <functional>
#include <list>
#include <memory>
#include <vector>

namespace boost
{
  namespace asio
  {
    namespace ssl
    {
      class context;
    }
  }
}

namespace fhg
{
  namespace com
  {
    class peer_t;

    using tcp_socket_t = boost::asio::ip::tcp::socket;
    using ssl_stream_t = boost::asio::ssl::stream<tcp_socket_t>;
    using socket_t = boost::variant<std::unique_ptr<tcp_socket_t>, std::unique_ptr<ssl_stream_t>>;

    class handshake_exception : public boost::system::system_error
    {
    public:
      handshake_exception (boost::system::error_code const& ec)
        : boost::system::system_error (ec)
      {}
    };

    class connection_t : private boost::noncopyable
                       , public boost::enable_shared_from_this<connection_t>
    {
    public:
      typedef boost::shared_ptr<connection_t> ptr_t;

      typedef std::function <void (boost::system::error_code const &)> completion_handler_t;

      explicit
      connection_t
        ( boost::asio::io_service & io_service
        , boost::asio::ssl::context* ctx
        , boost::asio::io_service::strand const& strand
        , std::function<void (ptr_t connection, std::unique_ptr<message_t>)> handle_hello_message
        , std::function<void (ptr_t connection, std::unique_ptr<message_t>)> handle_user_data
        , std::function<void (ptr_t connection, const boost::system::error_code&)> handle_error
        , peer_t* peer
        );

      ~connection_t ();

      boost::asio::ip::tcp::socket & socket();

      void async_send (message_t msg, completion_handler_t hdl);

      template <typename SettableSocketOption>
      void set_option(const SettableSocketOption & o)
      {
        socket().set_option (o);
      }

      void start ();
      void stop ();

      const p2p::address_t & local_address () const { return m_local_addr; }
      const p2p::address_t & remote_address () const { return m_remote_addr; }

      void local_address  (const p2p::address_t & a) { m_local_addr = a; }
      void remote_address (const p2p::address_t & a)
      {
        m_remote_addr = a;
      }

      //! Assumes to be called from within strand. Will call back
      //! peer_t::request_handshake_response via strand, never from
      //! within this call stack.
      void request_handshake
        (std::shared_ptr<util::thread::event<std::exception_ptr>> connect_done);
      //! Assumes to be called from within strand. Will call back
      //! peer_t::acknowledge_handshake_response via strand, never
      //! from within this call stack.
      void acknowledge_handshake();

    private:
      struct to_send_t
      {
        to_send_t (message_t msg, completion_handler_t hdl);

        completion_handler_t handler;
        message_t _message;
        std::vector<boost::asio::const_buffer> buffers;
      };

      void start_read ();
      void start_send ();

      void handle_read_header ( const boost::system::error_code & ec
                              , std::size_t bytes_transferred
                              );
      void handle_read_data ( const boost::system::error_code & ec
                            , std::size_t bytes_transferred
                            );

      void handle_write ( const boost::system::error_code & ec );

      peer_t* _peer;

      boost::asio::io_service::strand strand_;
      socket_t socket_;
      tcp_socket_t& _raw_socket;
      std::function<void (ptr_t connection, std::unique_ptr<message_t>)> _handle_hello_message;
      std::function<void (ptr_t connection, std::unique_ptr<message_t>)> _handle_user_data;
      std::function<void (ptr_t connection, const boost::system::error_code&)> _handle_error;
      std::unique_ptr<message_t> in_message_;

      std::list <to_send_t> to_send_;

      p2p::address_t m_local_addr;
      p2p::address_t m_remote_addr;
    };
  }
}
