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

#include <we/expr/eval/context.hpp>
#include <we/loader/loader.hpp>
#include <we/type/module_call.hpp>

#include <gpi-space/pc/client/api.hpp>
#include <drts/private/scoped_allocation.hpp>

namespace we
{
  namespace loader
  {
    expr::eval::context module_call
      ( we::loader::loader& loader
      , gpi::pc::client::api_t /*const*/*
      , gspc::scoped_allocation /*const*/*
      , drts::worker::context* context
      , expr::eval::context const& input
      , const we::type::module_call_t& module_call
      );
  }
}
