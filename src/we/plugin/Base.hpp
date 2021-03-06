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
#include <we/type/value.hpp>

#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace gspc
{
  namespace we
  {
    namespace plugin
    {
      using Context = ::expr::eval::context;
      using PlaceName = std::string;
      using Value = ::pnet::type::value::value_type;

      //! It is safe to use a temporary for the place name.
      //! Calls to put_token will not block, however, the execution
      //! will happen asynchronously.
      using PutToken = std::function<void (PlaceName const&, Value)>;

      //! All methods including constructor and destructor shall not
      //! block.
      struct Base
      {
        //! executed after expression has been evaluated
        //! The callback is guaranteed to be valid during object life
        //! time.
        //! Plugin::Plugin (Context const&, PutToken);

        //! executed after expression input has been bound
        virtual void before_eval (Context const&) = 0;

        //! executed after expression has been evaluated
        virtual void after_eval (Context const&) = 0;

        virtual ~Base() = default;
      };
    }
  }
}

#if defined(__clang__)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
#endif

#define GSPC_WE_PLUGIN_CREATE_SIGNATURE()                                   \
  extern "C" [[gnu::visibility ("default")]]                                \
    std::unique_ptr<::gspc::we::plugin::Base>                               \
      gspc_we_plugin_create ( ::gspc::we::plugin::Context const& context    \
                            , ::gspc::we::plugin::PutToken put_token        \
                            )

GSPC_WE_PLUGIN_CREATE_SIGNATURE();

#define GSPC_WE_PLUGIN_CREATE(_plugin)                  \
  GSPC_WE_PLUGIN_CREATE_SIGNATURE()                     \
  {                                                     \
    return std::unique_ptr<::gspc::we::plugin::Base>    \
      (new _plugin (context, std::move (put_token)));   \
  }

#if defined(__clang__)
  #pragma clang diagnostic pop
#endif
