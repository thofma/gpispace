# This file is part of GPI-Space.
# Copyright (C) 2020 Fraunhofer ITWM
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.

add_subdirectory (worker)

install (FILES
  certificates.hpp
  client.fwd.hpp
  client.hpp
  drts.fwd.hpp
  drts.hpp
  pimpl.hpp
  rifd_entry_points.fwd.hpp
  rifd_entry_points.hpp
  scoped_rifd.fwd.hpp
  scoped_rifd.hpp
  stream.fwd.hpp
  stream.hpp
  information_to_reattach.fwd.hpp
  information_to_reattach.hpp
  virtual_memory.fwd.hpp
  virtual_memory.hpp
  worker_description.hpp
  DESTINATION include/drts/
)

extended_add_library (NAME gspc
  TYPE SHARED
  SOURCES "client.cpp"
          "drts.cpp"
          "information_to_reattach.cpp"
          "private/option.cpp"
          "private/startup_and_shutdown.cpp"
          "rifd_entry_points.cpp"
          "scoped_rifd.cpp"
          "stream.cpp"
          "virtual_memory.cpp"
  LIBRARIES sdpa
            pnet
            rif-strategies
            gpi-space-pc-client-static
            gpi-space-pc-segment
            GPISpace::vmem
            fhg-revision
            Util::Generic
            Boost::filesystem
            Boost::program_options
  INSTALL
)

add_subdirectory (test/add_worker)
add_subdirectory (test/remove_worker)
add_subdirectory (test/parallel_running_workflows)
add_subdirectory (test/put_token)
add_subdirectory (test/workflow_response)
add_subdirectory (test/forbid_double_worker_instances)
add_subdirectory (test/use_fixed_ports_for_agents_and_workers)
add_subdirectory (test/multiple_module_implementations)
add_subdirectory (test/work_stealing)
add_subdirectory (test/run_bunch_of_tasks)
add_subdirectory (test)
