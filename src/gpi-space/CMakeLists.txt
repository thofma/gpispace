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

extended_add_library (NAME gpi-space-gpiapi
  SOURCES "gpi/system.cpp"
          "gpi/gaspi.cpp"
  LIBRARIES GASPI::static
            gspc::logging
            Util::Generic
            GPISpace::vmem
  SYSTEM_INCLUDE_DIRECTORIES PRIVATE "${LIBRT_INCLUDE_DIR}"
)

extended_add_library (NAME gpi-space-pc-client-static
  SOURCES "pc/client/api.cpp"
  LIBRARIES Util::Generic
            gspc::logging
  SYSTEM_INCLUDE_DIRECTORIES PRIVATE "${LIBRT_INCLUDE_DIR}"
)

extended_add_library (NAME gpi-space-pc-segment
  SOURCES "pc/segment/segment.cpp"
  LIBRARIES "${LIBRT_SHARED_LIBRARY}"
            Util::Generic
            gspc::logging
  SYSTEM_INCLUDE_DIRECTORIES PRIVATE "${LIBRT_INCLUDE_DIR}"
)

extended_add_library (NAME gpi-space-pc-memory
  SOURCES "pc/memory/memory_area.cpp"
          "pc/memory/handle_generator.cpp"
          "pc/memory/manager.cpp"
          "pc/memory/gaspi_area.cpp"
          "pc/memory/shm_area.cpp"
          "pc/memory/beegfs_area.cpp"
  LIBRARIES gpi-space-pc-segment
            gspc::logging
            mmgr
            gpi-space_url
            Util::Generic
            gpi-space-gpiapi
  SYSTEM_INCLUDE_DIRECTORIES PRIVATE "${LIBRT_INCLUDE_DIR}"
)

extended_add_library (NAME gpi-space-pc-container
  SOURCES "pc/container/manager.cpp"
  LIBRARIES Util::Generic
            gspc::logging
  SYSTEM_INCLUDE_DIRECTORIES PRIVATE "${LIBRT_INCLUDE_DIR}"
)

extended_add_library (NAME gpi-space-pc-global
  SOURCES "pc/global/topology.cpp"
  LIBRARIES Util::Generic RPC
            gspc::logging
  SYSTEM_INCLUDE_DIRECTORIES PRIVATE "${LIBRT_INCLUDE_DIR}"
)

extended_add_library (NAME gpi-space_url
  SOURCES "pc/url.cpp"
  LIBRARIES fhg-util
)

extended_add_library (NAME gpi_type_check
  SOURCES "types.cpp"
  LIBRARIES gpi-space-gpiapi
)

add_subdirectory (pc/test)
add_subdirectory (tests)
