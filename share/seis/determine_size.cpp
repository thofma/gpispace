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

#include <iostream>
#include <string>
#include <fstream>

#include <stdexcept>

#include "SegYBHeader.h"
#include "SegYEBCHeader.h"
#include "SegYHeader.h"

#include "swap_bytes.h"

// ************************************************************************* //

#ifdef __cplusplus
extern "C"
{
#endif

void determine_size ( const std::string & filename
                    , const std::string & type
                    , long & num       // number of traces in this file
                    , long & size      // size of one trace + header in bytes
                    )
{
  if (type == "text")
    {
      throw std::runtime_error ("determine_size: note implemented for file type 'text' ");
    }
  else if ( (type == "segy") || (type == "su") )
    {
       const int endianess( LITENDIAN );
       FILE * inp (fopen (filename.c_str(), "rb"));

       if (inp == NULL)
         {
           throw std::runtime_error ("determine_size: could not open " + filename);
         }

       if (type == "segy")
	   fseek (inp, sizeof(SegYBHeader) + sizeof(SegYEBCHeader), SEEK_SET);

      SegYHeader Header;
      if (1 != fread ((char*)&Header, sizeof(SegYHeader), 1, inp))
      {
        throw std::runtime_error
          ("determine_size: Could not read SegYHeader from " + filename);
      };
      if ( (type == "segy") && (endianess != BIGENDIAN) )
      {
	  swap_bytes((void*)&(Header.ns), 1, sizeof(unsigned short));
      }
      size = sizeof(SegYHeader) + Header.ns*sizeof(float);

      fseek (inp, 0, SEEK_END);
      const long endpos = ftell (inp);
      if (type == "segy")
	  num = (endpos - sizeof(SegYBHeader) - sizeof(SegYEBCHeader)) / size;
      else
	  num = (endpos ) / size;
      fclose (inp);
    }
  else
    {
      throw std::runtime_error ("determine_size: unknown type " + type);
    }
}

#ifdef __cplusplus
}
#endif
