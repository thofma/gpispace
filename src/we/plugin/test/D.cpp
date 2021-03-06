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

#include <we/plugin/Base.hpp>

#include <util-generic/unreachable.hpp>

#include <stdexcept>

struct D : public gspc::we::plugin::Base
{
  D (::gspc::we::plugin::Context const&, ::gspc::we::plugin::PutToken)
  {
    throw std::runtime_error ("D::D()");
  }

  virtual void before_eval (::gspc::we::plugin::Context const&) override
  {
    FHG_UTIL_UNREACHABLE();
  }
  virtual void after_eval (::gspc::we::plugin::Context const&) override
  {
    FHG_UTIL_UNREACHABLE();
  }
};

GSPC_WE_PLUGIN_CREATE (D)
