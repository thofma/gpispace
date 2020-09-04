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

#include <fhg/util/backtracing_exception.hpp>

#ifndef NO_BACKTRACE
#include <sstream>
#include <execinfo.h>
#include <malloc.h>
#ifndef NO_CXA_DEMANGLE
#include <cxxabi.h>
#endif
#endif

namespace fhg
{
  namespace util
  {
#ifndef NO_BACKTRACE
    std::string make_backtrace (const std::string& reason)
    {
      std::stringstream what;
      what << reason << std::endl << std::endl;

      static const int nframes (50);
      static void* array[nframes];

      const int size (backtrace (array, nframes));
      char** strings (backtrace_symbols (array, size));

      for (int i (0); i < size; ++i)
      {
        const std::string line (strings[i]);
        if ( line.find ("make_backtrace") != std::string::npos
           || line.find ("backtracing_exception") != std::string::npos
           )
        {
          continue;
        }

#ifdef __APPLE__
        const size_t plus_pos (line.rfind (" + "));
        const size_t start_pos (line.rfind (" ", plus_pos - 1) + 1);

        const std::string function_name
          (line.substr (start_pos, plus_pos - start_pos));
#elif defined (__linux__)
        const size_t plus_pos (line.rfind ("+"));
        const size_t end_pos (line.find (")"));
        const size_t name_end_pos ( plus_pos == std::string::npos
                                  ? end_pos
                                  : plus_pos
                                  );
        const size_t start_pos (line.rfind ("(", name_end_pos) + 1);
        const size_t address_pos (line.rfind ("["));

        const std::string function_name
          ( name_end_pos - start_pos <= 1
          ? "unknown function"
          : line.substr (start_pos, name_end_pos - start_pos)
          );

#elif !defined (NO_CXA_DEMANGE)
#warning "Can't pretty print backtraces on this platform."
#define NO_CXA_DEMANGLE
        const std::string function_name;
#endif

#ifndef NO_CXA_DEMANGLE
        int status;
        const char* ret
          (abi::__cxa_demangle(function_name.c_str(), nullptr, nullptr, &status));
        const std::string demangled_name
          (status == 0 ? ret : function_name);
#else
        const std::string& demangled_name (function_name);
#endif

#if defined (__APPLE__)
        what << line.substr (0, line.rfind ("0x", start_pos - 1))
             << demangled_name
             << ": "
             << line.substr (plus_pos + 3); // address in function
#elif defined (__linux__)
        // [address] name + offset
        what << line.substr ( address_pos
                            , line.find ("]", address_pos)
                            - address_pos + 1
                            )
             << " "
             << demangled_name;
        if (plus_pos != std::string::npos)
        {
          what << " + " << line.substr ( name_end_pos + 1
                                       , end_pos - name_end_pos - 1
                                       );
        }
#else
        what << line;
#endif
        what << std::endl;
      }

      if (size == nframes)
      {
        what << "...maybe more" << std::endl;
      }

      free (strings);

      return what.str();
    }
#else
    std::string make_backtrace (const std::string& reason)
    {
      return reason;
    }
#endif

    backtracing_exception::backtracing_exception (const std::string& reason)
      : std::runtime_error (make_backtrace (reason))
    {
    }
  }
}
