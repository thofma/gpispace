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

#include <sdpa/events/BacklogNoLongerFullEvent.hpp>
#include <sdpa/events/ErrorEvent.hpp>
#include <sdpa/events/JobFinishedEvent.hpp>
#include <sdpa/events/SubmitJobAckEvent.hpp>
#include <sdpa/events/SubmitJobEvent.hpp>
#include <sdpa/test/sdpa/utils.hpp>
#include <sdpa/types.hpp>

#include <test/certificates_data.hpp>

#include <we/type/activity.hpp>

#include <fhg/util/thread/event.hpp>
#include <util-generic/testing/flatten_nested_exceptions.hpp>
#include <util-generic/testing/printer/optional.hpp>

#include <functional>
#include <string>

#include <boost/test/data/monomorphic.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>

namespace
{
  class fake_drts_worker_reporting_backlog_full final
    : public utils::no_thread::fake_drts_worker_waiting_for_finished_ack
  {
  public:
    fake_drts_worker_reporting_backlog_full
      ( std::function<void (std::string)> announce_job
      , utils::agent const& master
      , fhg::com::Certificates const& certificates
      )
      : utils::no_thread::fake_drts_worker_waiting_for_finished_ack
          (announce_job, master, certificates)
    {}

    void report_backlog_full (std::string name)
    {
      const job_t job (_jobs.at (name));
      _jobs.erase (name);

      _network.perform<sdpa::events::ErrorEvent>
        ( job._owner
        , sdpa::events::ErrorEvent::SDPA_EBACKLOGFULL
        , "Cannot accept this job, my backlog is full!"
        , job._id
        );
    }

    void report_can_take_jobs()
    {
      _network.perform<sdpa::events::BacklogNoLongerFullEvent> (_master.get());
    }

  private:
    basic_drts_component::event_thread_and_worker_join _ = {*this};
  };
}

BOOST_DATA_TEST_CASE
  ( one_worker_reports_backlog_full_the_other_two_receive_cancellation_requests
  , certificates_data
  , certificates
  )
{
  const utils::agent agent (certificates);

  utils::client client (agent, certificates);
  client.submit_job (utils::net_with_one_child_requiring_workers (3));

  fhg::util::thread::event<std::string> job_submitted_1;

  fake_drts_worker_reporting_backlog_full worker_1
    ( [&job_submitted_1] (std::string j){job_submitted_1.notify (j);}
    , agent
    , certificates
    );

  fhg::util::thread::event<std::string> job_submitted_2;
  fhg::util::thread::event<std::string> cancel_requested_2;
  utils::fake_drts_worker_notifying_cancel worker_2
    ( [&job_submitted_2] (std::string j) { job_submitted_2.notify (j);}
    , [&cancel_requested_2] (std::string j) { cancel_requested_2.notify (j); }
    , agent
    , certificates
    );

  fhg::util::thread::event<std::string> job_submitted_3;
  fhg::util::thread::event<std::string> cancel_requested_3;
  utils::fake_drts_worker_notifying_cancel worker_3
    ( [&job_submitted_3] (std::string j) { job_submitted_3.notify (j);}
    , [&cancel_requested_3] (std::string j) { cancel_requested_3.notify (j); }
    , agent
    , certificates
    );

  const std::string job_name (job_submitted_1.wait());
  worker_1.report_backlog_full (job_name);

  std::string const job_name_2 (job_submitted_2.wait());
  std::string const job_name_3 (job_submitted_3.wait());

  sdpa::job_id_t const job_id_2 (worker_2.job_id (job_name_2));
  sdpa::job_id_t const job_id_3 (worker_3.job_id (job_name_3));

  BOOST_REQUIRE_EQUAL (cancel_requested_2.wait(), job_id_2);
  BOOST_REQUIRE_EQUAL (cancel_requested_3.wait(), job_id_3);
}

BOOST_DATA_TEST_CASE
  ( one_worker_reports_backlog_full_the_still_running_task_is_cancelled
  , certificates_data
  , certificates
  )
{
  const utils::agent agent (certificates);

  utils::client client (agent, certificates);
  client.submit_job (utils::net_with_one_child_requiring_workers (3));

  fhg::util::thread::event<std::string> job_submitted_1;
  utils::fake_drts_worker_waiting_for_finished_ack worker_1
    ([&job_submitted_1] (std::string j) { job_submitted_1.notify (j); }, agent, certificates);

  fhg::util::thread::event<std::string> job_submitted_2;
  fake_drts_worker_reporting_backlog_full worker_2
    ( [&job_submitted_2] (std::string j){job_submitted_2.notify (j);}
    , agent
    , certificates
    );

  fhg::util::thread::event<std::string> job_submitted_3;
  fhg::util::thread::event<std::string> cancel_requested_3;
  utils::fake_drts_worker_notifying_cancel worker_3
    ( [&job_submitted_3] (std::string j) { job_submitted_3.notify (j);}
    , [&cancel_requested_3] (std::string j) { cancel_requested_3.notify (j); }
    , agent
    , certificates
    );

  const std::string job_name (job_submitted_1.wait());
  job_submitted_2.wait();
  job_submitted_3.wait();

  worker_1.finish_and_wait_for_ack (job_name);
  worker_2.report_backlog_full (job_name);

  BOOST_REQUIRE_EQUAL (cancel_requested_3.wait(), worker_3.job_id (job_name));
}

BOOST_DATA_TEST_CASE
  ( worker_reporting_backlog_full_can_take_new_tasks_again_after_reporting_backlog_no_longer_full
  , certificates_data
  , certificates
  )
{
  const utils::agent agent (certificates);

  utils::client client (agent, certificates);
  client.submit_job (utils::net_with_one_child_requiring_workers (2));

  fhg::util::thread::event<std::string> job_submitted_1;

  fake_drts_worker_reporting_backlog_full worker_1
    ( [&job_submitted_1] (std::string j){job_submitted_1.notify (j);}
    , agent
    , certificates
    );

  fhg::util::thread::event<std::string> job_submitted_2;
  fhg::util::thread::event<std::string> cancel_requested_2;
  utils::fake_drts_worker_notifying_cancel worker_2
    ( [&job_submitted_2] (std::string j) { job_submitted_2.notify (j);}
    , [&cancel_requested_2] (std::string j) { cancel_requested_2.notify (j); }
    , agent
    , certificates
    );

  const std::string job_name_1 (job_submitted_1.wait());
  BOOST_REQUIRE_EQUAL (job_name_1, job_submitted_2.wait());
  worker_1.report_backlog_full (job_name_1);

  sdpa::job_id_t const job_id (worker_2.job_id (job_name_1));
  BOOST_REQUIRE_EQUAL (cancel_requested_2.wait(), job_id);
  worker_2.canceled (job_id);

  worker_1.report_can_take_jobs();

  const std::string job_name_2 (job_submitted_1.wait());
  BOOST_REQUIRE_EQUAL (job_name_2, job_submitted_2.wait());

  worker_1.finish_and_wait_for_ack (job_name_2);
  worker_2.finish_and_wait_for_ack (job_name_2);
}
