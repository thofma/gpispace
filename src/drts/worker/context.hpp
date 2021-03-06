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

#include <drts/worker/context_fwd.hpp>

#include <functional>
#include <set>
#include <stdexcept>
#include <string>

namespace drts
{
  namespace worker
  {
    class redirect_output;
    class context_constructor;

    void throw_cancelled();
    void on_signal_unexpected (int);
    void on_exit_unexpected (int);

    class context
    {
    private:
      friend class redirect_output;
      friend class context_constructor;
      class implementation;
      implementation* _;

    public:
      context (context_constructor);
      ~context();

      std::string const& worker_name() const;

      std::set<std::string> const& workers() const;
      std::string worker_to_hostname (std::string const&) const;

      void module_call_do_cancel() const;

      void execute_and_kill_on_cancel
        ( std::function<void()> fun
        , std::function<void()> on_cancel
        , std::function<void (int)> on_signal
        , std::function<void (int)> on_exit
        );

      void execute_and_kill_on_cancel
        ( std::function<void()> fun
        , std::function<void()> on_cancel
        )
      {
        execute_and_kill_on_cancel
          ( fun
          , on_cancel
          , &on_signal_unexpected
          , &on_exit_unexpected
          );
      }

      struct cancelled : public std::exception {};

      void execute_and_kill_on_cancel (std::function<void()> fun)
      {
        execute_and_kill_on_cancel
          ( fun
          , &throw_cancelled
          );
      }
    };
  }
}
