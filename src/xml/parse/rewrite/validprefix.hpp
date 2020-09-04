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

#include <fhg/util/starts_with.hpp>

namespace rewrite
{
  static std::string magic_prefix = "_";

  inline bool has_magic_prefix (const std::string & name)
  {
    return fhg::util::starts_with (magic_prefix, name);
  }

  inline std::string mk_prefix (const std::string & name)
  {
    return magic_prefix + name + magic_prefix;
  }
}