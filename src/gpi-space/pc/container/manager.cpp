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

#include <gpi-space/pc/container/manager.hpp>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/variant/static_visitor.hpp>

#include <util-generic/nest_exceptions.hpp>
#include <util-generic/print_exception.hpp>
#include <util-generic/syscall.hpp>

#include <gpi-space/pc/memory/shm_area.hpp>
#include <gpi-space/pc/proto/message.hpp>
#include <gpi-space/pc/url.hpp>
#include <gpi-space/pc/url_io.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <stdexcept>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

namespace gpi
{
  namespace pc
  {
    namespace container
    {
      manager_t::~manager_t ()
      {
        try
        {
          if (m_socket >= 0)
          {
            m_stopping = true;

            safe_unlink (m_path);
            close_socket (m_socket);

            _listener_thread.join();

            m_socket = -1;
          }
        }
        catch (...)
        {
          _logger.emit ( "error within ~manager_t: "
                       + fhg::util::current_exception_printer().string()
                       , fhg::logging::legacy::category_level_error
                       );
        }

        std::lock_guard<std::mutex> const _ (_mutex_processes);
        for ( std::pair<gpi::pc::type::process_id_t const, std::thread>& process
            : m_processes
            )
        {
          process.second.join();

          _logger.emit ( "process container " + std::to_string (process.first)
                       + " detached"
                       , fhg::logging::legacy::category_level_info
                       );
        }

        _memory_manager.clear();
      }

      void manager_t::close_socket (const int fd)
      {
        fhg::util::syscall::shutdown (fd, SHUT_RDWR);
        fhg::util::syscall::close (fd);
      }

      void manager_t::safe_unlink(std::string const & path)
      {
        struct stat st;

        try
        {
          fhg::util::syscall::stat (path.c_str(), &st);
        }
        catch (boost::system::system_error const&)
        {
          return;
        }

        if (!S_ISSOCK(st.st_mode))
        {
          throw std::runtime_error ("not a socket");
        }

        fhg::util::syscall::unlink (path.c_str());
      }

      void manager_t::listener_thread_main()
      {
        int cfd;
        struct sockaddr_un peer_addr;
        socklen_t peer_addr_size;

        for (;;)
        {
          peer_addr_size = sizeof(struct sockaddr_un);
          try
          {
            cfd = fhg::util::syscall::accept ( m_socket
                                             , (struct sockaddr*)&peer_addr
                                             , &peer_addr_size
                                             );
          }
          catch (boost::system::system_error const& se)
          {
            if (m_stopping)
            {
              break;
            }
            else
            {
              continue;
            }
          }

            gpi::pc::type::process_id_t const id (++m_process_counter);

            {
              std::lock_guard<std::mutex> const _ (_mutex_processes);

              m_processes.emplace
                ( id
                , std::thread
                    (&manager_t::process_communication_thread, this, id, cfd)
                );
            }

          _logger.emit ( "process container " + std::to_string (id)
                       + " attached"
                       , fhg::logging::legacy::category_level_info
                       );
        }
      }

      namespace
      {
        struct handle_message_t : public boost::static_visitor<gpi::pc::proto::message_t>
        {
          handle_message_t ( fhg::logging::stream_emitter& logger
                           , gpi::pc::type::process_id_t const& proc_id
                           , memory::manager_t& memory_manager
                           , global::topology_t& topology
                           )
            : _logger (logger)
            , m_proc_id (proc_id)
            , _memory_manager (memory_manager)
            , _topology (topology)
          {}

          /**********************************************/
          /***     M E M O R Y   R E L A T E D        ***/
          /**********************************************/

          gpi::pc::proto::message_t
            operator () (const gpi::pc::proto::memory::alloc_t & alloc) const
          {
            gpi::pc::proto::memory::alloc_reply_t rpl;
            rpl.handle = _memory_manager.alloc
              ( m_proc_id
              , alloc.segment, alloc.size, alloc.name, alloc.flags
              );
            return gpi::pc::proto::memory::message_t (rpl);
          }

          gpi::pc::proto::message_t
            operator () (const gpi::pc::proto::memory::free_t & free) const
          {
            _memory_manager.free (free.handle);
            return gpi::pc::proto::error::error_t
              (gpi::pc::proto::error::success, "success");
          }

          gpi::pc::proto::message_t
            operator () (const gpi::pc::proto::memory::memcpy_t & cpy) const
          {
            gpi::pc::type::validate (cpy.dst.handle);
            gpi::pc::type::validate (cpy.src.handle);
            gpi::pc::proto::memory::memcpy_reply_t rpl;
            rpl.memcpy_id = _memory_manager.memcpy (cpy.dst, cpy.src, cpy.size);
            return gpi::pc::proto::memory::message_t (rpl);
          }

          gpi::pc::proto::message_t
            operator () (const gpi::pc::proto::memory::wait_t & w) const
          {
            try
            {
              _memory_manager.wait (w.memcpy_id);
            }
            catch (...)
            {
              return proto::memory::message_t
                (proto::memory::wait_reply_t (std::current_exception()));
            }

            return proto::memory::message_t (proto::memory::wait_reply_t());
          }

          gpi::pc::proto::message_t
            operator () (const gpi::pc::proto::memory::info_t & info) const
          {
            gpi::pc::proto::memory::info_reply_t rpl;
            rpl.descriptor = _memory_manager.info (info.handle);
            return gpi::pc::proto::memory::message_t (rpl);
          }

          gpi::pc::proto::message_t
            operator() (const gpi::pc::proto::memory::get_transfer_costs_t& request) const
          {
            return gpi::pc::proto::memory::message_t
              (gpi::pc::proto::memory::transfer_costs_t (_memory_manager.get_transfer_costs (request.transfers)));
          }

          /**********************************************/
          /***     S E G M E N T   R E L A T E D      ***/
          /**********************************************/

          gpi::pc::proto::message_t
            operator () (const gpi::pc::proto::segment::register_t & register_segment) const
          {
            memory::area_ptr_t area
              ( new memory::shm_area_t ( _logger
                                       , m_proc_id
                                       , register_segment.name
                                       , register_segment.size
                                       , _memory_manager.handle_generator()
                                       )
              );

            gpi::pc::proto::segment::register_reply_t rpl;
            rpl.id =
              _memory_manager.register_memory (m_proc_id, area);

            return gpi::pc::proto::segment::message_t (rpl);
          }

          gpi::pc::proto::message_t
            operator () (const gpi::pc::proto::segment::unregister_t & unregister_segment) const
          {
            _memory_manager.unregister_memory
              (m_proc_id, unregister_segment.id);
            return gpi::pc::proto::error::error_t
              (gpi::pc::proto::error::success, "success");
          }

          gpi::pc::proto::message_t
            operator () (const gpi::pc::proto::segment::add_memory_t & add_mem) const
          try
          {
            return proto::segment::message_t
              ( proto::segment::add_reply_t
                  ( _memory_manager.add_memory
                      (m_proc_id, add_mem.url, _topology)
                  )
              );
          }
          catch (...)
          {
            return proto::segment::message_t
              (proto::segment::add_reply_t (std::current_exception()));
          }

          gpi::pc::proto::message_t
            operator () (const gpi::pc::proto::segment::del_memory_t & del_mem) const
          {
            _memory_manager.del_memory (m_proc_id, del_mem.id, _topology);
            return
              gpi::pc::proto::error::error_t (gpi::pc::proto::error::success);
          }

          /**********************************************/
          /***     C O N T R O L   R E L A T E D      ***/
          /**********************************************/

          gpi::pc::proto::message_t
          operator () (const gpi::pc::proto::segment::message_t & m) const
          {
            return boost::apply_visitor (*this, m);
          }

          gpi::pc::proto::message_t
          operator () (const gpi::pc::proto::memory::message_t & m) const
          {
            return boost::apply_visitor (*this, m);
          }

          /*** Catch all other messages ***/

          template <typename T>
            gpi::pc::proto::message_t
            operator () (T const &) const
          {
            return gpi::pc::proto::error::error_t
              (gpi::pc::proto::error::bad_request,  "invalid input message");
          }

        private:
          fhg::logging::stream_emitter& _logger;
          gpi::pc::type::process_id_t const& m_proc_id;
          memory::manager_t& _memory_manager;
          global::topology_t& _topology;
        };

        gpi::pc::proto::message_t handle_message
          ( fhg::logging::stream_emitter& logger
          , gpi::pc::type::process_id_t const& id
          , gpi::pc::proto::message_t const& request
          , gpi::pc::memory::manager_t& memory_manager
          , global::topology_t& topology
          )
        {
          try
          {
            return boost::apply_visitor
              (handle_message_t (logger, id, memory_manager, topology), request);
          }
          catch (...)
          {
            return gpi::pc::proto::error::error_t
              ( gpi::pc::proto::error::bad_request
              , fhg::util::current_exception_printer().string()
              );
          }
        }

        namespace
        {
          template<typename Buf, typename IO, typename Desc>
            void io_exact
              (int fd, Buf buffer, size_t size, IO io, Desc what, Desc got)
          {
            auto missing (size);

            while (missing > 0)
            {
              auto const bytes (io (fd, buffer, size));

              if (bytes == 0 || bytes > size)
              {
                //! \note the case bytes == 0 is used to terminate the
                //! communication_threads

                throw std::runtime_error
                  (str ( boost::format
                           ("io_exact: unable to %1% %2% bytes, %3% %4%")
                       % what
                       % size
                       % got
                       % bytes
                       )
                  );
              }

              missing -= bytes;
              buffer += bytes;
            }
          }
        }

        void read_exact (int fd, char* buffer, size_t size)
        {
          return io_exact
            (fd, buffer, size, &fhg::util::syscall::read, "read", "read");
        }
        void read_exact (int fd, void* buffer, size_t size)
        {
          return read_exact (fd, static_cast<char*> (buffer), size);
        }
        void write_exact (int fd, char const* buffer, size_t size)
        {
          return io_exact
            (fd, buffer, size, &fhg::util::syscall::write, "write", "written");
        }
        void write_exact (int fd, void const* buffer, size_t size)
        {
          return write_exact (fd, static_cast<char const*> (buffer), size);
        }
      }

      void manager_t::process_communication_thread
        (gpi::pc::type::process_id_t process_id, int socket)
      {
        _logger.emit ( "process container (" + std::to_string (process_id)
                     + ") started on socket " + std::to_string (socket)
                     , fhg::logging::legacy::category_level_trace
                     );

        for (;;)
        {
          try
          {
            gpi::pc::proto::message_t request;

            fhg::util::nest_exceptions<std::runtime_error>
              ( [&]
                {
                  gpi::pc::proto::header_t header;
                  read_exact (socket, &header, sizeof (header));

                  std::vector<char> buffer (header.length);
                  read_exact (socket, buffer.data(), buffer.size());

                  fhg::util::nest_exceptions<std::runtime_error>
                    ( [&]
                      {
                        std::stringstream sstr
                          (std::string (buffer.data(), buffer.size()));
                        boost::archive::binary_iarchive ia (sstr);
                        ia & request;
                      }
                    , "could not decode message"
                    );
                }
              , "could not receive message"
              );

            gpi::pc::proto::message_t const reply
              (handle_message (_logger, process_id, request, _memory_manager, _topology));

            fhg::util::nest_exceptions<std::runtime_error>
              ( [&]
                {
                  std::string data;
                  gpi::pc::proto::header_t header;

                  fhg::util::nest_exceptions<std::runtime_error>
                    ( [&]
                      {
                        std::stringstream sstr;
                        boost::archive::text_oarchive oa (sstr);
                        oa & reply;
                        data = sstr.str();
                      }
                    , "could not encode message"
                    );

                  header.length = data.size();

                  write_exact (socket, &header, sizeof (header));
                  write_exact (socket, data.data(), data.size());
                }
              , "could not send message"
              );
          }
          catch (...)
          {
            _logger.emit ( "process container " + std::to_string (process_id)
                         + " exited: "
                         + fhg::util::current_exception_printer().string()
                         , fhg::logging::legacy::category_level_error
                         );
            break;
          }
        }

        try
        {
          fhg::util::syscall::shutdown (socket, SHUT_RDWR);
        }
        catch (boost::system::system_error const& se)
        {
          if (se.code() != boost::system::errc::not_connected)
          {
            //! \note ignored: already disconnected = fine, we terminate
            throw;
          }
        }

        fhg::util::syscall::close (socket);

        _memory_manager.garbage_collect (process_id);

        _logger.emit ( "process container (" + std::to_string (process_id)
                     + ") terminated"
                     , fhg::logging::legacy::category_level_trace
                     );

        //! \note this detaches _this_ thread from everything
        //! left. Nothing shall be done in here that accesses `this`
        //! after the erase, or does anything! Let this thing die!

        if (!m_stopping)
        {
          std::lock_guard<std::mutex> const _ (_mutex_processes);
          m_processes.at (process_id).detach();
          m_processes.erase (process_id);
        }
      }


      manager_t::manager_t ( fhg::logging::stream_emitter& logger
                           , std::string const & p
                           , fhg::vmem::gaspi_context& gaspi_context
                           , std::unique_ptr<fhg::rpc::service_tcp_provider_with_deferred_dispatcher> topology_rpc_server
                           )
        : _logger (logger)
        , m_path (p)
        , m_socket (-1)
        , m_stopping (false)
        , m_process_counter (0)
        , _memory_manager (_logger, gaspi_context)
        , _topology ( _memory_manager
                    , gaspi_context
                    , std::move (topology_rpc_server)
                    )
      {
        fhg::util::nest_exceptions<std::runtime_error>
          ( [&]
            {
              safe_unlink (m_path);
            }
          , "could not unlink path '" + m_path + "'"
          );

        fhg::util::nest_exceptions<std::runtime_error>
          ( [&]
            {
              m_socket = fhg::util::syscall::socket (AF_UNIX, SOCK_STREAM, 0);
            }
          , "could not create process-container communication socket"
          );

        struct close_socket_on_error
        {
          ~close_socket_on_error()
          {
            if (!_committed)
            {
              fhg::util::syscall::close (_socket);
            }
          }
          bool _committed;
          int& _socket;
        } close_socket_on_error = {false, m_socket};

        {
          const int on (1);
          fhg::util::syscall::setsockopt (m_socket, SOL_SOCKET, SO_PASSCRED, &on, sizeof (on));
        }

        struct sockaddr_un my_addr;
        memset (&my_addr, 0, sizeof(my_addr));
        my_addr.sun_family = AF_UNIX;
        strncpy ( my_addr.sun_path
                , m_path.c_str()
                , sizeof(my_addr.sun_path) - 1
                );

        fhg::util::nest_exceptions<std::runtime_error>
          ( [&]
            {
              fhg::util::syscall::bind
                (m_socket, (struct sockaddr *)&my_addr, sizeof (struct sockaddr_un));
            }
          , "could not bind process-container communication socket to path " + m_path
          );

        struct delete_socket_file_on_error
        {
          ~delete_socket_file_on_error()
          {
            if (!_committed)
            {
              fhg::util::syscall::unlink (_path.string().c_str());
            }
          }
          bool _committed;
          boost::filesystem::path _path;
        } delete_socket_file_on_error = {false, m_path};

        fhg::util::syscall::chmod (m_path.c_str(), 0700);

        const std::size_t backlog_size (16);
        fhg::util::syscall::listen (m_socket, backlog_size);

        _listener_thread = std::thread (&manager_t::listener_thread_main, this);

        delete_socket_file_on_error._committed = true;
        close_socket_on_error._committed = true;
      }
    }
  }
}
