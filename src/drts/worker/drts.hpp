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

#include <drts/private/scoped_allocation.hpp>

#include <fhg/util/thread/bounded_queue.hpp>
#include <fhg/util/thread/queue.hpp>

#include <fhgcom/peer.hpp>
#include <logging/stream_emitter.hpp>

#include <gpi-space/pc/client/api.hpp>

#include <util-generic/threadsafe_queue.hpp>

#include <sdpa/daemon/NotificationEvent.hpp>
#include <sdpa/events/EventHandler.hpp>
#include <sdpa/events/SDPAEvent.hpp>

#include <we/loader/loader.hpp>

#include <boost/asio/io_service.hpp>
#include <boost/thread/scoped_thread.hpp>

#include <atomic>
#include <functional>
#include <future>
#include <map>
#include <mutex>
#include <string>
#include <vector>

struct wfe_task_t;

class DRTSImpl final : public sdpa::events::EventHandler
{
  using masters_t = std::vector<fhg::com::p2p::address_t>;

public:
  class Job
  {
  public:
    enum state_t
    {
      PENDING
    , RUNNING
    , FINISHED
    , FAILED
    , CANCELED
    , CANCELED_DUE_TO_WORKER_SHUTDOWN
    };

    using owner_type = masters_t::const_iterator;

    Job ( std::string const& jobid
        , we::type::activity_t const& activity_
        , boost::optional<std::string> const& target_impl_
        , owner_type const& owner_
        , std::set<std::string> const& workers_
        )
      : id (jobid)
      , activity (activity_)
      , target_impl (target_impl_)
      , owner (owner_)
      , workers (workers_)
      , state (Job::PENDING)
      , result()
      , message ("")
    {}

    std::string const id;
    we::type::activity_t activity;
    boost::optional<std::string> const target_impl;
    owner_type const owner;
    std::set<std::string> const workers;
    std::atomic<state_t> state;
    we::type::activity_t result;
    std::string message;
  };

private:
  typedef std::map<std::string, std::shared_ptr<DRTSImpl::Job>> map_of_jobs_t;

public:

  using master_info = std::tuple<fhg::com::host_t, fhg::com::port_t>;

  DRTSImpl
    ( std::function<void()> request_stop
    , std::unique_ptr<boost::asio::io_service> peer_io_service
    , std::string const& kernel_name
    , unsigned short comm_port
    , gpi::pc::client::api_t /*const*/* virtual_memory_socket
    , gspc::scoped_allocation /*const*/* shared_memory
    , std::vector<master_info> const& masters
    , std::vector<std::string> const& capability_names
    , std::vector<boost::filesystem::path> const& library_path
    , std::size_t backlog_length
    , fhg::logging::stream_emitter& log_emitter
    , fhg::com::Certificates const& certificates
    );
  ~DRTSImpl();

  virtual void handle_worker_registration_response
    (fhg::com::p2p::address_t const& source, const sdpa::events::worker_registration_response *e) override;
  virtual void handleSubmitJobEvent
    (fhg::com::p2p::address_t const& source, const sdpa::events::SubmitJobEvent *e) override;
  virtual void handleCancelJobEvent
    (fhg::com::p2p::address_t const& source, const sdpa::events::CancelJobEvent *e) override;
  virtual void handleJobFailedAckEvent
    (fhg::com::p2p::address_t const& source, const sdpa::events::JobFailedAckEvent *e) override;
  virtual void handleJobFinishedAckEvent
    (fhg::com::p2p::address_t const& source, const sdpa::events::JobFinishedAckEvent *e) override;
  virtual void handleDiscoverJobStatesEvent
    (fhg::com::p2p::address_t const& source, const sdpa::events::DiscoverJobStatesEvent*) override;

private:
  void event_thread();
  void job_execution_thread();

  void start_receiver();

  template<typename Event, typename... Args>
    void send_event (fhg::com::p2p::address_t const& destination, Args&&... args);

  std::function<void()> _request_stop;

  bool m_shutting_down;

  std::string m_my_name;

  mutable std::mutex _currently_executed_tasks_mutex;
  std::map<std::string, wfe_task_t *> _currently_executed_tasks;

  we::loader::loader m_loader;

  fhg::logging::stream_emitter& _log_emitter;
  void emit_gantt (wfe_task_t const&, sdpa::daemon::NotificationEvent::state_t);

  gpi::pc::client::api_t /*const*/* _virtual_memory_api;
  gspc::scoped_allocation /*const*/* _shared_memory;

  //! \todo Two sets for connected and unconnected masters?
  masters_t m_masters;

  fhg::util::interruptible_threadsafe_queue<std::pair< fhg::com::p2p::address_t
                                                     , sdpa::events::SDPAEvent::Ptr
                                                     >
                                           > m_event_queue;

  mutable std::mutex m_job_map_mutex;

  map_of_jobs_t m_jobs;

  fhg::util::interruptible_bounded_threadsafe_queue<std::shared_ptr<DRTSImpl::Job>>
    m_pending_jobs;

  mutable std::mutex _guard_backlogfull_notified_masters;
  std::unordered_set<fhg::com::p2p::address_t> _masters_backlogfull_notified;

  fhg::com::peer_t _peer;

  boost::strict_scoped_thread<> m_event_thread;
  decltype (m_event_queue)::interrupt_on_scope_exit _interrupt_event_thread;
  boost::strict_scoped_thread<> m_execution_thread;
  decltype (m_pending_jobs)::interrupt_on_scope_exit _interrupt_execution_thread;

  std::unordered_map<fhg::com::p2p::address_t, std::promise<void>>
    _registration_responses;

  struct mark_remaining_tasks_as_canceled_helper
  {
    ~mark_remaining_tasks_as_canceled_helper();

    std::mutex& _currently_executed_tasks_mutex;
    std::map<std::string, wfe_task_t *>& _currently_executed_tasks;
    std::mutex& _jobs_guard;
    map_of_jobs_t& _jobs;
  } _mark_remaining_tasks_as_canceled_helper
    = { _currently_executed_tasks_mutex, _currently_executed_tasks
      , m_job_map_mutex, m_jobs
      };
};
