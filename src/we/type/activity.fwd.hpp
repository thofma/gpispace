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

#include <we/type/id.hpp>
#include <we/type/value.hpp>

#include <utility>
#include <vector>

namespace we
{
    namespace type
    {
      class activity_t;

      using TokenOnPort = std::pair< pnet::type::value::value_type
                                   , we::port_id_type
                                   >;
      using TokensOnPorts = std::vector<TokenOnPort>;
    }
}
