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

#include <we/type/signature.hpp>
#include <we/type/signature/show.hpp>
#include <we/type/signature/cpp.hpp>
#include <we/type/signature/dump.hpp>
#include <we/type/signature/names.hpp>
#include <we/type/signature/resolve.hpp>

#include <we/type/signature/boost/test/printer.hpp>

#include <we/exception.hpp>

#include <util-generic/testing/flatten_nested_exceptions.hpp>

#include <sstream>
#include <unordered_map>

BOOST_AUTO_TEST_CASE (signature_show)
{
#define CHECK(_expected,_sig...)                \
  {                                             \
    using pnet::type::signature::show;          \
                                                \
    std::ostringstream oss;                     \
                                                \
    oss << show (signature_type (_sig));        \
                                                \
    BOOST_CHECK_EQUAL (oss.str(), _expected);   \
  }

  using pnet::type::signature::signature_type;
  using pnet::type::signature::structured_type;
  using pnet::type::signature::structure_type;
  using pnet::type::signature::field_type;

  CHECK ("float", std::string ("float"));

  structure_type f;

  CHECK ("s :: []", structured_type (std::make_pair ("s", f)));

  f.push_back (std::make_pair (std::string ("x"), std::string ("float")));

  CHECK ("s :: [x :: float]", structured_type (std::make_pair ("s", f)));

  f.push_back (std::make_pair (std::string ("y"), std::string ("float")));

  CHECK ( "point2D :: [x :: float, y :: float]"
        , structured_type (std::make_pair ("point2D", f))
        );

  structure_type ps;

  ps.push_back (std::make_pair (std::string ("p"), std::string ("point2D")));
  ps.push_back (structured_type (std::make_pair ("q", f)));

  signature_type line2D (structured_type (std::make_pair ("line2D", ps)));

  CHECK ("line2D :: [p :: point2D, q :: [x :: float, y :: float]]", line2D);

#undef CHECK

  {
    std::unordered_set<std::string> n (pnet::type::signature::names (line2D));

    BOOST_REQUIRE_EQUAL (n.size(), 2);
    BOOST_REQUIRE_EQUAL (n.count("point2D"), 1);
    BOOST_REQUIRE_EQUAL (n.count("float"), 1);
  }

  ps.push_back (std::make_pair (std::string ("l"), std::string ("list")));

  {
    signature_type s (structured_type (std::make_pair ("s", ps)));

    std::unordered_set<std::string> n (pnet::type::signature::names (s));

    BOOST_REQUIRE_EQUAL (n.size(), 3);
    BOOST_REQUIRE_EQUAL (n.count("point2D"), 1);
    BOOST_REQUIRE_EQUAL (n.count("float"), 1);
    BOOST_REQUIRE_EQUAL (n.count("list"), 1);
  }
}

BOOST_AUTO_TEST_CASE (signature_dump)
{
#define CHECK(_expected,_sig...)                        \
  {                                                     \
    using pnet::type::signature::dump;                  \
                                                        \
    std::ostringstream oss;                             \
                                                        \
    oss << dump (structured_type (_sig));               \
                                                        \
    BOOST_CHECK_EQUAL (oss.str(), _expected);           \
  }

  using pnet::type::signature::structured_type;
  using pnet::type::signature::structure_type;
  using pnet::type::signature::field_type;

  structure_type f;

  CHECK ( "<struct name=\"s\"/>\n"
        , structured_type (std::make_pair ("s", f))
        );

  f.push_back (std::make_pair (std::string ("x"), std::string ("float")));

  CHECK ( "<struct name=\"s\">\n"
          "  <field name=\"x\" type=\"float\"/>\n"
          "</struct>\n"
        , structured_type (std::make_pair ("s", f))
        );

  f.push_back (std::make_pair (std::string ("y"), std::string ("float")));

  CHECK ( "<struct name=\"point2D\">\n"
          "  <field name=\"x\" type=\"float\"/>\n"
          "  <field name=\"y\" type=\"float\"/>\n"
          "</struct>\n"
        , structured_type (std::make_pair ("point2D", f))
        );

  structure_type ps;

  ps.push_back (std::make_pair (std::string ("p"), std::string ("point2D")));
  ps.push_back (structured_type (std::make_pair ("q", f)));

  CHECK ( "<struct name=\"line2D\">\n"
          "  <field name=\"p\" type=\"point2D\"/>\n"
          "  <struct name=\"q\">\n"
          "    <field name=\"x\" type=\"float\"/>\n"
          "    <field name=\"y\" type=\"float\"/>\n"
          "  </struct>\n"
          "</struct>\n"
        , structured_type (std::make_pair ("line2D", ps))
        );

#undef CHECK
}

BOOST_AUTO_TEST_CASE (signature_cpp)
{
#define CHECK_HEADER(_expected,_sig...)                 \
  {                                                     \
    using pnet::type::signature::cpp::header;           \
                                                        \
    std::ostringstream oss;                             \
                                                        \
    oss << header (structured_type (_sig));             \
                                                        \
    BOOST_CHECK_EQUAL (oss.str(), _expected);           \
  }
#define CHECK_HEADER_OP(_expected,_sig...)              \
  {                                                     \
    using pnet::type::signature::cpp::header_op;        \
                                                        \
    std::ostringstream oss;                             \
                                                        \
    oss << header_op (structured_type (_sig));          \
                                                        \
    BOOST_CHECK_EQUAL (oss.str(), _expected);           \
  }
#define CHECK_IMPL(_expected,_sig...)                   \
  {                                                     \
    using pnet::type::signature::cpp::impl;             \
                                                        \
    std::ostringstream oss;                             \
                                                        \
    oss << impl (structured_type (_sig));               \
                                                        \
    BOOST_CHECK_EQUAL (oss.str(), _expected);           \
  }

  using pnet::type::signature::structured_type;
  using pnet::type::signature::structure_type;
  using pnet::type::signature::field_type;

  CHECK_HEADER
    ("\n"
     "namespace pnetc\n"
     "{\n"
     "  namespace type\n"
     "  {\n"
     "    namespace empty\n"
     "    {\n"
     "      struct empty\n"
     "      {\n"
     "        empty()\n"
     "        {}\n"
     "        bool operator== (empty const& rhs) const\n"
     "        {\n"
     "          return true\n"
     "            ;\n"
     "        }\n"
     "        bool operator< (empty const& rhs) const\n"
     "        {\n"
     "          return false;\n"
     "        }\n"
     "      };\n"
     "    }\n"
     "  }\n"
     "}"
     , std::make_pair (std::string ("empty"), structure_type())
    );

  CHECK_HEADER_OP
    ("#include <we/type/value.hpp>\n"
     "#include <we/type/value/to_value.hpp>\n"
     "#include <we/type/value/from_value.hpp>\n"
     "#include <iosfwd>\n"
     "\n"
     "namespace pnetc\n"
     "{\n"
     "  namespace type\n"
     "  {\n"
     "    namespace empty\n"
     "    {\n"
     "      empty from_value (const pnet::type::value::value_type&);\n"
     "      pnet::type::value::value_type to_value (const empty&);\n"
     "      std::ostream& operator<< (std::ostream&, const empty&);\n"
     "    }\n"
     "  }\n"
     "}\n"
     "namespace pnet\n"
     "{\n"
     "  namespace type\n"
     "  {\n"
     "    namespace value\n"
     "    {\n"
     "      template<>\n"
     "        inline value_type to_value<pnetc::type::empty::empty> (const pnetc::type::empty::empty& x)\n"
     "      {\n"
     "        return pnetc::type::empty::to_value (x);\n"
     "      }\n"
     "      template<>\n"
     "        inline pnetc::type::empty::empty from_value<pnetc::type::empty::empty> (value_type const& v)\n"
     "      {\n"
     "        return pnetc::type::empty::from_value (v);\n"
     "      }\n"
     "    }\n"
     "  }\n"
     "}"
     , std::make_pair (std::string ("empty"), structure_type())
    );

  CHECK_IMPL
    ("#include <we/field.hpp>\n"
     "#include <we/signature_of.hpp>\n"
     "#include <we/type/value/poke.hpp>\n"
     "#include <we/type/value/show.hpp>\n"
     "#include <iostream>\n"
     "\n"
     "namespace pnetc\n"
     "{\n"
     "  namespace type\n"
     "  {\n"
     "    namespace empty\n"
     "    {\n"
     "      empty from_value (const pnet::type::value::value_type& v)\n"
     "      {\n"
     "        return empty();\n"
     "      }\n"
     "      pnet::type::value::value_type to_value (const empty& x)\n"
     "      {\n"
     "        pnet::type::value::value_type v;\n"
     "        return v;\n"
     "      }\n"
     "      std::ostream& operator<< (std::ostream& os, const empty& x)\n"
     "      {\n"
     "        return os << pnet::type::value::show (to_value (x));\n"
     "      }\n"
     "    }\n"
     "  }\n"
     "}"
     , std::make_pair (std::string ("empty"), structure_type())
    );

  structure_type f;
  f.push_back (std::make_pair (std::string ("x"), std::string ("float")));
  f.push_back (std::make_pair (std::string ("y"), std::string ("float")));

  CHECK_HEADER
    ("\n"
     "namespace pnetc\n"
     "{\n"
     "  namespace type\n"
     "  {\n"
     "    namespace point2D\n"
     "    {\n"
     "      struct point2D\n"
     "      {\n"
     "        float x;\n"
     "        float y;\n"
     "        point2D()\n"
     "          : x()\n"
     "          , y()\n"
     "        {}\n"
     "        explicit point2D\n"
     "          ( float const& _x\n"
     "          , float const& _y\n"
     "          )\n"
     "          : x (_x)\n"
     "          , y (_y)\n"
     "        {}\n"
     "        bool operator== (point2D const& rhs) const\n"
     "        {\n"
     "          return true\n"
     "            && (this->x == rhs.x)\n"
     "            && (this->y == rhs.y)\n"
     "            ;\n"
     "        }\n"
     "        bool operator< (point2D const& rhs) const\n"
     "        {\n"
     "          return (this->x < rhs.x) || ((this->x == rhs.x) && ((this->y < rhs.y)));\n"
     "        }\n"
     "      };\n"
     "    }\n"
     "  }\n"
     "}"
     , std::make_pair ("point2D", f)
    );

  CHECK_HEADER_OP
    ("#include <we/type/value.hpp>\n"
     "#include <we/type/value/to_value.hpp>\n"
     "#include <we/type/value/from_value.hpp>\n"
     "#include <iosfwd>\n"
     "\n"
     "namespace pnetc\n"
     "{\n"
     "  namespace type\n"
     "  {\n"
     "    namespace point2D\n"
     "    {\n"
     "      point2D from_value (const pnet::type::value::value_type&);\n"
     "      pnet::type::value::value_type to_value (const point2D&);\n"
     "      std::ostream& operator<< (std::ostream&, const point2D&);\n"
     "    }\n"
     "  }\n"
     "}\n"
     "namespace pnet\n"
     "{\n"
     "  namespace type\n"
     "  {\n"
     "    namespace value\n"
     "    {\n"
     "      template<>\n"
     "        inline value_type to_value<pnetc::type::point2D::point2D> (const pnetc::type::point2D::point2D& x)\n"
     "      {\n"
     "        return pnetc::type::point2D::to_value (x);\n"
     "      }\n"
     "      template<>\n"
     "        inline pnetc::type::point2D::point2D from_value<pnetc::type::point2D::point2D> (value_type const& v)\n"
     "      {\n"
     "        return pnetc::type::point2D::from_value (v);\n"
     "      }\n"
     "    }\n"
     "  }\n"
     "}"
     , std::make_pair ("point2D", f)
    );

  CHECK_IMPL
    ("#include <we/field.hpp>\n"
     "#include <we/signature_of.hpp>\n"
     "#include <we/type/value/poke.hpp>\n"
     "#include <we/type/value/show.hpp>\n"
     "#include <iostream>\n"
     "\n"
     "namespace pnetc\n"
     "{\n"
     "  namespace type\n"
     "  {\n"
     "    namespace point2D\n"
     "    {\n"
     "      point2D from_value (const pnet::type::value::value_type& v)\n"
     "      {\n"
     "        return point2D\n"
     "          ( pnet::field_as< float > (\"x\", v, std::string(\"float\"))\n"
     "          , pnet::field_as< float > (\"y\", v, std::string(\"float\"))\n"
     "          );\n"
     "      }\n"
     "      pnet::type::value::value_type to_value (const point2D& x)\n"
     "      {\n"
     "        pnet::type::value::value_type v;\n"
     "        pnet::type::value::poke (\"x\", v, x.x);\n"
     "        pnet::type::value::poke (\"y\", v, x.y);\n"
     "        return v;\n"
     "      }\n"
     "      std::ostream& operator<< (std::ostream& os, const point2D& x)\n"
     "      {\n"
     "        return os << pnet::type::value::show (to_value (x));\n"
     "      }\n"
     "    }\n"
     "  }\n"
     "}"
     , std::make_pair ("point2D", f)
    );

  {
    structure_type a;
    a.push_back (std::make_pair (std::string ("i"), std::string ("int")));
    structure_type b;
    b.push_back (structured_type (std::make_pair ("a", a)));
    structure_type c;
    c.push_back (structured_type (std::make_pair ("b", b)));

    CHECK_HEADER
      ("\n"
       "namespace pnetc\n"
       "{\n"
       "  namespace type\n"
       "  {\n"
       "    namespace c\n"
       "    {\n"
       "      namespace b\n"
       "      {\n"
       "        namespace a\n"
       "        {\n"
       "          struct a\n"
       "          {\n"
       "            int i;\n"
       "            a()\n"
       "              : i()\n"
       "            {}\n"
       "            explicit a\n"
       "              ( int const& _i\n"
       "              )\n"
       "              : i (_i)\n"
       "            {}\n"
       "            bool operator== (a const& rhs) const\n"
       "            {\n"
       "              return true\n"
       "                && (this->i == rhs.i)\n"
       "                ;\n"
       "            }\n"
       "            bool operator< (a const& rhs) const\n"
       "            {\n"
       "              return (this->i < rhs.i);\n"
       "            }\n"
       "          };\n"
       "        }\n"
       "        struct b\n"
       "        {\n"
       "          a::a a;\n"
       "          b()\n"
       "            : a()\n"
       "          {}\n"
       "          explicit b\n"
       "            ( a::a const& _a\n"
       "            )\n"
       "            : a (_a)\n"
       "          {}\n"
       "          bool operator== (b const& rhs) const\n"
       "          {\n"
       "            return true\n"
       "              && (this->a == rhs.a)\n"
       "              ;\n"
       "          }\n"
       "          bool operator< (b const& rhs) const\n"
       "          {\n"
       "            return (this->a < rhs.a);\n"
       "          }\n"
       "        };\n"
       "      }\n"
       "      struct c\n"
       "      {\n"
       "        b::b b;\n"
       "        c()\n"
       "          : b()\n"
       "        {}\n"
       "        explicit c\n"
       "          ( b::b const& _b\n"
       "          )\n"
       "          : b (_b)\n"
       "        {}\n"
       "        bool operator== (c const& rhs) const\n"
       "        {\n"
       "          return true\n"
       "            && (this->b == rhs.b)\n"
       "            ;\n"
       "        }\n"
       "        bool operator< (c const& rhs) const\n"
       "        {\n"
       "          return (this->b < rhs.b);\n"
       "        }\n"
       "      };\n"
       "    }\n"
       "  }\n"
       "}"
      , std::make_pair ("c", c)
      );

    CHECK_HEADER_OP
      ("#include <we/type/value.hpp>\n"
       "#include <we/type/value/to_value.hpp>\n"
       "#include <we/type/value/from_value.hpp>\n"
       "#include <iosfwd>\n"
       "\n"
       "namespace pnetc\n"
       "{\n"
       "  namespace type\n"
       "  {\n"
       "    namespace c\n"
       "    {\n"
       "      namespace b\n"
       "      {\n"
       "        namespace a\n"
       "        {\n"
       "          a from_value (const pnet::type::value::value_type&);\n"
       "          pnet::type::value::value_type to_value (const a&);\n"
       "          std::ostream& operator<< (std::ostream&, const a&);\n"
       "        }\n"
       "        b from_value (const pnet::type::value::value_type&);\n"
       "        pnet::type::value::value_type to_value (const b&);\n"
       "        std::ostream& operator<< (std::ostream&, const b&);\n"
       "      }\n"
       "      c from_value (const pnet::type::value::value_type&);\n"
       "      pnet::type::value::value_type to_value (const c&);\n"
       "      std::ostream& operator<< (std::ostream&, const c&);\n"
       "    }\n"
       "  }\n"
       "}\n"
       "namespace pnet\n"
       "{\n"
       "  namespace type\n"
       "  {\n"
       "    namespace value\n"
       "    {\n"
       "      template<>\n"
       "        inline value_type to_value<pnetc::type::c::b::a::a> (const pnetc::type::c::b::a::a& x)\n"
       "      {\n"
       "        return pnetc::type::c::b::a::to_value (x);\n"
       "      }\n"
       "      template<>\n"
       "        inline pnetc::type::c::b::a::a from_value<pnetc::type::c::b::a::a> (value_type const& v)\n"
       "      {\n"
       "        return pnetc::type::c::b::a::from_value (v);\n"
       "      }\n"
       "      template<>\n"
       "        inline value_type to_value<pnetc::type::c::b::b> (const pnetc::type::c::b::b& x)\n"
       "      {\n"
       "        return pnetc::type::c::b::to_value (x);\n"
       "      }\n"
       "      template<>\n"
       "        inline pnetc::type::c::b::b from_value<pnetc::type::c::b::b> (value_type const& v)\n"
       "      {\n"
       "        return pnetc::type::c::b::from_value (v);\n"
       "      }\n"
       "      template<>\n"
       "        inline value_type to_value<pnetc::type::c::c> (const pnetc::type::c::c& x)\n"
       "      {\n"
       "        return pnetc::type::c::to_value (x);\n"
       "      }\n"
       "      template<>\n"
       "        inline pnetc::type::c::c from_value<pnetc::type::c::c> (value_type const& v)\n"
       "      {\n"
       "        return pnetc::type::c::from_value (v);\n"
       "      }\n"
       "    }\n"
       "  }\n"
       "}"
      , std::make_pair ("c", c)
      );

    CHECK_IMPL
      ("#include <we/field.hpp>\n"
       "#include <we/signature_of.hpp>\n"
       "#include <we/type/value/poke.hpp>\n"
       "#include <we/type/value/show.hpp>\n"
       "#include <iostream>\n"
       "\n"
       "namespace pnetc\n"
       "{\n"
       "  namespace type\n"
       "  {\n"
       "    namespace c\n"
       "    {\n"
       "      namespace b\n"
       "      {\n"
       "        namespace a\n"
       "        {\n"
       "          a from_value (const pnet::type::value::value_type& v)\n"
       "          {\n"
       "            return a\n"
       "              ( pnet::field_as< int > (\"i\", v, std::string(\"int\"))\n"
       "              );\n"
       "          }\n"
       "          pnet::type::value::value_type to_value (const a& x)\n"
       "          {\n"
       "            pnet::type::value::value_type v;\n"
       "            pnet::type::value::poke (\"i\", v, x.i);\n"
       "            return v;\n"
       "          }\n"
       "          std::ostream& operator<< (std::ostream& os, const a& x)\n"
       "          {\n"
       "            return os << pnet::type::value::show (to_value (x));\n"
       "          }\n"
       "        }\n"
       "        b from_value (const pnet::type::value::value_type& v)\n"
       "        {\n"
       "          return b\n"
       "            ( a::from_value (pnet::field (\"a\", v, pnet::signature_of (a::to_value (a::a()))))\n"
       "            );\n"
       "        }\n"
       "        pnet::type::value::value_type to_value (const b& x)\n"
       "        {\n"
       "          pnet::type::value::value_type v;\n"
       "          pnet::type::value::poke (\"a\", v, a::to_value (x.a));\n"
       "          return v;\n"
       "        }\n"
       "        std::ostream& operator<< (std::ostream& os, const b& x)\n"
       "        {\n"
       "          return os << pnet::type::value::show (to_value (x));\n"
       "        }\n"
       "      }\n"
       "      c from_value (const pnet::type::value::value_type& v)\n"
       "      {\n"
       "        return c\n"
       "          ( b::from_value (pnet::field (\"b\", v, pnet::signature_of (b::to_value (b::b()))))\n"
       "          );\n"
       "      }\n"
       "      pnet::type::value::value_type to_value (const c& x)\n"
       "      {\n"
       "        pnet::type::value::value_type v;\n"
       "        pnet::type::value::poke (\"b\", v, b::to_value (x.b));\n"
       "        return v;\n"
       "      }\n"
       "      std::ostream& operator<< (std::ostream& os, const c& x)\n"
       "      {\n"
       "        return os << pnet::type::value::show (to_value (x));\n"
       "      }\n"
       "    }\n"
       "  }\n"
       "}"
      , std::make_pair ("c", c)
      );
  }
#undef CHECK
}

namespace
{
  using pnet::type::signature::signature_type;

  class resolver
  {
  public:
    resolver (const std::unordered_map<std::string, signature_type>& m)
      : _m (m)
    {}
    boost::optional<signature_type> operator() (const std::string& key) const
    {
      std::unordered_map<std::string, signature_type>::const_iterator
        pos (_m.find (key));

      if (pos == _m.end())
      {
        return boost::none;
      }

      return pos->second;
    }
  private:
    const std::unordered_map<std::string, signature_type>& _m;
  };
}

BOOST_AUTO_TEST_CASE (resolve)
{
  using pnet::type::signature::signature_type;
  using pnet::type::signature::structured_type;
  using pnet::type::signature::structure_type;
  using pnet::type::signature::field_type;
  using pnet::type::signature::resolver_type;
  using pnet::type::signature::resolve;

  structure_type point_fields;
  point_fields.push_back (std::make_pair ( std::string ("x")
                                         , std::string ("double")
                                         )
                         );
  point_fields.push_back (std::make_pair ( std::string ("y")
                                         , std::string ("double")
                                         )
                         );
  const structured_type point (std::make_pair ("point", point_fields));
  structure_type circle_fields;
  circle_fields.push_back (std::make_pair ( std::string ("center")
                                          , std::string ("point")
                                          )
                          );
  circle_fields.push_back (std::make_pair ( std::string ("radius")
                                          , std::string ("double")
                                          )
                          );
  const structured_type circle (std::make_pair ("circle", circle_fields));

  structure_type circle_resolved_fields;
  circle_resolved_fields.push_back
    (structured_type (std::make_pair ( std::string ("center")
                                     , point_fields
                                     )
                     )
    );
  circle_resolved_fields.push_back (std::make_pair ( std::string ("radius")
                                                   , std::string ("double")
                                                   )
                                   );
  const structured_type circle_resolved
    (std::make_pair ("circle", circle_resolved_fields));


  std::unordered_map<std::string, signature_type> resolver_map;

  BOOST_CHECK_THROW ( resolve (circle, resolver (resolver_map))
                    , pnet::exception::could_not_resolve
                    );

  resolver_map.insert (std::make_pair (std::string ("point"), point));

  BOOST_REQUIRE_EQUAL ( signature_type (circle_resolved)
                      , resolve (circle,resolver (resolver_map))
                      );
}
