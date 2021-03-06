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

#include <vector>

// serialization
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>

#include <gpi-space/pc/type/typedefs.hpp>
#include <gpi-space/pc/type/flags.hpp>
#include <gpi-space/pc/type/handle.hpp>
#include <gpi-space/pc/type/time_stamp.hpp>

namespace gpi
{
  namespace pc
  {
    namespace type
    {
      namespace handle
      {
        inline bool is_null (const gpi::pc::type::handle_id_t i)
        {
          return 0 == i;
        }

        struct descriptor_t
        {
          gpi::pc::type::handle_t id;
          gpi::pc::type::segment_id_t segment;
          gpi::pc::type::offset_t offset;
          gpi::pc::type::size_t size;
          gpi::pc::type::size_t local_size;
          gpi::pc::type::size_t nref;
          gpi::pc::type::name_t name;
          gpi::pc::type::process_id_t creator;
          gpi::pc::type::flags_t flags;
          gpi::pc::type::time_stamp_t ts;

          descriptor_t ()
            : id (0)
            , segment (0)
            , offset (0)
            , size (0)
            , local_size (0)
            , nref (0)
            , name ("")
            , flags (0)
          {}

          bool operator< (const descriptor_t & other) const
          {
            return id < other.id;
          }

        private:
          friend class boost::serialization::access;
          template<typename Archive>
          void serialize (Archive & ar, const unsigned int /*version*/)
          {
            ar & BOOST_SERIALIZATION_NVP( id.handle );
            ar & BOOST_SERIALIZATION_NVP( segment );
            ar & BOOST_SERIALIZATION_NVP( offset );
            ar & BOOST_SERIALIZATION_NVP( size );
            ar & BOOST_SERIALIZATION_NVP (local_size);
            ar & BOOST_SERIALIZATION_NVP( nref );
            ar & BOOST_SERIALIZATION_NVP( name );
            ar & BOOST_SERIALIZATION_NVP( creator );
            ar & BOOST_SERIALIZATION_NVP( flags );
            ar & BOOST_SERIALIZATION_NVP( ts );
          }
        };
      }
    }
  }
}
