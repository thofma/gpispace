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

#include <we/loader/Module.hpp>

#include <we/expr/eval/context.hpp>
#include <we/loader/api-guard.hpp>
#include <we/loader/exceptions.hpp>
#include <we/type/value/boost/test/printer.hpp>

#include <drts/worker/context.hpp>
#include <drts/worker/context_impl.hpp>

#include <util-generic/testing/flatten_nested_exceptions.hpp>
#include <util-generic/testing/require_exception.hpp>

#include <boost/asio/io_service.hpp>
#include <boost/format.hpp>
#include <boost/version.hpp>

BOOST_AUTO_TEST_CASE (ctor_load_failed)
{
  fhg::util::testing::require_exception
    ( [] { we::loader::Module ("<path>"); }
    , we::loader::module_load_failed
        ( "<path>"
        , "dlopen: <path>: cannot open shared object file: No such file or directory"
        )
    );
}

BOOST_AUTO_TEST_CASE (ctor_failed_exception_from_we_mod_initialize)
{
  fhg::util::testing::require_exception
    ( [] { we::loader::Module ("./libinitialize_throws.so"); }
    , we::loader::module_load_failed
        ( "./libinitialize_throws.so"
        , "initialize_throws"
        )
    );
}

BOOST_AUTO_TEST_CASE (ctor_failed_bad_boost_version)
{
#define XSTR(x) STR(x)
#define STR(x) #x
  fhg::util::testing::require_exception
    ( [] { we::loader::Module ("./libempty_not_linked_with_pnet.so"); }
    , we::loader::module_load_failed
        ( "./libempty_not_linked_with_pnet.so"
        , ( boost::format
              ("dlopen: ./libempty_not_linked_with_pnet.so: undefined symbol: %1%")
          % XSTR (WE_GUARD_SYMBOL)
          ).str()
        )
    );
#undef STR
#undef XSTR
}

BOOST_AUTO_TEST_CASE (call_not_found)
{
  we::loader::Module m ("./libempty.so");

  fhg::logging::stream_emitter logger;
  drts::worker::context context
    (drts::worker::context_constructor
      ("noname", (std::set<std::string>()), logger)
    );
  expr::eval::context input;
  expr::eval::context output;

  fhg::util::testing::require_exception
    ( [&m, &context, &input, &output]
    {
      m.call ( "<name>", &context, input, output
             , std::map<std::string, void*>()
             );
    }
    , we::loader::function_not_found ("./libempty.so", "<name>")
    );
}

namespace
{
  static int x;

  void inc ( drts::worker::context*
           , expr::eval::context const&
           , expr::eval::context&
           , std::map<std::string, void*> const&
           )
  {
    ++x;
  }
}

BOOST_AUTO_TEST_CASE (call_local)
{
  we::loader::Module m ("./libempty.so");
  m.add_function ("f", &inc);

  fhg::logging::stream_emitter logger;
  drts::worker::context context
    (drts::worker::context_constructor
      ("noname", (std::set<std::string>()), logger)
    );
  expr::eval::context input;
  expr::eval::context output;

  x=0;

  m.call ("f", &context, input, output, std::map<std::string, void*>());

  BOOST_REQUIRE_EQUAL (x, 1);
}

BOOST_AUTO_TEST_CASE (call_lib)
{
  we::loader::Module m ("./libanswer.so");

  fhg::logging::stream_emitter logger;
  drts::worker::context context
    (drts::worker::context_constructor
      ("noname", (std::set<std::string>()), logger)
    );
  expr::eval::context input;
  expr::eval::context output;

  m.call ("answer", &context, input, output, std::map<std::string, void*>());

  BOOST_REQUIRE_EQUAL ( output.value ({"out"})
                      , pnet::type::value::value_type (42L)
                      );
}

BOOST_AUTO_TEST_CASE (duplicate_function)
{
  we::loader::Module m ("./libempty.so");
  m.add_function ("f", &inc);

  fhg::util::testing::require_exception
    ( [&m] { m.add_function ("f", &inc); }
    , we::loader::duplicate_function ("./libempty.so", "f")
    );
}

BOOST_AUTO_TEST_CASE (ensures_library_unloads_properly)
{
  // \note Relies on the library not linking anyone in additional to
  // what is already loaded, so that the not-unloaded set is only
  // exactly the library we know about.

  auto const libempty_nodelete ("./libempty_nodelete.so");

  fhg::util::testing::require_exception
    ( [&]
      {
        we::loader::Module
          {we::loader::RequireModuleUnloadsWithoutRest{}, libempty_nodelete};
      }
    , we::loader::module_load_failed
        ( libempty_nodelete
        , we::loader::module_does_not_unload
            (libempty_nodelete, {libempty_nodelete}).what()
        )
    );
}
