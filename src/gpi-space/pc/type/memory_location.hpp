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

#include <iostream>
#include <string>

#include <boost/lexical_cast.hpp>
// serialization
#include <boost/serialization/nvp.hpp>

#include <gpi-space/pc/type/typedefs.hpp>
#include <gpi-space/pc/type/handle.hpp>

namespace gpi
{
  namespace pc
  {
    namespace type
    {
      struct memory_location_t
      {
        gpi::pc::type::handle_t handle;
        gpi::pc::type::offset_t offset;

        memory_location_t ()
          : handle (0)
          , offset (0)
        {}

        memory_location_t (const handle_t hdl, const offset_t off)
          : handle (hdl)
          , offset (off)
        {}

      private:
        friend class boost::serialization::access;
        template<typename Archive>
        void serialize (Archive & ar, const unsigned int /*version*/)
        {
          ar & BOOST_SERIALIZATION_NVP( handle );
          ar & BOOST_SERIALIZATION_NVP( offset );
        }
      };

      // handle[+off=0][,size=0]
      struct memory_region_t
      {
        memory_location_t     location;
        gpi::pc::type::size_t size;

        memory_region_t ()
          : location ()
          , size (0)
        {}

        memory_region_t (const memory_location_t loc, const size_t sz)
          : location (loc)
          , size (sz)
        {}
      private:
        friend class boost::serialization::access;
        template<typename Archive>
        void serialize (Archive & ar, const unsigned int /*version*/)
        {
          ar & BOOST_SERIALIZATION_NVP( location );
          ar & BOOST_SERIALIZATION_NVP( size );
        }
      };

      inline
      std::ostream & operator<< ( std::ostream & os
                                , const memory_location_t & loc
                                )
      {
        return os << handle_t(loc.handle) << "+" << loc.offset;
      }

      inline
      std::ostream & operator<< ( std::ostream & os
                                , const memory_region_t & reg
                                )
      {
        return os << reg.location << "," << reg.size;
      }

      inline
      std::istream & operator>> ( std::istream & is
                                , memory_location_t & loc
                                )
      {
        // read a string
        std::string tmp;
        is >> tmp;

        std::string::size_type split_pos (tmp.find('+'));
        loc.handle =
            boost::lexical_cast<handle_t>(tmp.substr( 0
                                                    , split_pos
                                                    )
                                         );
        if (split_pos != std::string::npos)
        {
          loc.offset =
              boost::lexical_cast<offset_t>
                (tmp.substr ( split_pos+1
                            , tmp.size() - split_pos
                            )
                );
        }
        else
        {
          loc.offset = 0;
        }
        return is;
      }

      inline
      std::istream & operator>> ( std::istream & is
                                , memory_region_t & reg
                                )
      {
        // read a string
        std::string tmp;
        is >> tmp;

        std::string::size_type split_pos (tmp.find(','));
        reg.location =
            boost::lexical_cast<memory_location_t>(tmp.substr( 0
                                                             , split_pos
                                                             )
                                                  );
        if (split_pos != std::string::npos)
        {
          reg.size =
              boost::lexical_cast<size_t>
                (tmp.substr ( split_pos+1
                            , tmp.size() - split_pos
                            )
                );
        }
        else
        {
          reg.size = 0;
        }
        return is;
      }
    }
  }
}
