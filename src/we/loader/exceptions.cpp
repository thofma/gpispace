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

#include <we/loader/exceptions.hpp>

#include <util-generic/hash/boost/filesystem/path.hpp>
#include <util-generic/print_container.hpp>

#include <boost/format.hpp>

#include <algorithm>
#include <iterator>

namespace we
{
  namespace loader
  {
    module_load_failed::module_load_failed
        ( boost::filesystem::path const& file
        , std::string const& reason
        )
      : std::runtime_error
          ( ( boost::format ("could not load module '%1%': %2%")
            % file
            % reason
            ).str()
          )
    {}

    module_not_found::module_not_found
        ( boost::filesystem::path const& file
        , std::string const& search_path
        )
      : std::runtime_error
          ( ( boost::format ("module '%1%' not found in '%2%'")
            % file
            % search_path
            ).str()
          )
    {}

    function_not_found::function_not_found
        ( boost::filesystem::path const& module
        , std::string const& name
        )
      : std::runtime_error
          ( ( boost::format ("function %1%::%2% not found")
            % module
            % name
            ).str()
          )
    {}

    duplicate_function::duplicate_function
        ( boost::filesystem::path const& module
        , std::string const& name
        )
      : std::runtime_error
          ( ( boost::format ("duplicate function %1%::%2%")
            % module
            % name
            ).str()
          )
    {}

    namespace
    {
      template<typename T>
        std::vector<T> left_overs (std::vector<T> lhs, std::vector<T> rhs)
      {
        std::sort (lhs.begin(), lhs.end());
        std::sort (rhs.begin(), rhs.end());
        std::vector<T> diff;
        std::set_difference
          ( lhs.begin(), lhs.end(), rhs.begin(), rhs.end()
          , std::inserter (diff, diff.begin())
          );
        return diff;
      }
    }

    module_does_not_unload::module_does_not_unload
        ( boost::filesystem::path module
        , std::vector<boost::filesystem::path> before
        , std::vector<boost::filesystem::path> after
        )
      : module_does_not_unload (module, left_overs (after, before))
    {}
    module_does_not_unload::module_does_not_unload
        ( boost::filesystem::path module
        , std::vector<boost::filesystem::path> left_over
        )
      : std::runtime_error
          ( ( boost::format ( "module '%1%' does not properly unload on dlclose"
                              ", leaking %2% loaded in the process"
                            )
            % module
            % fhg::util::print_container ("{", ", ", "}", left_over)
            ).str()
          )
    {}

    function_does_not_unload::function_does_not_unload
        ( std::string module
        , std::string name
        , std::vector<boost::filesystem::path> before
        , std::vector<boost::filesystem::path> after
        )
      : function_does_not_unload
          (module, name, left_overs (after, before))
    {}
    function_does_not_unload::function_does_not_unload
        ( std::string module
        , std::string name
        , std::vector<boost::filesystem::path> left_over
        )
      : std::runtime_error
          ( ( boost::format ( "function %1%::%2% dynamically opened libraries "
                              "but did not properly unload, leaking %3% "
                              "loaded in the process"
                            )
            % module
            % name
            % fhg::util::print_container ("{", ", ", "}", left_over)
            ).str()
          )
    {}
  }
}
