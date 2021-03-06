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

#include <sdpa/events/MgmtEvent.hpp>
#include <sdpa/capability.hpp>

#include <boost/optional.hpp>

namespace sdpa
{
  namespace events
  {
    class WorkerRegistrationEvent : public MgmtEvent
    {
    public:
      typedef boost::shared_ptr<WorkerRegistrationEvent> Ptr;

      WorkerRegistrationEvent
        ( std::string const& name
        , const capabilities_set_t& cpbset
        , const unsigned long allocated_shared_memory_size
        , bool children_allowed
        , const std::string& hostname
        )
          : MgmtEvent()
          , _name (name)
          , cpbset_ (cpbset)
          , allocated_shared_memory_size_ (allocated_shared_memory_size)
          , children_allowed_(children_allowed)
          , hostname_(hostname)
      {}

      std::string const& name() const
      {
        return _name;
      }
      const capabilities_set_t& capabilities() const
      {
        return cpbset_;
      }

      const std::string& hostname() const
      {
        return hostname_;
      }

      const bool& children_allowed() const
      {
	return children_allowed_;
      }

      const unsigned long& allocated_shared_memory_size() const
      {
        return allocated_shared_memory_size_;
      }

      virtual void handleBy
        (fhg::com::p2p::address_t const& source, EventHandler* handler) override
      {
        handler->handleWorkerRegistrationEvent (source, this);
      }

    private:
      std::string _name;
      capabilities_set_t cpbset_;
      unsigned long allocated_shared_memory_size_;
      bool children_allowed_;
      std::string hostname_;
    };

    SAVE_CONSTRUCT_DATA_DEF (WorkerRegistrationEvent, e)
    {
      SAVE_MGMTEVENT_CONSTRUCT_DATA (e);
      SAVE_TO_ARCHIVE (e->name());
      SAVE_TO_ARCHIVE (e->capabilities());
      SAVE_TO_ARCHIVE (e->allocated_shared_memory_size());
      SAVE_TO_ARCHIVE (e->children_allowed());
      SAVE_TO_ARCHIVE (e->hostname());
    }

    LOAD_CONSTRUCT_DATA_DEF (WorkerRegistrationEvent, e)
    {
      LOAD_MGMTEVENT_CONSTRUCT_DATA();
      LOAD_FROM_ARCHIVE (std::string, name);
      LOAD_FROM_ARCHIVE (capabilities_set_t, cpbset);
      LOAD_FROM_ARCHIVE (unsigned long, allocated_shared_memory_size);
      LOAD_FROM_ARCHIVE (bool, children_allowed);
      LOAD_FROM_ARCHIVE (std::string, hostname);

      ::new (e) WorkerRegistrationEvent ( name
                                        , cpbset
                                        , allocated_shared_memory_size
                                        , children_allowed
                                        , hostname
                                        );
    }
  }
}
