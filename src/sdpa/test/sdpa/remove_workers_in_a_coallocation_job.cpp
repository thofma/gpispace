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

#include <sdpa/test/sdpa/utils.hpp>
#include <sdpa/types.hpp>

#include <test/certificates_data.hpp>

#include <fhg/util/thread/event.hpp>
#include <util-generic/testing/flatten_nested_exceptions.hpp>
#include <util-generic/testing/printer/optional.hpp>

#include <boost/test/data/monomorphic.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>

#include <string>

BOOST_DATA_TEST_CASE
  ( remove_workers_in_a_coallocation_job_and_add_them_again
  , certificates_data
  , certificates
  )
{
  utils::agent const agent (certificates);

  utils::client client (agent, certificates);

  sdpa::job_id_t const job_id
    (client.submit_job (utils::net_with_one_child_requiring_workers (2)));

  {
    fhg::util::thread::event<std::string> submitted_0;
    utils::fake_drts_worker_waiting_for_finished_ack worker_0
      ([&] (std::string s) { submitted_0.notify (s); }, agent, certificates);

    fhg::util::thread::event<std::string> submitted_1;
    utils::fake_drts_worker_notifying_module_call_submission worker_1
      ([&] (std::string s) { submitted_1.notify (s); }, agent, certificates);

    worker_0.finish_and_wait_for_ack (submitted_0.wait());
    submitted_1.wait();
  }

  {
    fhg::util::thread::event<std::string> submitted_0;
    utils::fake_drts_worker_waiting_for_finished_ack worker_0
         ([&] (std::string s) { submitted_0.notify (s); }, agent, certificates);

    fhg::util::thread::event<std::string> submitted_1;
    utils::fake_drts_worker_waiting_for_finished_ack worker_1
      ([&] (std::string s) { submitted_1.notify (s); }, agent, certificates);

    worker_0.finish_and_wait_for_ack (submitted_0.wait());
    worker_1.finish_and_wait_for_ack (submitted_1.wait());
  }

  BOOST_REQUIRE_EQUAL ( client.wait_for_terminal_state_and_cleanup (job_id)
                      , sdpa::status::FINISHED
                      );
}
