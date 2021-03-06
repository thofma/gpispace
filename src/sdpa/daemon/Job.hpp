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

#include <sdpa/job_states.hpp>
#include <sdpa/master_network_info.hpp>
#include <sdpa/requirements_and_preferences.hpp>
#include <sdpa/types.hpp>

#include <we/type/activity.hpp>
#include <we/type/net.hpp>

#include <fhg/assert.hpp>

#include <boost/msm/back/state_machine.hpp>
#include <boost/msm/front/state_machine_def.hpp>
#include <boost/optional.hpp>

#include <mutex>

namespace sdpa
{
  namespace daemon
  {
    // front-end: define the FSM structure
    struct JobFSM_ : public boost::msm::front::state_machine_def<JobFSM_>
    {
      virtual ~JobFSM_() = default;

      struct s_pending : public boost::msm::front::state<>{};
      struct s_running : public boost::msm::front::state<>{};
      struct s_finished : public boost::msm::front::state<>
      {
        we::type::activity_t result;
        s_finished() = default;
        s_finished (we::type::activity_t result_)
          : result (std::move (result_))
        {}
      };
      struct s_failed : public boost::msm::front::state<>
      {
        std::string message;
        s_failed() = default;
        s_failed (std::string message_)
          : message (std::move (message_))
        {}
      };
      struct s_canceling : public boost::msm::front::state<>{};
      struct s_canceled : public boost::msm::front::state<>{};

      struct e_begin_cancel {};
      struct e_canceled {};
      struct e_dispatch {};
      struct e_failed {};
      struct e_finished {};
      struct e_reschedule {};

      typedef s_pending initial_state;

      struct transition_table : boost::mpl::vector
        <
        //     Start         Event              Next
        //   +-------------+------------------+-------------+
         _row< s_pending   , e_begin_cancel   , s_canceling >,
         _row< s_pending   , e_dispatch       , s_running   >,
         _row< s_pending   , e_failed         , s_failed    >,
         _row< s_pending   , e_finished       , s_finished  >,
        _irow< s_pending   , e_reschedule   /*, ignore */   >,
        //   +-------------+------------------+-------------+
         _row< s_running   , e_begin_cancel   , s_canceling >,
        _irow< s_running   , e_dispatch     /*, ignore */   >,
         _row< s_running   , e_failed         , s_failed    >,
         _row< s_running   , e_finished       , s_finished  >,
         _row< s_running   , e_reschedule     , s_pending   >,
        //   +-------------+------------------+-------------+
        _irow< s_finished  , e_finished     /*, ignore */   >,
        //   +-------------+------------------+-------------+
        _irow< s_failed    , e_failed       /*, ignore */   >,
        //   +-------------+------------------+-------------+
        _irow< s_canceling , e_begin_cancel /*, ignore */   >,
         _row< s_canceling , e_canceled       , s_canceled  >,
        _irow< s_canceling , e_dispatch     /*, ignore */   >,
         _row< s_canceling , e_failed         , s_canceled  >,
         _row< s_canceling , e_finished       , s_canceled  >,
        //   +-------------+------------------+-------------+
        _irow< s_canceled  , e_canceled     /*, ignore */   >,
        _irow< s_canceled  , e_failed       /*, ignore */   >,
        _irow< s_canceled  , e_finished     /*, ignore */   >
        >{};

      //! \note This table refers to the order in which states are
      //! first seen in the state machine definition. This is hacky
      //! and should be removed / done via visitors.
      static status::code state_code (size_t state)
      {
        static status::code const state_codes[] =
          { status::PENDING
          , status::RUNNING
          , status::FINISHED
          , status::FAILED
          , status::CANCELING
          , status::CANCELED
          };
        fhg_assert ( state < sizeof (state_codes) / sizeof (*state_codes)
                   , "index shall be valid"
                   );
        return state_codes[state];
      }

      template <class FSM, class Event>
      void no_transition(Event const& e, FSM&, int state)
      {
        throw std::runtime_error ( "no transition from state "
                                 + status::show(state_code(state))
                                 + " on event "
                                 + typeid(e).name()
                                 );
      }
    };

    //! \todo actually use in statemachine, currently used by
    //! CoallocationScheduler::Reservation only
    using terminal_state = boost::variant < JobFSM_::s_finished
                                          , JobFSM_::s_failed
                                          , JobFSM_::s_canceled
                                          >;
    struct job_result_type
    {
      std::map<sdpa::worker_id_t, terminal_state> individual_results;
      //! \todo is is correct to only take the last success as the
      //! global result? some kind of reduce instead?
      JobFSM_::s_finished last_success;
    };

    using Implementation = boost::optional<std::string>;

    struct job_source_wfe {};
    struct job_source_master
    {
      master_info_t::iterator _;
    };
    struct job_source_client {};
    using job_source = boost::variant < job_source_wfe
                                      , job_source_master
                                      , job_source_client
                                      >;

    struct job_handler_wfe {};
    struct job_handler_worker {};
    using job_handler = boost::variant<job_handler_wfe, job_handler_worker>;

    class Job : public boost::msm::back::state_machine<JobFSM_>
    {
    public:
      Job ( const job_id_t id
          , we::type::activity_t
          , job_source
          , job_handler
          , Requirements_and_preferences
          );

      we::type::activity_t const& activity() const
      {
        return _activity;
      }
      const job_id_t& id() const;
      job_source const& source() const;
      job_handler const& handler() const { return _handler; }
      Requirements_and_preferences requirements_and_preferences() const;

      std::string error_message () const;
      const we::type::activity_t& result() const;

      status::code getStatus() const;

      void CancelJob();
      void CancelJobAck();
      void Dispatch();
      void JobFailed (std::string error_message);
      void JobFinished (we::type::activity_t);
      void Reschedule();

    private:
      mutable std::mutex mtx_;
      we::type::activity_t _activity;
      job_id_t id_;
      job_source _source;
      job_handler _handler;
      Requirements_and_preferences _requirements_and_preferences;

      std::string m_error_message;
      we::type::activity_t result_;
    };
}}
