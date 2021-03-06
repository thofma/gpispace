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

fhg_add_test (NAME drts_should_handle_multiple_workflows_being_submitted_during_lifetime
  SOURCES drts_should_handle_multiple_workflows_being_submitted_during_lifetime.cpp
  USE_BOOST
  ARGS --gspc-home "${CMAKE_INSTALL_PREFIX}"
       --source-directory "${CMAKE_SOURCE_DIR}/share/selftest"
       --shared-directory "${SHARED_DIRECTORY_FOR_TESTS}"
  LIBRARIES gspc
            test-utilities
            fhg-util
            Boost::program_options
            Boost::filesystem
  REQUIRES_INSTALLATION
  START_SCOPED_RIF
)

fhg_add_test (NAME id_generator
  SOURCES id_generator.cpp
  USE_BOOST
  LIBRARIES sdpa
)
