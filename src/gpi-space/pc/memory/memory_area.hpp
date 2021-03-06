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

#include <mmgr/dtmmgr.hpp>

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include <logging/stream_emitter.hpp>

#include <gpi-space/types.hpp>
#include <gpi-space/pc/type/typedefs.hpp>
#include <gpi-space/pc/type/handle.hpp>
#include <gpi-space/pc/type/memory_location.hpp>
#include <gpi-space/pc/type/segment_descriptor.hpp>
#include <gpi-space/pc/type/handle_descriptor.hpp>

#include <gpi-space/pc/memory/handle_generator.hpp>

#include <fhg/util/thread/queue.hpp>

#include <future>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace gpi
{
  namespace pc
  {
    namespace memory
    {
      class area_t : boost::noncopyable
      {
      public:
        virtual ~area_t () = default;

        /* public interface the basic implementation is the same
           for all kinds of segments.

           specific segments may/must hook into specific implementation details
           though
        */
        gpi::pc::type::size_t size () const;
        std::string const & name () const;
        bool in_use () const;
        int type () const;
        gpi::pc::type::flags_t flags () const;

        // WORK HERE:
        //    this function *must not* be called from the dtor
        //    otherwise we endup calling pure virtual functions
        void garbage_collect ();
        void garbage_collect (const gpi::pc::type::process_id_t pid);

        void                set_id (const gpi::pc::type::id_t id);
        gpi::pc::type::id_t get_id () const;

        gpi::pc::type::id_t get_owner () const;

        gpi::pc::type::handle_t
        alloc ( const gpi::pc::type::process_id_t proc_id
              , const gpi::pc::type::size_t size
              , const std::string & name
              , const gpi::pc::type::flags_t flags
              );

        int
        remote_alloc ( const gpi::pc::type::handle_t hdl
                     , const gpi::pc::type::offset_t offset
                     , const gpi::pc::type::size_t size
                     , const gpi::pc::type::size_t local_size
                     , const std::string & name
                     );

        void
        remote_free (const gpi::pc::type::handle_t hdl);

        void
        free (const gpi::pc::type::handle_t hdl);

        void defrag (const gpi::pc::type::size_t free_at_least = 0);

        gpi::pc::type::segment::descriptor_t const &
        descriptor () const;

        gpi::pc::type::segment::descriptor_t & descriptor ();

        gpi::pc::type::handle::descriptor_t const &
        descriptor (const gpi::pc::type::handle_t) const;

        gpi::pc::type::size_t
        attach_process (const gpi::pc::type::process_id_t);

        gpi::pc::type::size_t
        detach_process (const gpi::pc::type::process_id_t);

        bool is_local (const gpi::pc::type::memory_region_t region) const;
        bool is_local ( const gpi::pc::type::memory_location_t loc
                      , const gpi::pc::type::size_t amt
                      ) const;
        bool is_eligible_for_deletion () const;

        bool is_process_attached (const gpi::pc::type::process_id_t) const;

        void check_bounds ( const gpi::pc::type::memory_location_t & loc
                          , const gpi::pc::type::size_t size
                          ) const;

        /**
           Return a raw pointer to the given memory location, if possible.

           It may return nullptr in the following cases:

           - the location is out of bounds
           - the implementation does not support raw pointers.
         */
        void *pointer_to (const gpi::pc::type::memory_location_t & loc);

        gpi::pc::type::size_t
        read_from ( gpi::pc::type::memory_location_t loc
                  , void *buffer
                  , gpi::pc::type::size_t amount
                  );

        gpi::pc::type::size_t
        write_to ( gpi::pc::type::memory_location_t loc
                 , const void *buffer
                 , gpi::pc::type::size_t amount
                 );

        std::packaged_task<void()> get_transfer_task
          ( const type::memory_location_t src
          , const type::memory_location_t dst
          , area_t& dst_area
          , type::size_t amount
          );
        virtual double get_transfer_costs ( const gpi::pc::type::memory_region_t&
                                          , const gpi::rank_t
                                          ) const = 0;
      protected:
        area_t ( fhg::logging::stream_emitter&
               , const gpi::pc::type::segment::segment_type type
               , const gpi::pc::type::process_id_t creator
               , const std::string & name
               , const gpi::pc::type::size_t size
               , const gpi::pc::type::flags_t flags
               , handle_generator_t&
               );

        void reinit ();

        gpi::pc::type::offset_t location_to_offset (gpi::pc::type::memory_location_t loc);

        /* hook functions that need to be overridded by specific segments */
        virtual
        gspc::vmem::dtmmgr::Arena_t grow_direction (const gpi::pc::type::flags_t) const = 0;

        virtual
        bool is_allowed_to_attach (const gpi::pc::type::process_id_t) const;

        virtual bool is_range_local ( const gpi::pc::type::handle::descriptor_t &
                                    , const gpi::pc::type::offset_t a
                                    , const gpi::pc::type::offset_t b
                                    ) const = 0;
        virtual void *raw_ptr (gpi::pc::type::offset_t off) = 0;

        virtual gpi::pc::type::size_t get_local_size ( const gpi::pc::type::size_t size
                                                     , const gpi::pc::type::flags_t flags
                                                     ) const = 0;

        virtual std::packaged_task<void()> get_send_task
          ( area_t& src_area
          , const gpi::pc::type::memory_location_t src
          , const gpi::pc::type::memory_location_t dst
          , gpi::pc::type::size_t amount
          );

        virtual std::packaged_task<void()> get_recv_task
          ( area_t& dst_area
          , const gpi::pc::type::memory_location_t dst
          , const gpi::pc::type::memory_location_t src
          , gpi::pc::type::size_t amount
          );

        virtual gpi::pc::type::size_t
        read_from_impl ( gpi::pc::type::offset_t offset
                       , void *buffer
                       , gpi::pc::type::size_t amount
                       );

        virtual gpi::pc::type::size_t
        write_to_impl ( gpi::pc::type::offset_t offset
                      , const void *buffer
                      , gpi::pc::type::size_t amount
                      );

        /*
         hook functions that may be overriden
         */
        virtual void alloc_hook (const gpi::pc::type::handle::descriptor_t &) {}
        virtual void  free_hook (const gpi::pc::type::handle::descriptor_t &) {}
      private:
        typedef std::recursive_mutex mutex_type;
        typedef std::unique_lock<mutex_type> lock_type;
        typedef std::unordered_set <gpi::pc::type::process_id_t> process_ids_t;
        typedef std::unordered_map< gpi::pc::type::handle_t
                                  , gpi::pc::type::handle::descriptor_t
                                  > handle_descriptor_map_t;

        void update_descriptor_from_mmgr ();

        void internal_alloc (gpi::pc::type::handle::descriptor_t &);

      protected:
        fhg::logging::stream_emitter& _logger;

      private:
        mutable mutex_type m_mutex;
        gpi::pc::type::segment::descriptor_t m_descriptor;
        gspc::vmem::dtmmgr m_mmgr;
        handle_descriptor_map_t m_handles;
        process_ids_t m_attached_processes;

        handle_generator_t& _handle_generator;
      };

      typedef boost::shared_ptr<area_t> area_ptr_t;
    }
  }
}
