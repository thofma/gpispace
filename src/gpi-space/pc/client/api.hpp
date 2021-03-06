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

#include <logging/stream_emitter.hpp>

#include <gpi-space/pc/proto/message.hpp>
#include <gpi-space/pc/segment/segment.hpp>
#include <gpi-space/pc/type/flags.hpp>
#include <gpi-space/pc/type/typedefs.hpp>
#include <gpi-space/pc/type/memory_location.hpp>

#include <we/type/range.hpp>

#include <boost/filesystem/path.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include <list>
#include <map>
#include <mutex>
#include <string>
#include <utility>

namespace gpi
{
  namespace pc
  {
    namespace client
    {
      class api_t : public boost::noncopyable
      {
      public:
        typedef boost::shared_ptr<gpi::pc::segment::segment_t> segment_ptr;
        typedef std::map<gpi::pc::type::segment_id_t, segment_ptr> segment_map_t;

        api_t (fhg::logging::stream_emitter&, std::string const & path);

        ~api_t ();

        //! \todo add const (do not call stop()) in memcpy, ptr, wait
        //! and replace /*const*/ in
        //!   src/drts/worker/drts-kernel.cpp
        //!   src/drts/worker/drts.cpp
        //!   src/drts/worker/drts.hpp
        //!   src/we/loader/module_call.cpp
        //!   src/we/loader/module_call.hpp

        // api to gpi
        type::handle_id_t alloc ( const type::segment_id_t
                                , const type::size_t
                                , const std::string & desc
                                , const type::flags_t
                                );
        void free (const type::handle_id_t);

      private:
        type::memcpy_id_t memcpy ( type::memory_location_t const& dst
                                 , type::memory_location_t const& src
                                 , type::size_t const amount
                                 );
        void wait (type::memcpy_id_t const&);

      public:
        void memcpy_and_wait ( type::memory_location_t const& dst
                             , type::memory_location_t const& src
                             , type::size_t const amount
                             )
        {
          wait (memcpy (dst, src, amount));
        }

        std::function<double (std::string const&)>
        transfer_costs (std::list<std::pair<we::local::range, we::global::range>> const&);

        std::map<std::string, double>
        transfer_costs (std::list<gpi::pc::type::memory_region_t> const&);

        void * ptr(const gpi::pc::type::handle_t h);

        gpi::pc::type::segment_id_t register_segment( std::string const & name
                                                    , const gpi::pc::type::size_t sz
                                                    );
        void unregister_segment(const gpi::pc::type::segment_id_t);

      private:
        void stop ();
        gpi::pc::type::handle::descriptor_t
        info(const gpi::pc::type::handle_t h);

        type::segment_id_t create_segment (std::string const& info);
        void delete_segment (type::segment_id_t);

        typedef std::recursive_mutex mutex_type;
        typedef std::unique_lock<mutex_type> lock_type;

        gpi::pc::proto::message_t communicate (gpi::pc::proto::message_t const &);

        fhg::logging::stream_emitter& _logger;
        mutable mutex_type m_mutex;
        int m_socket;
        segment_map_t m_segments;

        friend struct remote_segment;
      };

      struct remote_segment
      {
      public:
        static struct {} gaspi;
        static struct {} filesystem;

        remote_segment ( api_t&
                       , decltype (gaspi)
                       , type::size_t
                       , type::size_t communication_buffer
                       , type::size_t num_communication_buffers
                       );
        remote_segment ( api_t&
                       , decltype (filesystem)
                       , type::size_t
                       , boost::filesystem::path
                       );

      private:
        remote_segment (api_t&, std::string const&);

      public:
        ~remote_segment();
        remote_segment (remote_segment const&) = delete;
        remote_segment (remote_segment&&) = delete;
        remote_segment& operator= (remote_segment const&) = delete;
        remote_segment& operator= (remote_segment&&) = delete;

        operator type::segment_id_t() const
        {
          return _segment_id;
        }

      private:
        api_t& _api;
        type::segment_id_t _segment_id;
      };
    }
  }
}
