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

fhg_add_test (NAME sdpa_network_strategy
  SOURCES network_strategy.cpp
  USE_BOOST
  LIBRARIES sdpa we-dev
            fhgcom
            test-utilities
            Boost::filesystem
)

fhg_add_test (NAME sdpa_job_states
  SOURCES job_states.cpp
  USE_BOOST
  LIBRARIES fhg-util
)
