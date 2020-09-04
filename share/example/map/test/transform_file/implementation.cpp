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

#include <map/interface.hpp>
#include <map/test/transform_file/type.hpp>

#include <boost/format.hpp>

#include <fstream>
#include <iostream>
#include <stdexcept>

void map_produce ( map::user_data_type const& user_data
                 , map::memory_buffer_type buffer
                 , unsigned long id
                 )
{
  transform_file::parameter const parameter
  {transform_file::from_bytearray (user_data)};
  std::string const input {parameter.input().string()};
  unsigned long const size {parameter.size()};

  std::cout << "read " << input << "[" << id << "]\n";

  std::ifstream stream (input, std::ifstream::binary);

  if (!stream)
  {
    throw std::runtime_error
      ((boost::format ("Could not open '%1%' for reading") % input).str());
  }

  if (!stream.seekg (id * buffer.second, stream.beg))
  {
    throw std::runtime_error
      (( boost::format ("Could not seek to position '%2%' of '%1%'")
       % input
       % (id * buffer.second)
       ).str()
      );
  };

  if (!stream.read ( static_cast<char*> (buffer.first)
                   , std::min (buffer.second, size - id * buffer.second)
                   )
     )
  {
    throw std::runtime_error
      ((boost::format ("Could not read from '%1%'") % input).str());
  };
}

void map_process
  ( map::user_data_type const&
  , map::const_memory_buffer_type input
  , map::memory_buffer_type output
  )
{
  std::cout << "transform\n";

  if (input.second != output.second)
  {
    throw std::logic_error
      ( ( boost::format ("input/output buffer sizes differ: %1% != %2%")
        % input.second
        % output.second
        ).str()
      );
  }

  char const* const mem_input (static_cast<char const*> (input.first));
  char* const mem_output (static_cast<char*> (output.first));

  std::transform ( mem_input, mem_input + input.second
                 , mem_output
                 , [](char c) { return std::tolower (c); }
                 );
}

void map_consume ( map::user_data_type const& user_data
                 , map::const_memory_buffer_type buffer
                 , unsigned long id
                 )
{
  transform_file::parameter const parameter
  {transform_file::from_bytearray (user_data)};
  std::string const output {parameter.output().string()};
  unsigned long const size {parameter.size()};

  std::cout << "write " << output << "[" << id << "]\n";

  std::fstream stream
    (output, std::fstream::binary | std::fstream::out | std::fstream::in);

  if (!stream)
  {
    throw std::runtime_error
      ((boost::format ("Could not open '%1%' for writing") % output).str());
  }

  if (!stream.seekp (id * buffer.second, stream.beg))
  {
    throw std::runtime_error
      (( boost::format ("Could not seek to position '%2%' of '%1%'")
       % output
       % (id * buffer.second)
       ).str()
      );
  };

  if (!stream.write ( static_cast<char const*> (buffer.first)
                    , std::min (buffer.second, size - id * buffer.second)
                    )
     )
  {
    throw std::runtime_error
      ((boost::format ("Could not write to '%1%'") % output).str());
  };
}
