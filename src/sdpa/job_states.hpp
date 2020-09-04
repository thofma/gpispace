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

#include <fhg/util/macros.hpp>

#include <string>

namespace sdpa
{
  namespace status
  {
    enum code
      {
        // terminal states
        FINISHED
      , FAILED
      , CANCELED

        // non-terminal states
      , PENDING
      , RUNNING
      , CANCELING
      };

    inline bool is_terminal (code c)
    {
      return c == FINISHED || c == FAILED || c == CANCELED;
    }

    inline std::string show (status::code code)
    {
      switch (code)
      {
      case PENDING:
        return "SDPA::Pending";
      case RUNNING:
        return "SDPA::Running";
      case FINISHED:
        return "SDPA::Finished";
      case FAILED:
        return "SDPA::Failed";
      case CANCELED:
        return "SDPA::Canceled";
      case CANCELING:
        return "SDPA::Canceling";
      }

      INVALID_ENUM_VALUE (status::code, code);
    }
  }
}
