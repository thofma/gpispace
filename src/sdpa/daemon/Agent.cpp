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

#include <sdpa/daemon/Agent.hpp>
#include <sdpa/daemon/Job.hpp>
#include <sdpa/events/CancelJobEvent.hpp>
#include <sdpa/events/CapabilitiesGainedEvent.hpp>
#include <sdpa/events/CapabilitiesLostEvent.hpp>
#include <sdpa/events/DiscoverJobStatesEvent.hpp>
#include <sdpa/events/DiscoverJobStatesReplyEvent.hpp>
#include <sdpa/events/ErrorEvent.hpp>
#include <sdpa/events/JobStatusReplyEvent.hpp>
#include <sdpa/events/QueryJobStatusEvent.hpp>
#include <sdpa/events/SubscribeAckEvent.hpp>
#include <sdpa/events/delayed_function_call.hpp>
#include <sdpa/events/put_token.hpp>
#include <sdpa/events/workflow_response.hpp>
#include <sdpa/id_generator.hpp>

#include <fhg/util/boost/optional.hpp>
#include <fhg/util/macros.hpp>
#include <util-generic/cxx14/make_unique.hpp>
#include <util-generic/fallthrough.hpp>
#include <util-generic/functor_visitor.hpp>
#include <util-generic/hostname.hpp>
#include <util-generic/join.hpp>
#include <util-generic/print_exception.hpp>

#include <boost/range/adaptor/map.hpp>
#include <boost/tokenizer.hpp>

#include <algorithm>
#include <chrono>
#include <functional>
#include <sstream>
#include <thread>

namespace sdpa
{
  namespace daemon
  {
    namespace
    {
      std::vector<std::string> require_proper_url (std::string url)
      {
        const boost::tokenizer<boost::char_separator<char>> tok
          (url, boost::char_separator<char> (":"));

        const std::vector<std::string> vec (tok.begin(), tok.end());

        if (vec.empty() || vec.size() > 2)
        {
          throw std::runtime_error ("configuration of network failed: invalid url: has to be of format 'host[:port]'");
        }

        return vec;
      }

      fhg::com::host_t host_from_url (std::string url)
      {
        return fhg::com::host_t (require_proper_url (url)[0]);
      }
      fhg::com::port_t port_from_url (std::string url)
      {
        const std::vector<std::string> vec (require_proper_url (url));
        return fhg::com::port_t (vec.size() == 2 ? vec[1] : "0");
      }
    }

    Agent::Agent
        ( const std::string name
        , const std::string url
        , std::unique_ptr<boost::asio::io_service> peer_io_service
        , boost::optional<boost::filesystem::path> const& vmem_socket
        , master_info_t masters
        , bool create_wfe
        , fhg::com::Certificates const& certificates
        )
      : _name (name)
      , _master_info (std::move (masters))
      , _subscriptions()
      , _discover_sources()
      , _job_map_mutex()
      , job_map_()
      , _cleanup_job_map_on_dtor_helper (job_map_)
      , _worker_manager()
      , _scheduler ( [this] (job_id_t job_id)
                     {
                       return findJob (job_id)->requirements_and_preferences();
                     }
                   , _worker_manager
                   )
      , _cancel_mutex()
      , _scheduling_requested_guard()
      , _scheduling_requested_condition()
      , _scheduling_requested (false)
      , _random_extraction_engine
        (boost::make_optional (create_wfe, std::mt19937 (std::random_device()())))
      , mtx_subscriber_()
      , mtx_cpb_()
      , _log_emitter()
      , _registration_timeout (std::chrono::seconds (1))
      , _event_queue()
      , _network_strategy ( [this] ( fhg::com::p2p::address_t const& source
                                   , events::SDPAEvent::Ptr const& e
                                   )
                          {
                            _event_queue.put (source, e);
                          }
                          , std::move (peer_io_service)
                          , host_from_url (url)
                          , port_from_url (url)
                          , certificates
                          )
      , ptr_workflow_engine_
          ( create_wfe
          ? new we::layer
              ( std::bind (&Agent::submit, this, std::placeholders::_1, std::placeholders::_2)
              , std::bind (&Agent::cancel, this, std::placeholders::_1)
              , std::bind (&Agent::finished, this, std::placeholders::_1, std::placeholders::_2)
              , std::bind (&Agent::failed, this, std::placeholders::_1, std::placeholders::_2)
              , std::bind (&Agent::canceled, this, std::placeholders::_1)
              , std::bind (&Agent::discover, this, std::placeholders::_1, std::placeholders::_2)
              , std::bind (&Agent::discovered, this, std::placeholders::_1, std::placeholders::_2)
              , std::bind (&Agent::token_put, this, std::placeholders::_1, std::placeholders::_2)
              , std::bind (&Agent::workflow_response_response, this, std::placeholders::_1, std::placeholders::_2)
              , std::bind (&Agent::gen_id, this)
              , *_random_extraction_engine
              )
          : nullptr
          )
      , _registration_threads()
      , _scheduling_thread (&Agent::scheduling_thread, this)
      , _interrupt_scheduling_thread
          ( [this]
            {
              std::lock_guard<std::mutex> const _ (_scheduling_requested_guard);
              _scheduling_interrupted = true;
              _scheduling_requested_condition.notify_one();
            }
          )
      , _virtual_memory_api
        ( vmem_socket
        ? fhg::util::cxx14::make_unique<gpi::pc::client::api_t>
            (_log_emitter, vmem_socket->string())
        : nullptr
        )
      , _event_handler_thread (&Agent::handle_events, this)
      , _interrupt_event_queue (_event_queue)
    {
      for (master_network_info& master : _master_info)
      {
        requestRegistration (master);
      }
    }

    Agent::cleanup_job_map_on_dtor_helper::cleanup_job_map_on_dtor_helper
        (job_map_t& m)
      : _ (m)
    {}

    Agent::cleanup_job_map_on_dtor_helper::~cleanup_job_map_on_dtor_helper()
    {
      for (const Job* const pJob : _ | boost::adaptors::map_values )
      {
        delete pJob;
      }
    }

    const std::string& Agent::name() const
    {
      return _name;
    }

    boost::asio::ip::tcp::endpoint Agent::peer_local_endpoint() const
    {
      return _network_strategy.local_endpoint();
    }

    void Agent::serveJob
      ( WorkerSet const& workers
      , Implementation const& implementation
      , const job_id_t& jobId
      )
    {
      Job const* const ptrJob = findJob (jobId);
      if (ptrJob)
      {
        for (auto const& worker : workers)
        {
          child_proxy
            ( this
            , _worker_manager.address_by_worker (worker).get()->second
            )
            .submit_job ( ptrJob->id()
                        , ptrJob->activity()
                        , implementation
                        , workers
                        );
        }
      }
    }

    std::string Agent::gen_id()
    {
      static id_generator generator ("job");
      return generator.next();
    }

    Job* Agent::addJob ( const sdpa::job_id_t& job_id
                               , we::type::activity_t activity
                               , job_source source
                               , job_handler handler
                               )
    {
      return addJob ( job_id
                    , std::move (activity)
                    , std::move (source)
                    , std::move (handler)
                    , activity.requirements_and_preferences
                        (_virtual_memory_api.get())
                    );
    }


    Job* Agent::addJobWithNoPreferences
      ( const sdpa::job_id_t& job_id
      , we::type::activity_t activity
      , job_source source
      , job_handler handler
      )
    {
      return addJob
        ( job_id
        , std::move (activity)
        , std::move (source)
        , std::move (handler)
        , {{}, {}, null_transfer_cost, 1.0, 0, {}} //empty preferences
        );
    }

    Job* Agent::addJob
      ( const sdpa::job_id_t& job_id
      , we::type::activity_t activity
      , job_source source
      , job_handler handler
      , Requirements_and_preferences requirements_and_preferences
      )
    {
      Job* pJob = new Job
        ( job_id
        , std::move (activity)
        , std::move (source)
        , std::move (handler)
        , std::move (requirements_and_preferences)
        );

      std::lock_guard<std::mutex> const _ (_job_map_mutex);

      if (!job_map_.emplace (job_id, pJob).second)
      {
        delete pJob;
        throw std::runtime_error ("job with same id already exists");
      }

      return pJob;
    }

    Job* Agent::findJob (const sdpa::job_id_t& job_id ) const
    {
      std::lock_guard<std::mutex> const _ (_job_map_mutex);

      const job_map_t::const_iterator it (job_map_.find( job_id ));
      return it != job_map_.end() ? it->second : nullptr;
    }

    Job* Agent::require_job
      (job_id_t const& id, std::string const& error) const
    {
      Job* job (findJob (id));
      if (!job)
      {
        throw std::logic_error (error);
      }
      return job;
    }

    void Agent::deleteJob (const sdpa::job_id_t& job_id)
    {
      std::lock_guard<std::mutex> const _ (_job_map_mutex);

      const job_map_t::const_iterator it (job_map_.find( job_id ));
      if (it == job_map_.end())
      {
        throw std::runtime_error ("deleteJob: job not found");
      }

      delete it->second;
      job_map_.erase (it);
    }

    void Agent::handleDeleteJobEvent
      ( fhg::com::p2p::address_t const& source
      , events::DeleteJobEvent const* event
      )
    {
      Job* const job (require_job (event->job_id(), "delete_job for unknown job"));

      if (!sdpa::status::is_terminal (job->getStatus()))
      {
        throw std::runtime_error
          ("Cannot delete a job which is in a non-terminal state.");
      }

      if (!boost::get<job_source_client> (&job->source()))
      {
        throw std::invalid_argument
          ("tried deleting a job not submitted by a client");
      }

      deleteJob (job->id());
      parent_proxy (this, source).delete_job_ack (event->job_id());
    }

    void Agent::handleCancelJobEvent
      ( fhg::com::p2p::address_t const& source
      , events::CancelJobEvent const* event
      )
    {
      Job* const job (require_job (event->job_id(), "cancel_job for unknown job"));

      if (job->getStatus() == sdpa::status::CANCELING)
      {
        throw std::runtime_error
          ("A cancelation request for this job was already posted!");
      }

      if (sdpa::status::is_terminal (job->getStatus()))
      {
        throw std::runtime_error
          ( "Cannot cancel an already terminated job, its current status is: "
          + sdpa::status::show (job->getStatus())
          );
      }

      if ( boost::get<job_source_master> (&job->source())
         && boost::get<job_source_master> (job->source())._->address != source
         )
      {
        throw std::logic_error
          ("trying to cancel job owned by different master");
      }

      //! \note send immediately an acknowledgment to the component
      // that requested the cancellation if it does not get a
      // notification right after
      if (boost::get<job_source_wfe> (&job->source())
         || ( boost::get<job_source_client> (&job->source())
            && !isSubscriber (source, job->id())
            )
         )
      {
        parent_proxy (this, source).cancel_job_ack (event->job_id());
      }

      if (boost::get<job_handler_wfe> (&job->handler()))
      {
        if (job->getStatus() == sdpa::status::RUNNING)
        {
          job->CancelJob();
          workflowEngine()->cancel (job->id());
        }
        else
        {
          job_canceled (job);

          deleteJob (job->id());
        }
      }
      else
      {
        cancel_worker_handled_job (job->id());
      }
    }

    void Agent::emit_gantt ( job_id_t const& id
                                   , we::type::activity_t const& activity
                                   , NotificationEvent::state_t state
                                   )
    {
      _log_emitter.emit_message
        ( { NotificationEvent ({name()}, id, state, activity).encoded()
          , gantt_log_category
          }
        );
    }

    fhg::logging::endpoint Agent::logger_registration_endpoint() const
    {
      return _log_emitter.local_endpoint();
    }

    fhg::logging::stream_emitter& Agent::log_emitter()
    {
      return _log_emitter;
    }

    bool Agent::workflow_engine_submit (job_id_t job_id, Job* pJob)
    {
      try
      {
        workflowEngine()->submit (job_id, pJob->activity());

        // Should set the workflow_id here, or send it together with the activity
        pJob->Dispatch();

        return true;
      }
      catch (...)
      {
        fhg::util::current_exception_printer const error (": ");
        _log_emitter.emit ( "Exception occurred: " + error.string()
                          + ". Failed to submit the job " + job_id
                          + " to the workflow engine!"
                          , fhg::logging::legacy::category_level_error
                          );

        //! \note: was failed (job_id, error.string()) but wanted to skip
        //! the emission of the gantt
        job_failed (pJob, error.string());

        return false;
      }
    }

    void Agent::handleSubmitJobEvent
      ( fhg::com::p2p::address_t const& source
      , const events::SubmitJobEvent* evt
      )
    {
      const events::SubmitJobEvent& e (*evt);

      // First, check if the job 'job_id' wasn't already submitted!
      if(e.job_id() && findJob(*e.job_id()))
      {
        parent_proxy (this, source).submit_job_ack (*e.job_id());
        return;
      }

      const job_id_t job_id (e.job_id() ? *e.job_id() : job_id_t (gen_id()));

      auto const maybe_master (master_by_address (source));
      Job* const pJob ( addJobWithNoPreferences
                          ( job_id
                          , e.activity()
                          , maybe_master
                          ? job_source (job_source_master {*maybe_master})
                          : job_source (job_source_client{})
                          , hasWorkflowEngine()
                          ? job_handler (job_handler_wfe())
                          : job_handler (job_handler_worker())
                          )
                      );

      parent_proxy (this, source).submit_job_ack (job_id);

      // check if the message comes from outside or from WFE
      // if it comes from outside and the agent has an WFE, submit it to it
      if (boost::get<job_handler_wfe> (&pJob->handler()))
      {
        if (workflow_engine_submit (job_id, pJob))
        {
          emit_gantt (job_id, pJob->activity(), NotificationEvent::STATE_STARTED);
        }
      }
      else {
        _scheduler.enqueueJob(job_id);
        request_scheduling();
      }
    }

    void Agent::handleWorkerRegistrationEvent
      ( fhg::com::p2p::address_t const& source
      , const events::WorkerRegistrationEvent* event
      )
    try
    {
      // check if the worker source has already registered!
      // delete inherited capabilities that are owned by the current agent
      sdpa::capabilities_set_t workerCpbSet;

      // take the difference
      for (const sdpa::capability_t& cpb : event->capabilities())
      {
        // own capabilities have always the depth 0 and are not inherited
        // by the descendants
        if (!isOwnCapability (cpb))
        {
          workerCpbSet.emplace (cpb.with_increased_depth());
        }
      }

      _worker_manager.addWorker
        ( event->name()
        , workerCpbSet
        , event->allocated_shared_memory_size()
        , event->children_allowed()
        , event->hostname(), source
        );

      request_scheduling();

      // send to the masters my new set of capabilities
      if (!workerCpbSet.empty())
      {
        for (master_network_info const& info : _master_info)
        {
          if (info.address)
          {
            parent_proxy (this, *info.address).capabilities_gained (workerCpbSet);
          }
        }
      }

      child_proxy (this, source)
        .worker_registration_response (boost::none);
    }
    catch (...)
    {
      child_proxy (this, source)
        .worker_registration_response (std::current_exception());
    }

    void Agent::handleErrorEvent
      ( fhg::com::p2p::address_t const& source
      , const events::ErrorEvent* evt
      )
    {
      const sdpa::events::ErrorEvent& error (*evt);

      boost::optional<WorkerManager::worker_connections_t::right_map::iterator> const as_worker
        (_worker_manager.worker_by_address (source));

      // if it'a communication error, inspect all jobs and
      // send results if they are in a terminal state

      switch (error.error_code())
      {
        // this  should  better go  into  a  distinct  event, since  the  ErrorEvent
        // 'reason' should not be reused for important information
        case events::ErrorEvent::SDPA_EBACKLOGFULL:
        {
          sdpa::job_id_t const jobId(*error.job_id());
          Job* const pJob (findJob (jobId));
          if (!pJob)
          {
            throw std::runtime_error ("Got SDPA_EBACKLOGFULL error related to unknown job!");
          }

          if (!as_worker)
          {
            throw std::runtime_error ("Unknown entity (unregister worker) rejected the job " + jobId);
          }
        }
        FHG_UTIL_FALLTHROUGH;
        case events::ErrorEvent::SDPA_ENODE_SHUTDOWN:
        {
          unsubscribe(source);

          if (as_worker)
          {
            for (master_network_info const& info : _master_info)
            {
              if (info.address)
              {
                parent_proxy (this, *info.address).capabilities_lost
                  (_worker_manager.worker_capabilities (as_worker.get()->second));
              }
            }

            std::lock_guard<std::mutex> const _ (_cancel_mutex);
            _scheduler.reschedule_worker_jobs_and_maybe_remove_worker
              ( as_worker.get()->second
              , [this] (job_id_t const& job)
                {
                  return findJob (job);
                }
              , [this] (worker_id_t const& worker, job_id_t const& job)
                {
                  return child_proxy ( this
                                     , _worker_manager.address_by_worker (worker).get()->second
                                     ).cancel_job (job);
                }
              , (error.error_code() == events::ErrorEvent::SDPA_EBACKLOGFULL)
              );

            request_scheduling();
          }
          else
          {
            boost::optional<master_info_t::iterator> const as_master
              (master_by_address (source));

            if (as_master)
            {
              as_master.get()->address = boost::none;
              request_registration_soon (*as_master.get());
            }
          }

          break;
        }
        default:
        {
          _log_emitter.emit ( "Unhandled error ("
                            + std::to_string (error.error_code()) + "): "
                            + error.reason()
                            , fhg::logging::legacy::category_level_error
                            );
        }
      }
    }

    void Agent::submit ( const we::layer::id_type& job_id
                               , we::type::activity_t activity
                               )
    try
    {
      if (activity.handle_by_workflow_engine())
      {
        workflow_engine_submit
          ( job_id
          , addJobWithNoPreferences
              (job_id, std::move (activity), job_source_wfe(), job_handler_wfe())
          );
      }
      else
      {
        addJob (job_id, std::move (activity), job_source_wfe(), job_handler_worker());

        _scheduler.enqueueJob (job_id);
        request_scheduling();
      }
    }
    catch (...)
    {
      workflowEngine()->failed
        (job_id, fhg::util::current_exception_printer (": ").string());
    }

    void Agent::cancel (const we::layer::id_type& job_id)
    {
      delay
        (std::bind (&Agent::cancel_worker_handled_job, this, job_id));
    }

    void Agent::cancel_worker_handled_job
      (we::layer::id_type const& job_id)
    {
      std::lock_guard<std::mutex> const _ (_cancel_mutex);

      Job* const pJob (findJob (job_id));
      if (!pJob)
      {
        //! \note Job may have been removed between wfe requesting cancel
        //! and event thread handling this, which is not an error: wfe
        //! correctly handles that situation and expects us to ignore it.
        return;
      }

      bool const cancel_already_requested
        ( pJob->getStatus() == sdpa::status::CANCELING
        || pJob->getStatus() == sdpa::status::CANCELED
        );

      pJob->CancelJob();

      const std::unordered_set<worker_id_t>
        workers_to_cancel (_worker_manager.workers_to_send_cancel (job_id));

      if (!workers_to_cancel.empty())
      {
        if (cancel_already_requested)
        {
          return;
        }

        for (worker_id_t const& w : workers_to_cancel)
        {
          child_proxy ( this
                      , _worker_manager.address_by_worker (w).get()->second
                      ).cancel_job (job_id);
        }
      }
      else
      {
        job_canceled (pJob);

        _scheduler.delete_job (job_id);
        _scheduler.releaseReservation (job_id);
        _scheduler.delete_pending_job (job_id);

        if (!boost::get<job_source_client> (&pJob->source()))
        {
          deleteJob (job_id);
        }
      }
    }

    void Agent::finished ( const we::layer::id_type& id
                         , const we::type::activity_t& result
                         )
    {
      Job* const pJob (require_job (id, "got finished message for old/unknown Job " + id));

      job_finished (pJob, result);

      //! \note #817: gantt does not support nesting
      if ( fhg::util::visit<bool>
             ( pJob->source()
             , [] (job_source_wfe const&) { return false; }
             , [] (job_source_master const&) { return true; }
             , [] (job_source_client const&) { return true; }
             )
         )
      {
        emit_gantt (pJob->id(), pJob->result(), NotificationEvent::STATE_FINISHED);
      }
    }

    void Agent::failed ( const we::layer::id_type& id
                       , std::string const & reason
                       )
    {
      Job* const pJob (require_job (id, "got failed message for old/unknown Job " + id));

      job_failed (pJob, reason);

      emit_gantt (pJob->id(), pJob->activity(), NotificationEvent::STATE_FAILED);
    }

    void Agent::canceled (const we::layer::id_type& job_id)
    {
      Job* const pJob (require_job (job_id, "rts_canceled (unknown job)"));

      job_canceled (pJob);

      emit_gantt (pJob->id(), pJob->result(), NotificationEvent::STATE_CANCELED);

      if (boost::get<job_source_master> (&pJob->source()))
      {
        deleteJob (job_id);
      }
    }

    void Agent::handle_job_termination (Job* job)
    {
      auto const results
        (_scheduler.get_aggregated_results_if_all_terminated (job->id()));

      if (!results)
      {
        return;
      }

      //! \note rescheduled: never tell workflow engine or modify state!
      if ( _scheduler.reservation_canceled (job->id())
        && (job->getStatus() != sdpa::status::CANCELING)
         )
      {
        _scheduler.releaseReservation (job->id());
        _scheduler.enqueueJob (job->id());
        request_scheduling();

        return;
      }

      //! \todo instead of ignoring sub-failures and merging error
      //! messages, just pass on results to the user
      if (job->getStatus() == sdpa::status::CANCELING)
      {
        job_canceled (job);
      }
      else
      {
        std::vector<std::string> errors;
        for (auto& result : results->individual_results)
        {
          if ( JobFSM_::s_failed const* as_failed
             = boost::get<JobFSM_::s_failed> (&result.second)
             )
          {
            errors.emplace_back (result.first + ": " + as_failed->message);
          }
        }

        if (errors.empty())
        {
          job_finished (job, results->last_success.result);
        }
        else
        {
          job_failed (job, fhg::util::join (errors.begin(), errors.end(), ", ").string());
        }
      }

      _scheduler.releaseReservation (job->id());
      request_scheduling();

      if (boost::get<job_source_wfe> (&job->source()))
      {
        deleteJob (job->id());
      }
    }

    void Agent::handleJobFinishedEvent
      ( fhg::com::p2p::address_t const& source
      , events::JobFinishedEvent const* event
      )
    {
      Job* const job
        (require_job (event->job_id(), "job_finished for unknown job"));

      _scheduler.store_result
        ( _worker_manager.worker_by_address (source).get()->second
        , job->id()
        , JobFSM_::s_finished (event->result())
        );

      handle_job_termination (job);

      child_proxy (this, source).job_finished_ack (event->job_id());
    }

    void Agent::handleJobFailedEvent
      ( fhg::com::p2p::address_t const& source
      , events::JobFailedEvent const* event
      )
    {
      Job* const job
        (require_job (event->job_id(), "job_failed for unknown job"));

      _scheduler.store_result
        ( _worker_manager.worker_by_address (source).get()->second
        , job->id()
        , JobFSM_::s_failed (event->error_message())
        );

      handle_job_termination (job);

      child_proxy (this, source).job_failed_ack (event->job_id());
    }

    void Agent::handleCancelJobAckEvent
      ( fhg::com::p2p::address_t const& source
      , events::CancelJobAckEvent const* event
      )
    {
      Job* const job
        (require_job (event->job_id(), "cancel_job_ack for unknown job"));

      _scheduler.store_result
        ( _worker_manager.worker_by_address (source).get()->second
        , job->id()
        , JobFSM_::s_canceled()
        );

      handle_job_termination (job);
    }

    void Agent::job_finished
      (Job* job, we::type::activity_t const& result)
    {
      job->JobFinished (result);

      struct
      {
        using result_type = void;
        void operator() (job_source_wfe const&) const
        {
          _wfe->finished (_job->id(), _job->result());
        }
        void operator() (job_source_client const&) const {}
        void operator() (job_source_master const& master) const
        {
          parent_proxy (_this, master._).job_finished (_job->id(), _job->result());
        }
        Agent* _this;
        Job* _job;
        we::layer* _wfe;
      } visitor = {this, job, ptr_workflow_engine_.get()};
      boost::apply_visitor (visitor, job->source());

      notify_subscribers<events::JobFinishedEvent>
        (job->id(), job->id(), job->result());
    }
    void Agent::job_failed (Job* job, std::string const& reason)
    {
      job->JobFailed (reason);

      struct
      {
        using result_type = void;
        void operator() (job_source_wfe const&) const
        {
          _wfe->failed (_job->id(), _job->error_message());
        }
        void operator() (job_source_client const&) const {}
        void operator() (job_source_master const& master) const
        {
          parent_proxy (_this, master._).job_failed
            (_job->id(), _job->error_message());
        }
        Agent* _this;
        Job* _job;
        we::layer* _wfe;
      } visitor = {this, job, ptr_workflow_engine_.get()};
      boost::apply_visitor (visitor, job->source());

      notify_subscribers<events::JobFailedEvent>
        (job->id(), job->id(), job->error_message());
    }
    void Agent::job_canceled (Job* job)
    {
      job->CancelJobAck();

      struct
      {
        using result_type = void;
        void operator() (job_source_wfe const&) const
        {
          _wfe->canceled (_job->id());
        }
        void operator() (job_source_client const&) const {}
        void operator() (job_source_master const& master) const
        {
          parent_proxy (_this, master._).cancel_job_ack (_job->id());
        }
        Agent* _this;
        Job* _job;
        we::layer* _wfe;
      } visitor = {this, job, ptr_workflow_engine_.get()};
      boost::apply_visitor (visitor, job->source());

      notify_subscribers<events::CancelJobAckEvent> (job->id(), job->id());
    }

    boost::optional<master_info_t::iterator>
      Agent::master_by_address (fhg::com::p2p::address_t const& address)
    {
      master_info_t::iterator const it
        ( std::find_if ( _master_info.begin()
                       , _master_info.end()
                       , [&address] (master_info_t::value_type const& info)
                         {
                           return info.address == address;
                         }
                       )
        );
      return boost::make_optional (it != _master_info.end(), it);
    }

    void Agent::handle_worker_registration_response
      ( fhg::com::p2p::address_t const& source
      , sdpa::events::worker_registration_response const* response
      )
    {
      fhg::util::boost::get_or_throw<std::runtime_error>
        ( master_by_address (source)
        , "workerRegistrationAckEvent from source not in list of masters"
        );

      response->get();
    }

    void Agent::handleCapabilitiesGainedEvent
      ( fhg::com::p2p::address_t const& source
      , const events::CapabilitiesGainedEvent* pCpbGainEvt
      )
    {
      WorkerManager::worker_connections_t::right_map::iterator const worker
        ( fhg::util::boost::get_or_throw<std::runtime_error>
            ( _worker_manager.worker_by_address (source)
            , "capabilities_gained for unknown worker"
            )
        );

      sdpa::capabilities_set_t workerCpbSet;

      for (const sdpa::capability_t& cpb : pCpbGainEvt->capabilities())
      {
        // own capabilities have always the depth 0
        if (!isOwnCapability (cpb))
        {
          workerCpbSet.emplace (cpb.with_increased_depth());
        }
      }

      bool const bModified
        (_worker_manager.add_worker_capabilities (worker->second, workerCpbSet));

      if (bModified)
      {
        request_scheduling();

        if (!workerCpbSet.empty())
        {
          for (master_network_info const& info : _master_info)
          {
            if (info.address)
            {
              parent_proxy (this, *info.address).capabilities_gained (workerCpbSet);
            }
          }
        }
      }
    }

    void Agent::handleCapabilitiesLostEvent
      ( fhg::com::p2p::address_t const& source
      , const events::CapabilitiesLostEvent* pCpbLostEvt
      )
    {
      // tell the scheduler to remove the capabilities of the worker source

     WorkerManager::worker_connections_t::right_map::iterator const worker
        ( fhg::util::boost::get_or_throw<std::runtime_error>
            ( _worker_manager.worker_by_address (source)
            , "capabilities_lost for unknown worker"
            )
        );

      if ( _worker_manager.remove_worker_capabilities
             ( worker->second, pCpbLostEvt->capabilities())
         )
      {
        for (master_network_info const& info : _master_info)
        {
          if (info.address)
          {
            parent_proxy (this, *info.address).capabilities_lost
              (pCpbLostEvt->capabilities());
          }
        }
      }
    }

    void Agent::handle_events()
    try
    {
      while (true)
      {
        const std::pair<fhg::com::p2p::address_t, events::SDPAEvent::Ptr> event
          (_event_queue.get());
        try
        {
          event.second->handleBy (event.first, this);
        }
        catch (...)
        {
          sendEventToOther<events::ErrorEvent>
            ( event.first
            , events::ErrorEvent::SDPA_EUNKNOWN
            , fhg::util::current_exception_printer (": ").string()
            );
        }
      }
    }
    catch (decltype (_event_queue)::interrupted const&)
    {
    }

    void Agent::delay (std::function<void()> fun)
    {
      _event_queue.put
        ( fhg::com::p2p::address_t()
        , events::SDPAEvent::Ptr (new events::delayed_function_call (fun))
        );
    }

    void Agent::request_registration_soon
      (master_network_info& master)
    {
      _registration_threads.start
        (std::bind (&Agent::do_registration_after_sleep, this, master));
    }

    void Agent::do_registration_after_sleep
      (master_network_info& master)
    {
      std::this_thread::sleep_for (_registration_timeout);

      requestRegistration (master);
    }

    void Agent::requestRegistration (master_network_info& master)
    {
      try
      {
        master.address = _network_strategy.connect_to (master.host, master.port);
      }
      catch (std::exception const&)
      {
        request_registration_soon (master);
      }

      std::lock_guard<std::mutex> const guard_capabilites (mtx_cpb_);
      capabilities_set_t cpbSet;

      _worker_manager.getCapabilities (cpbSet);

      parent_proxy (this, master).worker_registration (cpbSet);
    }

    void Agent::unsubscribe(const fhg::com::p2p::address_t& id)
    {
      std::lock_guard<std::mutex> const _ (mtx_subscriber_);

      _subscriptions.left.erase (id);
    }

    void Agent::handleSubscribeEvent
      ( fhg::com::p2p::address_t const& source
      , const events::SubscribeEvent* pEvt
      )
    {
      const job_id_t& jobId (pEvt->job_id());

      std::lock_guard<std::mutex> const _ (mtx_subscriber_);

      Job const* const pJob (findJob (jobId));
      if (!pJob)
      {
        throw std::runtime_error ( "Could not subscribe for the job" + jobId
                                 + ". The job does not exist!"
                                 );
      }

      sendEventToOther<events::SubscribeAckEvent> (source, jobId);

      // check if the subscribed jobs are already in a terminal state
      const sdpa::status::code status (pJob->getStatus());
      switch (status)
      {
      case sdpa::status::FINISHED:
        {
          sendEventToOther<events::JobFinishedEvent>
            (source, pJob->id(), pJob->result());
        }
        return;

      case sdpa::status::FAILED:
        {
          sendEventToOther<events::JobFailedEvent>
            (source, pJob->id(), pJob->error_message());
        }
        return;

      case sdpa::status::CANCELED:
        {
          sendEventToOther<events::CancelJobAckEvent> (source, pJob->id());
        }
        return;

      case sdpa::status::PENDING:
      case sdpa::status::RUNNING:
      case sdpa::status::CANCELING:
        // store the subscriber and notify it later, when the job is terminated
        _subscriptions.insert ({source, jobId});
        return;
      }

      INVALID_ENUM_VALUE (sdpa::status::code, status);
    }

    bool Agent::isSubscriber
      (fhg::com::p2p::address_t const& agentId, job_id_t const& job)
    {
      std::lock_guard<std::mutex> const _ (mtx_subscriber_);
      return _subscriptions.count
        (decltype (_subscriptions)::value_type (agentId, job));
    }

    /**
     * Event SubmitJobAckEvent
     * Precondition: an acknowledgment event was received from a master
     * Action: - if the job was found, put the job into the state Running
     *         - move the job from the submitted queue of the worker worker_id, into its
     *           acknowledged queue
     *         - in the case when the worker was not found, trigger an exception and send back
     *           an error message
     * Postcondition: is either into the Running state or inexistent
     */
    void Agent::handleSubmitJobAckEvent
      (fhg::com::p2p::address_t const& source, const events::SubmitJobAckEvent* pEvent)
    {
      // Only, now should be state of the job updated to RUNNING
      // since it was not rejected, no error occurred etc ....
      //find the job ptrJob and call
      Job* const ptrJob = findJob(pEvent->job_id());
      if(!ptrJob)
      {
        _log_emitter.emit ( "job " + pEvent->job_id()
                          + " could not be acknowledged: the job "
                          + pEvent->job_id() + " not found!"
                          , fhg::logging::legacy::category_level_error
                          );

        throw std::runtime_error
          ("Could not acknowledge job: " + pEvent->job_id() + " not found");
      }

      if(ptrJob->getStatus() == sdpa:: status::CANCELING)
        return;

      WorkerManager::worker_connections_t::right_map::iterator const worker
        ( fhg::util::boost::get_or_throw<std::runtime_error>
            ( _worker_manager.worker_by_address (source)
            , "submit_job_ack for unknown worker"
            )
        );

      ptrJob->Dispatch();
      _worker_manager.acknowledge_job_sent_to_worker
        (pEvent->job_id(), worker->second);
    }

    // respond to a worker that the JobFinishedEvent was received
    void Agent::handleJobFinishedAckEvent
      (fhg::com::p2p::address_t const&, const events::JobFinishedAckEvent* pEvt)
    {
      // The result was successfully delivered by the worker and the WE was notified
      // therefore, I can delete the job from the job map
      if (!findJob(pEvt->job_id()))
      {
        throw std::runtime_error ("Couldn't find the job!");
      }

      // delete it from the map when you receive a JobFinishedAckEvent!
      deleteJob(pEvt->job_id());
    }

    // respond to a worker that the JobFailedEvent was received
    void Agent::handleJobFailedAckEvent
      ( fhg::com::p2p::address_t const&
      , const events::JobFailedAckEvent* pEvt
      )
    {
      if (!findJob(pEvt->job_id()))
      {
        throw std::runtime_error ("Couldn't find the job!");
      }

      // delete it from the map when you receive a JobFailedAckEvent!
      deleteJob(pEvt->job_id());
    }

    void Agent::discover
      ( we::layer::id_type discover_id
      , we::layer::id_type job_id
      )
    {
      delay ( std::bind ( &Agent::delayed_discover, this
                        , discover_id, job_id
                        )
            );
    }

    void Agent::delayed_discover
      ( we::layer::id_type discover_id
      , we::layer::id_type job_id
      )
    {
      Job const* const pJob (findJob (job_id));

      if (!pJob)
      {
        workflowEngine()->discovered
          ( discover_id
          , discovery_info_t (job_id, boost::none, discovery_info_set_t())
          );

        return;
      }

      if (pJob->getStatus() != sdpa::status::RUNNING)
      {
        workflowEngine()->discovered
          ( discover_id
          , discovery_info_t (job_id, pJob->getStatus(), discovery_info_set_t())
          );

        return;
      }

      fhg::util::visit<void>
        ( pJob->handler()
        , [&] (job_handler_worker const&)
          {
            std::unordered_set<worker_id_t> const workers
              (_worker_manager.findSubmOrAckWorkers (job_id));

            for (worker_id_t const& w : workers)
            {
              child_proxy (this, _worker_manager.address_by_worker (w).get()->second)
                .discover_job_states (job_id, discover_id);
            }
          }
        , [&] (job_handler_wfe const&)
          {
            workflowEngine()->discovered
              ( discover_id
              , discovery_info_t (job_id, pJob->getStatus(), discovery_info_set_t())
              );
          }
        );
    }

    void Agent::handleDiscoverJobStatesEvent
      ( fhg::com::p2p::address_t const& source
      , const sdpa::events::DiscoverJobStatesEvent *pEvt
      )
    {
      auto const job_id (pEvt->job_id());
      Job const* const pJob (findJob (job_id));

      if (!pJob)
      {
        parent_proxy (this, source).discover_job_states_reply
          ( pEvt->discover_id()
          , discovery_info_t (job_id, boost::none, discovery_info_set_t())
          );

        return;
      }

      if (pJob->getStatus() != sdpa::status::RUNNING)
      {
        parent_proxy (this, source).discover_job_states_reply
          ( pEvt->discover_id()
          , discovery_info_t (job_id, pJob->getStatus(), discovery_info_set_t())
          );

        return;
      }

      fhg::util::visit<void>
        ( pJob->handler()
        , [&] (job_handler_worker const&)
          {
            _discover_sources.emplace
              (std::make_pair (pEvt->discover_id(), job_id), source);

            std::unordered_set<worker_id_t> const workers
              (_worker_manager.findSubmOrAckWorkers (job_id));

            for (worker_id_t const& w: workers)
            {
              child_proxy (this, _worker_manager.address_by_worker (w).get()->second)
                .discover_job_states (job_id, pEvt->discover_id());
            }
          }
        , [&] (job_handler_wfe const&)
          {
            _discover_sources.emplace
              (std::make_pair (pEvt->discover_id(), job_id), source);

            workflowEngine()->discover (pEvt->discover_id(), job_id);
          }
        );
    }

    void Agent::discovered
      ( we::layer::id_type discover_id
      , sdpa::discovery_info_t discover_result
      )
    {
      const std::pair<job_id_t, job_id_t> source_id
        (discover_id, discover_result.job_id());

      parent_proxy (this, _discover_sources.at (source_id))
        .discover_job_states_reply (discover_id, discover_result);

      _discover_sources.erase (source_id);
    }

    void Agent::handleDiscoverJobStatesReplyEvent
      ( fhg::com::p2p::address_t const&
      , const sdpa::events::DiscoverJobStatesReplyEvent* e
      )
    {
      const std::pair<job_id_t, job_id_t> source_id
        (e->discover_id(), e->discover_result().job_id());
      const std::unordered_map<std::pair<job_id_t, job_id_t>, fhg::com::p2p::address_t>::iterator
        source (_discover_sources.find (source_id));

      if (source == _discover_sources.end())
      {
        workflowEngine()->discovered (e->discover_id(), e->discover_result());
      }
      else
      {
        parent_proxy (this, source->second).discover_job_states_reply
          (e->discover_id(), e->discover_result());

        _discover_sources.erase (source);
      }
    }

    void Agent::handleBacklogNoLongerFullEvent
      ( fhg::com::p2p::address_t const& source
      , const events::BacklogNoLongerFullEvent*
      )
    {
      boost::optional<WorkerManager::worker_connections_t::right_map::iterator> const as_worker
        (_worker_manager.worker_by_address (source));

      _worker_manager.set_worker_backlog_full (as_worker.get()->second, false);
      request_scheduling ();
    }

    namespace
    {
      template<typename Map>
        typename Map::mapped_type take (Map& map, typename Map::key_type key)
      {
        typename Map::iterator const it (map.find (key));
        if (it == map.end())
        {
          throw std::runtime_error ("take: key " + key + " not found");
        }

        typename Map::mapped_type const v (std::move (it->second));
        map.erase (it);
        return v;
      }
    }

    void Agent::handle_put_token
      ( fhg::com::p2p::address_t const& source
      , const events::put_token* event
      )
    try
    {
      auto const job_id (event->job_id());
      Job const* const job (findJob (job_id));

      if (!job || job->getStatus() != sdpa::status::RUNNING)
      {
        throw std::runtime_error
          ("unable to put token: " + event->job_id() + " unknown or not running");
      }

      fhg::util::visit<void>
        ( job->handler()
        , [&] (job_handler_worker const&)
          {
            _put_token_source.emplace (event->put_token_id(), source);

            std::unordered_set<worker_id_t> const workers
              (_worker_manager.findSubmOrAckWorkers (job_id));

            for (worker_id_t const& w : workers)
            {
              child_proxy (this, _worker_manager.address_by_worker (w).get()->second)
                .put_token ( job_id
                           , event->put_token_id()
                           , event->place_name()
                           , event->value()
                           );
            }
          }
        , [&] (job_handler_wfe const&)
          {
            _put_token_source.emplace (event->put_token_id(), source);

            workflowEngine()->put_token ( job_id
                                        , event->put_token_id()
                                        , event->place_name()
                                        , event->value()
                                        );
          }
        );
    }
    catch (...)
    {
      parent_proxy (this, source).put_token_response
        (event->put_token_id(), std::current_exception());
    }

    void Agent::handle_put_token_response
      ( fhg::com::p2p::address_t const&
      , events::put_token_response const* event
      )
    {
      parent_proxy (this, take (_put_token_source, event->put_token_id()))
        .put_token_response (event->put_token_id(), event->exception());
    }

    void Agent::token_put
      ( std::string put_token_id
      , boost::optional<std::exception_ptr> error
      )
    {
      parent_proxy (this, take (_put_token_source, put_token_id))
        .put_token_response (put_token_id, error);
    }

    void Agent::handle_workflow_response
      ( fhg::com::p2p::address_t const& source
      , const events::workflow_response* event
      )
    try
    {
      auto const job_id (event->job_id());
      Job const* const job (findJob (job_id));

      if (!job || job->getStatus() != sdpa::status::RUNNING)
      {
        throw std::runtime_error
          ( "unable to request workflow response: " + job_id
          + " unknown or not running"
          );
      }

      fhg::util::visit<void>
        ( job->handler()
        , [&] (job_handler_worker const&)
          {
            _workflow_response_source.emplace (event->workflow_response_id(), source);

            std::unordered_set<worker_id_t> const workers
              (_worker_manager.findSubmOrAckWorkers (job_id));

            for (worker_id_t const& w : workers)
            {
              child_proxy (this, _worker_manager.address_by_worker (w).get()->second)
                .workflow_response ( job_id
                                   , event->workflow_response_id()
                                   , event->place_name()
                                   , event->value()
                                   );
            }

          }
        , [&] (job_handler_wfe const&)
          {
            _workflow_response_source.emplace (event->workflow_response_id(), source);

            workflowEngine()->request_workflow_response ( job_id
                                                        , event->workflow_response_id()
                                                        , event->place_name()
                                                        , event->value()
                                                        );
          }
        );
    }
    catch (...)
    {
      parent_proxy (this, source).workflow_response_response
        (event->workflow_response_id(), std::current_exception());
    }

    void Agent::handle_workflow_response_response
      ( fhg::com::p2p::address_t const&
      , events::workflow_response_response const* event
      )
    {
      parent_proxy (this, take (_workflow_response_source, event->workflow_response_id()))
        .workflow_response_response (event->workflow_response_id(), event->content());
    }
    void Agent::workflow_response_response
      ( std::string workflow_response_id
      , boost::variant<std::exception_ptr, pnet::type::value::value_type> result
      )
    {
      parent_proxy (this, take (_workflow_response_source, workflow_response_id))
        .workflow_response_response (workflow_response_id, result);
    }

    void Agent::handleQueryJobStatusEvent
      ( fhg::com::p2p::address_t const& source
      , const events::QueryJobStatusEvent* pEvt
      )
    {
      sdpa::job_id_t const jobId = pEvt->job_id();

      Job const* const pJob (findJob(jobId));
      if (!pJob)
      {
        throw std::runtime_error ("Inexistent job: "+pEvt->job_id());
      }

      parent_proxy (this, source).query_job_status_reply
        (pJob->id(), pJob->getStatus(), pJob->error_message());
    }

    void Agent::scheduling_thread()
    {
      for (;;)
      {
        {
          std::unique_lock<std::mutex> lock (_scheduling_requested_guard);
          _scheduling_requested_condition.wait
            ( lock
            , [this]
              {
                return _scheduling_requested || _scheduling_interrupted;
              }
            );

          if (_scheduling_interrupted)
          {
            break;
          }

          _scheduling_requested = false;
        }

        std::lock_guard<std::mutex> const _ (_cancel_mutex);

        _scheduler.assignJobsToWorkers();
        _scheduler.steal_work();
        _scheduler.start_pending_jobs
          (std::bind ( &Agent::serveJob
                     , this
                     , std::placeholders::_1
                     , std::placeholders::_2
                     , std::placeholders::_3
                     )
          );
      }
    }

    void Agent::request_scheduling()
    {
      std::lock_guard<std::mutex> const _ (_scheduling_requested_guard);
      _scheduling_requested = true;
      _scheduling_requested_condition.notify_one();
    }

    Agent::child_proxy::child_proxy
        (Agent* that, fhg::com::p2p::address_t const& address)
      : _that (that)
      , _address (address)
    {}

    void Agent::child_proxy::worker_registration_response
      (boost::optional<std::exception_ptr> error) const
    {
      _that->sendEventToOther<events::worker_registration_response>
        (_address, std::move (error));
    }

    void Agent::child_proxy::submit_job
      ( boost::optional<job_id_t> id
      , we::type::activity_t activity
      , boost::optional<std::string> const& implementation
      , std::set<worker_id_t> const& workers
      ) const
    {
      _that->sendEventToOther<events::SubmitJobEvent>
        (_address, id, std::move (activity), implementation, workers);
    }

    void Agent::child_proxy::cancel_job (job_id_t id) const
    {
      _that->sendEventToOther<events::CancelJobEvent> (_address, id);
    }

    void Agent::child_proxy::job_failed_ack (job_id_t id) const
    {
      _that->sendEventToOther<events::JobFailedAckEvent> (_address, id);
    }

    void Agent::child_proxy::job_finished_ack (job_id_t id) const
    {
      _that->sendEventToOther<events::JobFinishedAckEvent> (_address, id);
    }

    void Agent::child_proxy::discover_job_states
      (job_id_t job_id, job_id_t discover_id) const
    {
      _that->sendEventToOther<events::DiscoverJobStatesEvent>
        (_address, job_id, discover_id);
    }

    void Agent::child_proxy::put_token
      ( job_id_t job_id
      , std::string put_token_id
      , std::string place_name
      , pnet::type::value::value_type value
      ) const
    {
      _that->sendEventToOther<events::put_token>
        (_address, job_id, put_token_id, place_name, value);
    }

    void Agent::child_proxy::workflow_response
      ( job_id_t job_id
      , std::string workflow_response_id
      , std::string place_name
      , pnet::type::value::value_type value
      ) const
    {
      _that->sendEventToOther<events::workflow_response>
        (_address, job_id, workflow_response_id, place_name, value);
    }

    Agent::parent_proxy::parent_proxy
        (Agent* that, fhg::com::p2p::address_t const& address)
      : _that (that)
      , _address (address)
    {}

    Agent::parent_proxy::parent_proxy
        (Agent* that, master_network_info const& master)
      : parent_proxy (that, master.address.get())
    {}

    Agent::parent_proxy::parent_proxy
        ( Agent* that
        , boost::optional<master_info_t::iterator> const& job_master
        )
      : parent_proxy (that, *job_master.get())
    {}

    void Agent::parent_proxy::worker_registration
      ( capabilities_set_t capabilities
      ) const
    {
      _that->sendEventToOther<events::WorkerRegistrationEvent>
        ( _address
        , _that->name(), capabilities, 0, true, fhg::util::hostname()
        );
    }

    void Agent::parent_proxy::notify_shutdown() const
    {
      _that->sendEventToOther<events::ErrorEvent>
        (_address, events::ErrorEvent::SDPA_ENODE_SHUTDOWN, "");
    }

    void Agent::parent_proxy::job_failed
      (job_id_t id, std::string message) const
    {
      _that->sendEventToOther<events::JobFailedEvent> (_address, id, message);
    }

    void Agent::parent_proxy::job_finished
      (job_id_t id, we::type::activity_t result) const
    {
      _that->sendEventToOther<events::JobFinishedEvent> (_address, id, result);
    }

    void Agent::parent_proxy::cancel_job_ack (job_id_t id) const
    {
      _that->sendEventToOther<events::CancelJobAckEvent> (_address, id);
    }

    void Agent::parent_proxy::delete_job_ack (job_id_t id) const
    {
      _that->sendEventToOther<events::DeleteJobAckEvent> (_address, id);
    }

    void Agent::parent_proxy::submit_job_ack (job_id_t id) const
    {
      _that->sendEventToOther<events::SubmitJobAckEvent> (_address, id);
    }

    void Agent::parent_proxy::capabilities_gained
      (capabilities_set_t capabilities) const
    {
      _that->sendEventToOther<events::CapabilitiesGainedEvent>
        (_address, capabilities);
    }

    void Agent::parent_proxy::capabilities_lost
      (capabilities_set_t capabilities) const
    {
      _that->sendEventToOther<events::CapabilitiesLostEvent>
        (_address, capabilities);
    }

    void Agent::parent_proxy::discover_job_states_reply
      (job_id_t discover_id, discovery_info_t info) const
    {
      _that->sendEventToOther<events::DiscoverJobStatesReplyEvent>
        (_address, discover_id, info);
    }

    void Agent::parent_proxy::query_job_status_reply
      (job_id_t id, status::code status, std::string error_message) const
    {
      _that->sendEventToOther<events::JobStatusReplyEvent>
        (_address, id, status, error_message);
    }

    void Agent::parent_proxy::put_token_response
      (std::string put_token_id, boost::optional<std::exception_ptr> error) const
    {
      _that->sendEventToOther<events::put_token_response>
        (_address, put_token_id, error);
    }

    void Agent::parent_proxy::workflow_response_response
      ( std::string workflow_response_id
      , boost::variant<std::exception_ptr, pnet::type::value::value_type> content
      ) const
    {
      _that->sendEventToOther<events::workflow_response_response>
        (_address, workflow_response_id, content);
    }
  }
}
