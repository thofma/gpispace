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

#include <gpi-space/pc/memory/beegfs_area.hpp>

#include <cerrno>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <cstring>

#include <gpi-space/pc/url.hpp>
#include <util-generic/cxx14/make_unique.hpp>
#include <util-generic/finally.hpp>
#include <util-generic/hostname.hpp>
#include <util-generic/print_exception.hpp>
#include <fhg/util/boost/optional.hpp>
#include <fhg/util/read_bool.hpp>
#include <util-generic/read_file.hpp>
#include <util-generic/syscall.hpp>
#include <util-generic/unused.hpp>
#include <util-generic/write_file.hpp>

#include <vmem/segment/beegfs.hpp>

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/system/system_error.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <gpi-space/pc/type/flags.hpp>

namespace gpi
{
  namespace pc
  {
    namespace memory
    {
      namespace detail
      {
        static beegfs_area_t::path_t data_path (beegfs_area_t::path_t const &p)
        {
          return p / "data";
        }

        static beegfs_area_t::path_t version_path (beegfs_area_t::path_t const &p)
        {
          return p / "version";
        }

        static beegfs_area_t::path_t lock_path (beegfs_area_t::path_t const &p)
        {
          return p / "lock";
        }
        namespace
        {
          boost::filesystem::path lock_info_path (boost::filesystem::path const& path)
          {
            return lock_path (path) / "info";
          }

          struct lock_info
          {
            lock_info (boost::filesystem::path const& path)
            {
              std::string const content
                (fhg::util::read_file (detail::lock_info_path (path)));
              std::istringstream iss (content);
              boost::archive::text_iarchive ia (iss);
              ia & hostname;
              ia & pid;
            }

            static void write_local_information
              (boost::filesystem::path const& path)
            {
              std::ostringstream oss;
              boost::archive::text_oarchive oa (oss);
              oa & fhg::util::hostname();
              oa & fhg::util::syscall::getpid();
              fhg::util::write_file
                (detail::lock_info_path (path), oss.str());
            }

            std::string hostname;
            pid_t pid;
          };
        }
      }
      namespace
      {
        struct scoped_file
        {
          scoped_file ( boost::filesystem::path const& path
                      , int flags
                      , int perms
                      )
            : _fd ( fhg::util::syscall::open
                      (path.string().c_str(), flags, perms)
                  )
          {}
          ~scoped_file()
          {
            fhg::util::syscall::close (_fd);
          }
          void ftruncate (std::size_t size)
          {
            fhg::util::syscall::ftruncate (_fd, size);
          }

          int _fd;
        };
      }

      beegfs_area_t::beegfs_area_t ( fhg::logging::stream_emitter& logger
                                   , const gpi::pc::type::process_id_t creator
                                   , const beegfs_area_t::path_t & path
                                   , const gpi::pc::type::size_t size        // total
                                   , const gpi::pc::type::flags_t flags
                                   , gpi::pc::global::itopology_t & topology
                                   , handle_generator_t& handle_generator
                                   )
        : area_t ( logger
                 , beegfs_area_t::area_type
                 , creator
                 , path.string ()
                 , size
                 , flags
                 , handle_generator
                 )
        , m_path (path)
        , m_version (BEEGFS_AREA_VERSION)
        , m_topology (topology)
      {
        try
        {
          open();
        }
        catch (...)
        {
          std::throw_with_nested
            (std::runtime_error ("opening '" + path.string() + "' failed"));
        }
      }

      beegfs_area_t::~beegfs_area_t ()
      {
        try
        {
          close();
        }
        catch (...)
        {
          _logger.emit ( fhg::util::current_exception_printer().string()
                       , fhg::logging::legacy::category_level_error
                       );
        }
      }

      std::string beegfs_area_t::version_string() const
      {
        return "BEEGFS segment version " + std::to_string (m_version);
      }

      beegfs_area_t::lock_file_helper::lock_file_helper
        (beegfs_area_t& area)
      try
        : filesystem_lock_directory (detail::lock_path (area.m_path))
      {
        detail::lock_info::write_local_information (area.m_path);
      }
      catch (...)
      {
        boost::optional<std::string> additional_information;
        try
        {
          detail::lock_info const existing (area.m_path);
          additional_information
            = " (existing lock was created by pid "
            + std::to_string (existing.pid)
            + " on host " + existing.hostname + ")";
        }
        catch (...)
        {
          //! \note ignore: just directory existed but no info file,
          //! or reading it failed due to being corrupt or the lock
          //! being removed the exact same time we tried to
          //! lock. either way, we can't give additional information
          //! to the user
        }

        std::throw_with_nested
          ( std::runtime_error
              ( "segment is still locked or creating lock at directory "
              + static_cast<boost::filesystem::path const&> (*this).string()
              + " failed" + additional_information.get_value_or ("")
              )
          );
      }

      void beegfs_area_t::open()
      {
        if (get_owner())
        {
          if (boost::filesystem::exists (m_path))
          {
            throw std::runtime_error ( "unable to create BeeGFS segment: "
                                     + m_path.string() + " already exists"
                                     );
          }

          boost::filesystem::create_directories (m_path);

          {
            fhg::util::write_file ( detail::version_path (m_path)
                                  , version_string()
                                  );
          }

          {
            scoped_file data ( detail::data_path (m_path)
                             , O_CREAT | O_EXCL | O_RDWR
                             , 0600 // TODO: pass in permissions
                             );

            data.ftruncate (size());
          }
        }
        bool succeeded (false);
        FHG_UTIL_FINALLY
          ( [&]
            {
              if (!succeeded && get_owner())
              {
                boost::filesystem::remove (detail::data_path (m_path));
                boost::filesystem::remove (detail::version_path (m_path));
                boost::filesystem::remove (m_path);
              }
            }
          );

        //! \todo move in front of creation even by checking first
        //! existing parent path
        fhg::vmem::segment::beegfs::check_requirements (m_path);

        {
          boost::optional<std::string> const found_version
            ( fhg::util::boost::exception_is_none
                ( &fhg::util::read_file<std::string>
                , detail::version_path (m_path)
                )
            );

          if (!!found_version && found_version.get() != version_string())
          {
            throw std::logic_error
              ( "the file segment was created by a newer version:"
                " found: " + found_version.get()
              + " wanted: " + version_string()
              );
          }
        }

        if (get_owner())
        {
          _lock_file
            = fhg::util::cxx14::make_unique<lock_file_helper> (*this);
        }
        FHG_UTIL_FINALLY ([&] { if (!succeeded) { _lock_file.reset(); }});

        path_t data_path = detail::data_path (m_path);
        int const fd ( fhg::util::syscall::open
                         (data_path.string ().c_str(), O_RDWR)
                     );
        FHG_UTIL_FINALLY ([&] { fhg::util::syscall::close (fd); });

        off_t const file_size (fhg::util::syscall::lseek (fd, 0, SEEK_END));
        fhg::util::syscall::lseek (fd, 0, SEEK_SET);

        if (size() != (gpi::pc::type::size_t)file_size)
        {
          throw std::logic_error
            ( "segment file on disk has size "
            + std::to_string (file_size)
            + " but tried to open it with size "
            + std::to_string (size())
            );
        }

        //! \todo better constant (number of threads? configurable?)
        for (int i (0); i < 20; ++i)
        {
          _fds.put
            (fhg::util::syscall::open (data_path.string().c_str(), O_RDWR));
        }

        _logger.emit ( "BEEGFS memory created: path: " + m_path.string()
                     + " size: " + std::to_string (size())
                     , fhg::logging::legacy::category_level_trace
                     );

        succeeded = true;
      }

      void beegfs_area_t::close()
      {
        if (get_owner())
        {
          boost::filesystem::remove (detail::data_path (m_path));
          boost::filesystem::remove (detail::version_path (m_path));

          _lock_file.reset();

          boost::filesystem::remove (m_path);
        }
      }

      gspc::vmem::dtmmgr::Arena_t
      beegfs_area_t::grow_direction (const gpi::pc::type::flags_t) const
      {
        // we do not support multiple arenas in this memory type
        return gspc::vmem::dtmmgr::ARENA_UP;
      }

      void *
      beegfs_area_t::raw_ptr (gpi::pc::type::offset_t)
      {
        return nullptr;
      }

      bool
      beegfs_area_t::is_allowed_to_attach (const gpi::pc::type::process_id_t) const
      {
        return false;
      }

      void
      beegfs_area_t::alloc_hook (const gpi::pc::type::handle::descriptor_t &hdl)
      {
        if (hdl.creator != (gpi::pc::type::process_id_t)(-1))
        {
          m_topology.alloc ( descriptor ().id
                           , hdl.id
                           , hdl.offset
                           , hdl.size
                           , hdl.local_size
                           , hdl.name
                           );
        }
      }

      void
      beegfs_area_t::free_hook (const gpi::pc::type::handle::descriptor_t &hdl)
      {
        m_topology.free(hdl.id);
      }

      bool
      beegfs_area_t::is_range_local( const gpi::pc::type::handle::descriptor_t &
                                , const gpi::pc::type::offset_t
                                , const gpi::pc::type::offset_t
                                ) const
      {
        return true;
      }

      gpi::pc::type::size_t
      beegfs_area_t::get_local_size ( const gpi::pc::type::size_t size
                                 , const gpi::pc::type::flags_t
                                 ) const
      {
        return size;
      }

      double beegfs_area_t::get_transfer_costs
        ( const gpi::pc::type::memory_region_t& transfer
        , const gpi::rank_t FHG_UTIL_UNUSED
                            ( rank
                            , "NYI: use BeeGFS::beegfs_getStripeTarget"
                              "to get locality information"
                            )
        ) const
      {
        return transfer.size;
      }

      gpi::pc::type::size_t
      beegfs_area_t::read_from_impl ( gpi::pc::type::offset_t offset
                                 , void *buffer
                                 , gpi::pc::type::size_t amount
                                 )
      {
        int const fd (_fds.get());
        FHG_UTIL_FINALLY ([&] { _fds.put (fd); });
        fhg::util::syscall::lseek (fd, offset, SEEK_SET);
        return fhg::util::syscall::read (fd, buffer, amount);
      }

      gpi::pc::type::size_t
      beegfs_area_t::write_to_impl ( gpi::pc::type::offset_t offset
                                , const void *buffer
                                , gpi::pc::type::size_t amount
                                )
      {
        int const fd (_fds.get());
        FHG_UTIL_FINALLY ([&] { _fds.put (fd); });

        struct flock lock_info;
        lock_info.l_type = F_WRLCK;
        lock_info.l_whence = SEEK_SET;
        lock_info.l_start = offset;
        lock_info.l_len = amount;

        fhg::util::syscall::fcntl_setlkw (fd, &lock_info);
        FHG_UTIL_FINALLY ( [&]
                           {
                             lock_info.l_type = F_UNLCK;
                             fhg::util::syscall::fcntl_setlkw (fd, &lock_info);
                           }
                         );
        fhg::util::syscall::lseek (fd, offset, SEEK_SET);
        return fhg::util::syscall::write (fd, buffer, amount);
      }

      area_ptr_t beegfs_area_t::create
        ( fhg::logging::stream_emitter& logger
        , std::string const &url_s
        , gpi::pc::global::itopology_t & topology
        , handle_generator_t& handle_generator
        , type::id_t owner
        )
      {
        url_t url (url_s);
        gpi::pc::type::flags_t flags = F_NONE;

        if (    fhg::util::read_bool (url.get ("exclusive").get_value_or ("false")))
        {
          gpi::flag::set (flags, F_EXCLUSIVE);
        }

        gpi::pc::type::size_t size =
          boost::lexical_cast<gpi::pc::type::size_t>(url.get ("total_size").get_value_or ("0"));

        area_ptr_t area (new beegfs_area_t ( logger
                                           , owner
                                           , url.path()
                                           , size
                                           , flags | F_GLOBAL
                                           , topology
                                           , handle_generator
                                           )
                        );
        return area;
      }
    }
  }
}
