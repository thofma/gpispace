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
#include <stdexcept>

#include <boost/lexical_cast.hpp>
#include <boost/serialization/nvp.hpp>

#include <gpi-space/pc/type/typedefs.hpp>

#include <functional>

namespace gpi
{
  namespace pc
  {
    namespace type
    {
      template <size_t Bits>
      void check_for_overflow (const gpi::pc::type::handle_id_t part)
      {
        if (part > ((1UL << Bits) - 1))
        {
          throw std::runtime_error ("handle would overflow");
        }
      }

      struct handle_t
      {
        static const int total_bits = (sizeof(handle_id_t)*8);
        static const int ident_bits = 12;
        static const int typec_bits = 4;
        static const int global_count_bits = (total_bits - ident_bits - typec_bits);
        static const int local_count_bits = (total_bits - typec_bits);
        static const int string_length = (2+total_bits/4); // 0x...

        handle_t ()
          : handle (0)
        {}

        handle_t (handle_id_t h)
          : handle (h)
        {}

        operator handle_id_t () const { return handle; }
        bool operator== (const handle_id_t o) const {return handle == o;}
        bool operator<  (const handle_id_t o) const {return handle < o; }

        union
        {
          struct __attribute__((packed))
          {
            union
            {
              struct // == sizeof(handle_id_t)
              {
                handle_id_t cntr  : global_count_bits;
                handle_id_t ident : ident_bits;
                int _pad  : typec_bits;
              } gpi;
              struct // == sizeof(handle_id_t)
              {
                handle_id_t cntr : local_count_bits;
                int _pad  : typec_bits;
              } shm;
              struct // == sizeof(handle_id_t)
              {
                handle_id_t _pad : (total_bits - typec_bits);
                int type : typec_bits;
              };
            };
          };
          handle_id_t handle;
        };

      private:
        friend class boost::serialization::access;
        template<typename Archive>
        void serialize (Archive & ar, const unsigned int /*version*/)
        {
          ar & BOOST_SERIALIZATION_NVP( handle );
        }
      };

      inline
      void validate (const handle_t & hdl)
      {
        if (hdl.type == 0)
        {
          throw std::invalid_argument
            ("invalid handle: " + boost::lexical_cast<std::string>(hdl.handle));
        }
      }

      inline
      std::ostream & operator << (std::ostream & os, const handle_t h)
      {
        const std::ios_base::fmtflags saved_flags (os.flags());
        const char saved_fill = os.fill (' ');
        const std::size_t saved_width = os.width (0);

        os << "0x";
        os.flags (std::ios::hex);
        os.width (handle_t::string_length - 2);
        os.fill ('0');
        os << h.handle;

        os.flags (saved_flags);
        os.fill (saved_fill);
        os.width (saved_width);

        return os;
      }

      inline
      std::istream & operator>> ( std::istream & is
                                , handle_t & h
                                )
      {
        const std::ios_base::fmtflags saved_flags (is.flags());
        is.flags (std::ios::hex);
        is >> h.handle;

        validate (h);

        is.flags (saved_flags);
        return is;
      }
    }
  }
}

namespace std
{
  template<> struct hash<gpi::pc::type::handle_t>
  {
    size_t operator()(gpi::pc::type::handle_t const& x) const
    {
      return std::hash<gpi::pc::type::handle_id_t>() (x.handle);
    }
  };
}
