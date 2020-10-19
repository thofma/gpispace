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

#include <boost/test/unit_test.hpp>

#include <drts/client.hpp>
#include <drts/drts.hpp>
#include <drts/scoped_rifd.hpp>

#include <test/make.hpp>
#include <test/parse_command_line.hpp>
#include <test/scoped_nodefile_from_environment.hpp>
#include <test/source_directory.hpp>
#include <test/shared_directory.hpp>

#include <we/type/value.hpp>
#include <we/type/value/boost/test/printer.hpp>

#include <util-generic/temporary_path.hpp>
#include <util-generic/testing/flatten_nested_exceptions.hpp>
#include <util-generic/testing/printer/multimap.hpp>
#include <util-generic/testing/require_container_is_permutation.hpp>

#include <boost/program_options.hpp>
#include <boost/range/adaptor/map.hpp>

#include <map>

namespace
{
  std::multimap<std::string, pnet::type::value::value_type>
    get_result (std::string const& main, long n)
  {
    boost::program_options::options_description options_description;

    options_description.add (test::options::source_directory());
    options_description.add (test::options::shared_directory());
    options_description.add (gspc::options::installation());
    options_description.add (gspc::options::drts());
    options_description.add (gspc::options::scoped_rifd());

    boost::program_options::variables_map vm
      ( test::parse_command_line
          ( boost::unit_test::framework::master_test_suite().argc
          , boost::unit_test::framework::master_test_suite().argv
          , options_description
          )
      );

    fhg::util::temporary_path const shared_directory
      (test::shared_directory (vm) / main);

    test::scoped_nodefile_from_environment const nodefile_from_environment
      (shared_directory, vm);

    fhg::util::temporary_path const _installation_dir
      (shared_directory / boost::filesystem::unique_path());
    boost::filesystem::path const installation_dir (_installation_dir);

    gspc::set_application_search_path (vm, installation_dir);

    vm.notify();

    gspc::installation const installation (vm);

    test::make_net const make
      ( installation
      , main
      , test::source_directory (vm)
      );

    gspc::scoped_rifds const rifds ( gspc::rifd::strategy {vm}
                                 , gspc::rifd::hostnames {vm}
                                 , gspc::rifd::port {vm}
                                 , installation
                                 );
    gspc::scoped_runtime_system const drts
      (vm, installation, "work:4", rifds.entry_points());

    return gspc::client (drts).put_and_run
      (gspc::workflow (make.pnet()), {{"n", n}});
  }
}

BOOST_AUTO_TEST_CASE (share_example_use_sequence)
{
  long const n (10);

  std::multimap<std::string, pnet::type::value::value_type> const result
    (get_result ("use_sequence", n));

  std::multimap<std::string, pnet::type::value::value_type> expected;
  for (long i (0); i < n; ++i)
  {
    for (long j (0); j < n - i - 1; ++j)
    {
      expected.emplace ("out", i);
    }
  }
  FHG_UTIL_TESTING_REQUIRE_CONTAINER_IS_PERMUTATION (result, expected);
}

BOOST_AUTO_TEST_CASE (share_example_use_sequence_bounded)
{
  long const n (10);

  std::multimap<std::string, pnet::type::value::value_type> const result
    (get_result ("use_sequence_bounded", n));

  std::multimap<std::string, pnet::type::value::value_type> expected;
  for (long i (0); i < n; ++i)
  {
    expected.emplace ("out", i);
  }
  FHG_UTIL_TESTING_REQUIRE_CONTAINER_IS_PERMUTATION (result, expected);
}