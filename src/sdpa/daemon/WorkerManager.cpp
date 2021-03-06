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

#include <sdpa/daemon/WorkerManager.hpp>

#include <sdpa/types.hpp>

#include <fhg/assert.hpp>

#include <util-generic/make_optional.hpp>

#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/algorithm/count_if.hpp>
#include <boost/range/algorithm_ext/push_back.hpp>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <queue>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace sdpa
{
  namespace daemon
  {
    namespace
    {
      struct cost_and_matching_info_t
      {
        cost_and_matching_info_t
            ( double cost
            , double matching_degree
            , unsigned long shared_memory_size
            , double last_time_idle
            , worker_id_t const& worker_id
            , boost::optional<std::string>const& implementation
            )
          : _cost (cost)
          , _matching_degree (matching_degree)
          , _shared_memory_size (shared_memory_size)
          , _last_time_idle (last_time_idle)
          , _worker_id (worker_id)
          , _implementation (implementation)
        {}

        double _cost;
        double _matching_degree;
        unsigned long _shared_memory_size;
        double _last_time_idle;
        worker_id_t _worker_id;
        boost::optional<std::string> _implementation;
      };

      class Compare
       {
       public:
         bool operator() (cost_and_matching_info_t const& l, cost_and_matching_info_t const& r)
         {
           return std::tie ( l._cost, l._matching_degree
                           , l._shared_memory_size, l._last_time_idle
                           )
             < std::tie ( r._cost, r._matching_degree
                        , l._shared_memory_size,  r._last_time_idle
                        );
         }
       };

      typedef std::priority_queue < cost_and_matching_info_t
                                  , std::vector<cost_and_matching_info_t>
                                  , Compare
                                  > base_priority_queue_t;

      class bounded_priority_queue_t : private base_priority_queue_t
      {
      public:
        explicit bounded_priority_queue_t (std::size_t capacity)
          : capacity_ (capacity)
        {}

        template<typename... Args>
          void emplace (Args&&... args)
        {
          if (size() < capacity_)
          {
            base_priority_queue_t::emplace (std::forward<Args> (args)...);
            return;
          }

          cost_and_matching_info_t const next_tuple (std::forward<Args> (args)...);

          if (comp (next_tuple, top()))
          {
            pop();
            base_priority_queue_t::emplace (std::move (next_tuple));
          }
        }

        Workers_and_implementation
          assigned_workers_and_implementation() const
        {
          WorkerSet workers;

          std::transform ( c.begin()
                         , c.end()
                         , std::inserter (workers, workers.begin())
                         , [] (const cost_and_matching_info_t& cost_and_matching_info)
                           {
                             return cost_and_matching_info._worker_id;
                           }
                         );

          return std::make_pair (workers, c.front()._implementation);
        }

        std::size_t size() const { return base_priority_queue_t::size(); }
      private:
        size_t capacity_;
      };
    }

    worker_id_host_info_t::worker_id_host_info_t
        ( worker_id_t worker_id
        , std::string worker_host
        , unsigned long shared_memory_size
        , double last_time_idle
        , boost::optional<std::string> implementation
        )
      : worker_id_ (std::move (worker_id))
      , worker_host_ (std::move (worker_host))
      , shared_memory_size_ (shared_memory_size)
      , _last_time_idle (last_time_idle)
      , _implementation (std::move (implementation))
    {}

    boost::optional<std::string> const&
      worker_id_host_info_t::implementation() const
    {
      return _implementation;
    }

    std::string WorkerManager::host_INDICATES_A_RACE (const sdpa::worker_id_t& worker) const
    {
      return worker_map_.at(worker)._hostname;
    }

    bool WorkerManager::hasWorker_INDICATES_A_RACE_TESTING_ONLY(const worker_id_t& worker_id) const
    {
      std::lock_guard<std::mutex> const _ (mtx_);
      return worker_map_.find(worker_id) != worker_map_.end();
    }

    std::unordered_set<worker_id_t> WorkerManager::findSubmOrAckWorkers
      (job_id_t const& job_id) const
    {
      std::lock_guard<std::mutex> const _ (mtx_);
      std::unordered_set<worker_id_t> submitted_or_ack_workers;

      boost::copy ( worker_map_
                  | boost::adaptors::filtered
                    ( [&job_id] (std::pair<worker_id_t const, Worker> const& w)
                      {
                       return w.second.submitted_.count (job_id)
                         || w.second.acknowledged_.count (job_id);
                      }
                    )
                  | boost::adaptors::map_keys
                  , std::inserter ( submitted_or_ack_workers
                                  , submitted_or_ack_workers.begin()
                                  )
                  );

      return submitted_or_ack_workers;
    }

    void WorkerManager::addWorker ( const worker_id_t& workerId
                                  , const capabilities_set_t& cpbSet
                                  , unsigned long allocated_shared_memory_size
                                  , const bool children_allowed
                                  , const std::string& hostname
                                  , const fhg::com::p2p::address_t& address
                                  )
    {
      std::lock_guard<std::mutex> const _ (mtx_);

      if (worker_map_.count (workerId) != 0)
      {
        throw std::runtime_error ("worker '" + workerId + "' already exists");
      }
      worker_connections_.left.insert ({workerId, address});
      auto result = worker_map_.emplace ( workerId
                                        , Worker ( cpbSet
                                                 , allocated_shared_memory_size
                                                 , children_allowed
                                                 , hostname
                                                 )
                                        );

      worker_equiv_classes_[result.first->second.capability_names_].add_worker_entry (result.first);

      // allow to steal from itself
      worker_equiv_classes_.at (result.first->second.capability_names_)
        ._stealing_allowed_classes.emplace (result.first->second.capability_names_);

      _num_free_workers++;
    }


    void WorkerManager::deleteWorker (const worker_id_t& workerId)
    {
      std::lock_guard<std::mutex> const _ (mtx_);

      auto const worker (worker_map_.find (workerId));
      fhg_assert (worker != worker_map_.end(), "Worker not found when deletion was requested!");

      auto const equivalence_class
        (worker_equiv_classes_.find (worker->second.capability_names_));

      equivalence_class->second.remove_worker_entry (worker);
      if (equivalence_class->second.n_workers() == 0)
      {
        worker_equiv_classes_.erase (equivalence_class);

        for (auto& worker_class : worker_equiv_classes_)
        {
          worker_class.second._stealing_allowed_classes.erase
            (worker->second.capability_names_);
        }
      }

      worker_map_.erase (worker);
      worker_connections_.left.erase (workerId);

      _num_free_workers--;
    }

    void WorkerManager::getCapabilities (sdpa::capabilities_set_t& agentCpbSet) const
    {
      std::lock_guard<std::mutex> const _ (mtx_);

      for (Worker const& worker : worker_map_ | boost::adaptors::map_values)
      {
        for (sdpa::capability_t const& capability : worker._capabilities)
        {
          const sdpa::capabilities_set_t::iterator itag_cpbs
            (agentCpbSet.find (capability));
          if (itag_cpbs == agentCpbSet.end())
          {
            agentCpbSet.insert (capability);
          }
          else if (itag_cpbs->depth() > capability.depth())
          {
            agentCpbSet.erase (itag_cpbs);
            agentCpbSet.insert (capability);
          }
        }
      }
    }

    std::pair<boost::optional<double>, boost::optional<std::string>>
      WorkerManager::match_requirements_and_preferences
        ( std::set<std::string> const& capabilities
        , const Requirements_and_preferences& requirements_and_preferences
        ) const
    {
      std::size_t matchingDeg (0);
      for ( we::type::requirement_t const& req
          : requirements_and_preferences.requirements()
          )
      {
        if (capabilities.count (req.value()))
        {
          if (!req.is_mandatory())
          {
            ++matchingDeg;
          }
        }
        else if (req.is_mandatory())
        {
          return std::make_pair (boost::none, boost::none);
        }
      }

      auto const preferences (requirements_and_preferences.preferences());

      if (preferences.empty())
      {
        return std::make_pair
          ( ( ( matchingDeg + 1.0)
            / (capabilities.size() + 1.0)
            )
          , boost::none
          );
      }

      auto const preference
        ( std::find_if ( preferences.cbegin()
                       , preferences.cend()
                       , [&] (Preferences::value_type const& pref)
                         {
                           return capabilities.count (pref);
                         }
                       )
        );

      if (preference == preferences.cend())
      {
        return std::make_pair (boost::none, boost::none);
      }

      boost::optional<double> matching_req_and_pref_deg
        ( ( matchingDeg
          + std::distance (preference, preferences.end())
          + 1.0
          )
          /
          (capabilities.size() + preferences.size() + 1.0)
        );

      return std::make_pair (matching_req_and_pref_deg, *preference);
    }

    Workers_and_implementation WorkerManager::find_assignment
      (const Requirements_and_preferences& requirements_and_preferences) const
    {
      std::lock_guard<std::mutex> const _(mtx_);

      size_t const num_required_workers
        (requirements_and_preferences.numWorkers());

      if (worker_map_.size() < num_required_workers)
      {
        return Workers_and_implementation ({}, boost::none);
      }

      bounded_priority_queue_t bpq (num_required_workers);

      for (auto const& worker_class : worker_equiv_classes_)
      {
        auto const matching_degree_and_implementation
          (match_requirements_and_preferences
             ( worker_class.first
             , requirements_and_preferences
             )
          );

        if (!matching_degree_and_implementation.first)
        {
          continue;
        }

        for (auto& worker_id : worker_class.second._worker_ids)
        {
          auto const& worker (worker_map_.at (worker_id));

          if (num_required_workers > 1 && worker._children_allowed)
            { continue; }

          if ( requirements_and_preferences.shared_memory_amount_required()
             > worker._allocated_shared_memory_size
             )
            { continue; }

          if (worker.backlog_full())
            { continue; }

          double const total_cost
            ( requirements_and_preferences.transfer_cost()(worker._hostname)
            + requirements_and_preferences.computational_cost()
            + worker_map_.at (worker_id).cost_assigned_jobs()
            );

          bpq.emplace ( total_cost
                      , -1.0 * matching_degree_and_implementation.first.get()
                      , worker._allocated_shared_memory_size
                      , worker._last_time_idle
                      , worker_id
                      , matching_degree_and_implementation.second
                      );
        }
      }

      if (bpq.size() == num_required_workers)
      {
        return bpq.assigned_workers_and_implementation();
      }

      return Workers_and_implementation ({}, boost::none);
    }

    std::pair<bool, unsigned long>
      WorkerManager::submit_and_serve_if_can_start_job_INDICATES_A_RACE
        ( job_id_t const& job_id
        , WorkerSet const& workers
        , Implementation const& implementation
        , std::function<void ( WorkerSet const&
                             , Implementation const&
                             , const job_id_t&
                             )
                       > const& serve_job
        )
    {
      std::lock_guard<std::mutex> const _(mtx_);
      bool const can_start
        ( std::all_of ( workers.begin()
                      , workers.end()
                      , [this] (worker_id_t const& worker)
                        {
                          return worker_map_.count (worker)
                            && !worker_map_.at (worker).isReserved();
                        }
                      )
        );

      if (can_start)
      {
        for (auto const& worker: workers)
        {
          submit_job_to_worker (job_id, worker);
        }

        serve_job (workers, implementation, job_id);
      }

      return std::make_pair (can_start, _num_free_workers);
    }

    std::unordered_set<worker_id_t> WorkerManager::workers_to_send_cancel
      (job_id_t const& job_id)
    {
      std::unordered_set<worker_id_t> workers_to_cancel;

      boost::copy ( worker_map_
                  | boost::adaptors::filtered
                    ( [&job_id] (std::pair<worker_id_t const, Worker> const& w)
                      {
                       return w.second.submitted_.count (job_id)
                         || w.second.acknowledged_.count (job_id);
                      }
                    )
                  | boost::adaptors::map_keys
                  , std::inserter (workers_to_cancel, workers_to_cancel.begin())
                  );

      return workers_to_cancel;
    }

    void WorkerManager::assign_job_to_worker
      ( const job_id_t& job_id
      , const worker_id_t& worker_id
      , double cost
      , Preferences const& preferences
      )
    {
      std::lock_guard<std::mutex> const _(mtx_);
      auto worker (worker_map_.find (worker_id));
      fhg_assert (worker != worker_map_.end());
      assign_job_to_worker (job_id, worker, cost, preferences);
    }

    void WorkerManager::assign_job_to_worker
      ( const job_id_t& job_id
      , const worker_iterator worker
      , double cost
      , Preferences const& preferences
      )
    {
      worker->second.assign (job_id, cost);

      auto& worker_class
        (worker_equiv_classes_.at (worker->second.capability_names_));
      worker_class.inc_pending_jobs (1);

      worker_class._idle_workers.erase (worker->first);

      worker_class.allow_classes_matching_preferences_stealing
        (worker_equiv_classes_, preferences);
    }

    void WorkerManager::submit_job_to_worker (const job_id_t& job_id, const worker_id_t& worker_id)
    {
      auto worker (worker_map_.find (worker_id));
      worker->second.submit (job_id);
      auto& equivalence_class
        (worker_equiv_classes_.at (worker->second.capability_names_));

      equivalence_class.dec_pending_jobs (1);
      equivalence_class.inc_running_jobs (1);

      equivalence_class._idle_workers.erase (worker->first);

      // Update the counter only if the worker is terminal,
      // i.e. it has no slaves. Submission to slave agents is always allowed.
      if (worker->second.is_terminal())
      {
        _num_free_workers--;
      }
    }

    void WorkerManager::acknowledge_job_sent_to_worker ( const job_id_t& job_id
                                                       , const worker_id_t& worker_id
                                                       )
    {
      std::lock_guard<std::mutex> const _(mtx_);
      worker_map_.at (worker_id).acknowledge (job_id);
    }

    void WorkerManager::delete_job_from_worker ( const job_id_t &job_id
                                               , const worker_id_t& worker_id
                                               , double cost
                                               )
    {
      std::lock_guard<std::mutex> const _(mtx_);
      auto worker (worker_map_.find (worker_id));
      delete_job_from_worker (job_id, worker, cost);
    }

    void WorkerManager::delete_job_from_worker
      ( const job_id_t &job_id
      , const worker_iterator worker
      , double cost
      )
    {
      if (worker != worker_map_.end())
      {
        auto& equivalence_class
          (worker_equiv_classes_.at (worker->second.capability_names_));

        if (worker->second.pending_.count (job_id))
        {
          worker->second.delete_pending_job (job_id, cost);
          equivalence_class.dec_pending_jobs (1);
        }
        else
        {
          worker->second.delete_submitted_job (job_id, cost);
          equivalence_class.dec_running_jobs (1);
        }

        if ( !worker->second.has_running_jobs()
           && !worker->second.has_pending_jobs()
           )
        {
          equivalence_class._idle_workers.emplace (worker->first);
        }

        // Update the counter only if the worker is terminal,
        // i.e. it has no slaves. Submission to slave agents is always allowed.
        if (worker->second.is_terminal())
        {
          _num_free_workers++;
        }
      }
    }

    const capabilities_set_t& WorkerManager::worker_capabilities (const worker_id_t& worker) const
    {
      std::lock_guard<std::mutex> const _(mtx_);
      return worker_map_.at (worker)._capabilities;
    }

    void WorkerManager::change_equivalence_class ( worker_iterator worker
                                                 , std::set<std::string> const& old_cpbs
                                                 )
    {
      {
        auto equivalence_class
          (worker_equiv_classes_.find (old_cpbs));
        fhg_assert (equivalence_class != worker_equiv_classes_.end());

        equivalence_class->second.remove_worker_entry (worker);
        if (equivalence_class->second.n_workers() == 0)
        {
          worker_equiv_classes_.erase (equivalence_class);
        }
      }

      {
        auto& equivalence_class
          (worker_equiv_classes_[worker->second.capability_names_]);
        equivalence_class.add_worker_entry (worker);
      }
    }

    bool WorkerManager::add_worker_capabilities ( const worker_id_t& worker_id
                                                , const capabilities_set_t& cpb_set
                                                )
    {
      std::lock_guard<std::mutex> const _(mtx_);

      worker_map_t::iterator worker (worker_map_.find (worker_id));
      const std::set<std::string> old_cpbs (worker->second.capability_names_);

      if (worker->second.addCapabilities (cpb_set))
      {
        change_equivalence_class (worker, old_cpbs);
        return true;
      }

      return false;
    }

    bool WorkerManager::remove_worker_capabilities ( const worker_id_t& worker_id
                                                   , const capabilities_set_t& cpb_set
                                                   )
    {
      std::lock_guard<std::mutex> const _(mtx_);

      worker_map_t::iterator worker (worker_map_.find (worker_id));
      const std::set<std::string> old_cpbs (worker->second.capability_names_);

      if (worker->second.removeCapabilities (cpb_set))
      {
        change_equivalence_class (worker, old_cpbs);
        return true;
      }

      return false;
    }

    void WorkerManager::set_worker_backlog_full ( const worker_id_t& worker_id
                                                , bool val
                                                )
    {
      std::lock_guard<std::mutex> const _ (mtx_);
      return worker_map_.at (worker_id).set_backlog_full (val);
    }

    boost::optional<WorkerManager::worker_connections_t::right_iterator>
      WorkerManager::worker_by_address (fhg::com::p2p::address_t const& address)
    {
      WorkerManager::worker_connections_t::right_iterator it
        (worker_connections_.right.find (address));
      return boost::make_optional (it != worker_connections_.right.end(), it);
    }

    boost::optional<WorkerManager::worker_connections_t::left_iterator>
      WorkerManager::address_by_worker (std::string const& worker)
    {
      WorkerManager::worker_connections_t::left_iterator it
        (worker_connections_.left.find (worker));
      return boost::make_optional (it != worker_connections_.left.end(), it);
    }

    WorkerManager::WorkerEquivalenceClass::WorkerEquivalenceClass()
      : _n_pending_jobs (0)
      , _n_running_jobs (0)
    {}

    void WorkerManager::WorkerEquivalenceClass::inc_pending_jobs (unsigned int k)
    {
      _n_pending_jobs += k;
    }

    void WorkerManager::WorkerEquivalenceClass::dec_pending_jobs (unsigned int k)
    {
      fhg_assert ( _n_pending_jobs >= k
                 , "The number of pending jobs of a group of workers cannot be a negative number"
                 );

      _n_pending_jobs -= k;
    }

    void WorkerManager::WorkerEquivalenceClass::inc_running_jobs (unsigned int k)
    {
      _n_running_jobs += k;
    }

    void WorkerManager::WorkerEquivalenceClass::dec_running_jobs (unsigned int k)
    {
      fhg_assert ( _n_running_jobs >= k
                 , "The number of running jobs of a group of workers cannot be a negative number"
                 );

      _n_running_jobs -= k;
    }

    unsigned int WorkerManager::WorkerEquivalenceClass::n_pending_jobs() const
    {
      return _n_pending_jobs;
    }

    unsigned int WorkerManager::WorkerEquivalenceClass::n_running_jobs() const
    {
      return _n_running_jobs;
    }

    unsigned int WorkerManager::WorkerEquivalenceClass::n_workers() const
    {
      return _worker_ids.size();
    }

    void WorkerManager::WorkerEquivalenceClass::add_worker_entry
      (worker_iterator worker)
    {
      _worker_ids.emplace (worker->first);
      inc_pending_jobs (worker->second.pending_.size());
      inc_running_jobs ( worker->second.submitted_.size()
                       + worker->second.acknowledged_.size()
                       );
      _idle_workers.emplace (worker->first);
    }

    void WorkerManager::WorkerEquivalenceClass::remove_worker_entry
      (worker_iterator worker)
    {
      _worker_ids.erase (worker->first);
      dec_pending_jobs (worker->second.pending_.size());
      dec_running_jobs ( worker->second.submitted_.size()
                       + worker->second.acknowledged_.size()
                       );
      _idle_workers.erase (worker->first);
    }

    void WorkerManager::WorkerEquivalenceClass::allow_classes_matching_preferences_stealing
      ( std::map<std::set<std::string>, WorkerEquivalenceClass> const& worker_classes
      , Preferences const& preferences
      )
    {
      if (preferences.empty())
      {
        return;
      }

      for (auto const& worker_class : worker_classes)
      {
        if (std::any_of ( preferences.begin()
                        , preferences.end()
                        , [&worker_class] (std::string const& preference)
                          { return worker_class.first.count (preference); }
                        )
           )
        {
          _stealing_allowed_classes.emplace (worker_class.first);
        }
      }
    }

    void WorkerManager::steal_work
      (std::function<scheduler::Reservation* (job_id_t const&)> reservation)
    {
      std::lock_guard<std::mutex> const _(mtx_);
      for (WorkerEquivalenceClass& weqc : worker_equiv_classes_
                                        | boost::adaptors::map_values
          )
      {
        weqc.steal_work (reservation, *this);
      }
    }

    void WorkerManager::WorkerEquivalenceClass::steal_work
      ( std::function<scheduler::Reservation* (job_id_t const&)> reservation
      , WorkerManager& worker_manager
      )
    {
      if (n_pending_jobs() == 0)
      {
        return;
      }

      std::unordered_set<worker_id_t> thieves;

      auto const& worker_classes (worker_manager.worker_equiv_classes_);
      for (auto const& cls : _stealing_allowed_classes)
      {
        thieves.insert ( worker_classes.at (cls)._idle_workers.begin()
                       , worker_classes.at (cls)._idle_workers.end()
                       );
      }

      if (thieves.empty())
      {
        return;
      }

      std::function<double (job_id_t const& job_id)> const cost
        { [&reservation] (job_id_t const& job_id)
          {
            return reservation (job_id)->cost();
          }
        };

      std::function<bool (worker_iterator const&, worker_iterator const&)> const
        comp { [] (worker_iterator const& lhs, worker_iterator const& rhs)
               {
                 return lhs->second.cost_assigned_jobs()
                   < rhs->second.cost_assigned_jobs();
               }
             };

      std::priority_queue < worker_iterator
                          , std::vector<worker_iterator>
                          , decltype (comp)
                          > to_steal_from (comp);

      for (worker_id_t const& w : _worker_ids)
      {
        auto const& it (worker_manager.worker_map_.find (w));
        fhg_assert (it != worker_manager.worker_map_.end());
        Worker const& worker (it->second);

        if (worker.stealing_allowed())
        {
          to_steal_from.emplace (it);
        }
      }

      while (!(thieves.empty() || to_steal_from.empty()))
      {
        worker_iterator const richest (to_steal_from.top());
        worker_iterator const& thief
          (worker_manager.worker_map_.find (*thieves.begin()));
        Worker& richest_worker (richest->second);

        auto it_job (std::max_element ( richest_worker.pending_.begin()
                                      , richest_worker.pending_.end()
                                      , [&reservation] ( job_id_t const& r
                                                       , job_id_t const& l
                                                       )
                                        {
                                          return reservation (r)->cost()
                                            < reservation (l)->cost();
                                        }
                                      )
                    );

        fhg_assert (it_job != richest_worker.pending_.end());

        Preferences const preferences (reservation (*it_job)->preferences());

        auto const preference
          (std::find_if ( preferences.begin()
                        , preferences.end()
                        , [&] (std::string const& pref)
                          {
                            return thief->second.hasCapability (pref);
                          }
                        )
          );

        if (preferences.empty() || preference != preferences.end())
        {
          reservation (*it_job)->replace_worker
            ( richest->first
            , thief->first
            , FHG_UTIL_MAKE_OPTIONAL (!preferences.empty(), *preference)
            , [&thief] (const std::string& cpb)
              {
                return thief->second.hasCapability (cpb);
              }
            );

          worker_manager.assign_job_to_worker
            (*it_job, thief, cost (*it_job), preferences);
          worker_manager.delete_job_from_worker
            (*it_job, richest, cost (*it_job));

          to_steal_from.pop();

          if (richest_worker.stealing_allowed())
          {
            to_steal_from.emplace (richest);
          }
        }

        thieves.erase (thieves.begin());
      }
    }

    std::unordered_set<sdpa::job_id_t> WorkerManager::delete_or_cancel_worker_jobs
      ( worker_id_t const& worker_id
      , std::function<Job* (sdpa::job_id_t const&)> get_job
      , std::function<scheduler::Reservation* (sdpa::job_id_t const&)> get_reservation
      , std::function<void (sdpa::worker_id_t const&, job_id_t const&)> cancel_worker_job
      )
    {
      std::lock_guard<std::mutex> const _(mtx_);

      Worker const& worker (worker_map_.at (worker_id));

      std::unordered_set<sdpa::job_id_t> jobs_to_reschedule
        ( worker.pending_.begin()
        , worker.pending_.end()
        );

      std::unordered_set<sdpa::job_id_t> jobs_to_cancel;
      std::set_union ( worker.submitted_.begin()
                     , worker.submitted_.end()
                     , worker.acknowledged_.begin()
                     , worker.acknowledged_.end()
                     , std::inserter (jobs_to_cancel, jobs_to_cancel.begin())
                      );

      for (job_id_t const& jobId : jobs_to_cancel)
      {
        Job* const pJob = get_job (jobId);
        fhg_assert (pJob);

        scheduler::Reservation* reservation (get_reservation (jobId));
        pJob->Reschedule();

        //! \note would never be set otherwise (function is only
        //! called after a worker died)
        reservation->mark_as_canceled_if_no_result_stored_yet (worker_id);

        bool const cancel_already_requested
          ( pJob->getStatus() == sdpa::status::CANCELING
          || pJob->getStatus() == sdpa::status::CANCELED
          );

        if ( !reservation->apply_to_workers_without_result // false for worker 5, true for worker 4
               ( [&jobId, &cancel_already_requested, &cancel_worker_job]
                 (worker_id_t const& wid)
                 {
                   if (!cancel_already_requested)
                   {
                     cancel_worker_job (wid, jobId);
                   }
                 }
               )
           )
        {
          jobs_to_reschedule.emplace (jobId);
        }
      }

      return jobs_to_reschedule;
    }

    unsigned long WorkerManager::num_free_workers() const
    {
      std::lock_guard<std::mutex> const _ (mtx_);
      return _num_free_workers;
    }
  }
}
