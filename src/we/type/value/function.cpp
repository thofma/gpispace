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

#include <we/type/value/function.hpp>

#include <we/exception.hpp>

#include <we/expr/exception.hpp>

#include <limits>

#include <cmath>

namespace pnet
{
  namespace type
  {
    namespace value
    {
      namespace
      {
        class visitor_unary : public boost::static_visitor<value_type>
        {
        public:
          visitor_unary (const expr::token::type& token)
            : _token (token)
          {}

          value_type operator() (bool x) const
          {
            switch (_token)
            {
            case expr::token::_not: return !x;
            case expr::token::_toint: return x ? 1 : 0;
            case expr::token::_tolong: return x ? 1L : 0L;
            case expr::token::_touint: return x ? 1U : 0U;
            case expr::token::_toulong: return x ? 1UL : 0UL;
            default: throw exception::eval (_token, x);
            }
          }
          value_type operator() (int x) const
          {
            return integral_signed (x);
          }
          value_type operator() (long x) const
          {
            return integral_signed (x);
          }
          value_type operator() (unsigned int x) const
          {
            return integral_unsigned (x);
          }
          value_type operator() (unsigned long x) const
          {
            return integral_unsigned (x);
          }

          template<typename T>
            value_type unary_fractional (T x) const
          {
            switch (_token)
            {
            case expr::token::neg: return -x;
            case expr::token::abs: return std::abs (x);
            case expr::token::_sin: return std::sin (x);
            case expr::token::_cos: return std::cos (x);
            case expr::token::_sqrt:
              if (x < 0)
              {
                throw expr::exception::eval::square_root_for_negative_argument<T> (x);
              }
              return std::sqrt (x);
            case expr::token::_log:
              if (!(x > 0))
              {
                throw expr::exception::eval::log_for_nonpositive_argument<T> (x);
              }
              return std::log (x);
            case expr::token::_floor: return std::floor (x);
            case expr::token::_ceil: return std::ceil (x);
            case expr::token::_round: return std::round (x);
            case expr::token::_toint: return static_cast<int> (x);
            case expr::token::_tolong: return static_cast<long> (x);
            case expr::token::_touint: return static_cast<unsigned int> (x);
            case expr::token::_toulong: return static_cast<unsigned long> (x);
            case expr::token::_tofloat: return static_cast<float> (x);
            case expr::token::_todouble: return static_cast<double> (x);
            default: throw exception::eval (_token, x);
            }
          }

          value_type operator() (float x) const
          {
            return unary_fractional (x);
          }
          value_type operator() (double x) const
          {
            return unary_fractional (x);
          }
          value_type operator() (std::string x) const
          {
            switch (_token)
            {
            case expr::token::_bitset_fromhex: return bitsetofint::from_hex (x);
            default: throw exception::eval (_token, x);
            }
          }
          value_type operator() (bitsetofint::type x) const
          {
            switch (_token)
            {
            case expr::token::_bitset_tohex: return bitsetofint::to_hex (x);
            case expr::token::_bitset_count: return x.count();
            default: throw exception::eval (_token, x);
            }
          }
          value_type operator() (we::type::bytearray x) const
          {
            throw exception::eval (_token, x);
          }
          value_type operator() (std::list<value_type> x) const
          {
            switch (_token)
            {
            case expr::token::_stack_empty: return x.empty();
            case expr::token::_stack_top: return x.back();
            case expr::token::_stack_pop: x.pop_back(); return x;
            case expr::token::_stack_size: return x.size();
            default: throw exception::eval (_token, x);
            }
          }
          value_type operator() (std::set<value_type> x) const
          {
            switch (_token)
            {
            case expr::token::_set_pop: x.erase (x.begin()); return x;
            case expr::token::_set_top: return *(x.begin());
            case expr::token::_set_empty: return x.empty();
            case expr::token::_set_size: return x.size();
            default: throw exception::eval (_token, x);
            }
          }
          value_type operator() (std::map<value_type, value_type> x) const
          {
            switch (_token)
            {
            case expr::token::_map_size: return x.size();
            case expr::token::_map_empty: return x.empty();
            default: throw exception::eval (_token, x);
            }
          }
          template<typename T>
            value_type operator() (T x) const
          {
            throw exception::eval (_token, x);
          }

        private:
          const expr::token::type& _token;

          template<typename T>
            value_type integral_signed (const T& x) const
          {
            switch (_token)
            {
            case expr::token::_not: return ~x;
            case expr::token::neg: return -x;
            case expr::token::abs: return std::abs (x);
            case expr::token::_sin: return std::sin (x);
            case expr::token::_cos: return std::cos (x);
            case expr::token::_sqrt:
              if (x < 0)
              {
                throw expr::exception::eval::square_root_for_negative_argument<T> (x);
              }
              return std::sqrt (x);
            case expr::token::_log:
              if (!(x > 0))
              {
                throw expr::exception::eval::log_for_nonpositive_argument<T> (x);
              }
              return std::log (x);
            case expr::token::_floor: return x;
            case expr::token::_ceil: return x;
            case expr::token::_round: return x;
            case expr::token::_toint: return static_cast<int> (x);
            case expr::token::_tolong: return static_cast<long> (x);
            case expr::token::_touint: return static_cast<unsigned int> (x);
            case expr::token::_toulong: return static_cast<unsigned long> (x);
            case expr::token::_tofloat: return static_cast<float> (x);
            case expr::token::_todouble: return static_cast<double> (x);
            default: throw exception::eval (_token, x);
            }
          }
          template<typename T>
            value_type integral_unsigned (const T& x) const
          {
            switch (_token)
            {
            case expr::token::_not: return ~x;
            case expr::token::_sin: return std::sin (x);
            case expr::token::_cos: return std::cos (x);
            case expr::token::_sqrt: return std::sqrt (x);
            case expr::token::_log: return std::log (x);
            case expr::token::_floor: return x;
            case expr::token::_ceil: return x;
            case expr::token::_round: return x;
            case expr::token::_toint: return static_cast<int> (x);
            case expr::token::_tolong: return static_cast<long> (x);
            case expr::token::_touint: return static_cast<unsigned int> (x);
            case expr::token::_toulong: return static_cast<unsigned long> (x);
            case expr::token::_tofloat: return static_cast<float> (x);
            case expr::token::_todouble: return static_cast<double> (x);
            default: throw exception::eval (_token, x);
            }
          }
        };

        class visitor_binary : public boost::static_visitor<value_type>
        {
        public:
          visitor_binary (const expr::token::type& token)
            : _token (token)
          {}

          value_type operator() (bool l, bool r) const
          {
            switch (_token)
            {
            case expr::token::_or_boolean: return l || r;
            case expr::token::_and_boolean: return l && r;
            case expr::token::lt: return l < r;
            case expr::token::le: return l <= r;
            case expr::token::gt: return l > r;
            case expr::token::ge: return l >= r;
            case expr::token::ne: return l != r;
            case expr::token::eq: return l == r;
            case expr::token::min: return std::min (l,r);
            case expr::token::max: return std::max (l,r);
            default: throw exception::eval (_token, l, r);
            }
          }
          value_type operator() (int l, int r) const
          {
            return integral_signed (l, r);
          }
          value_type operator() (long l, long r) const
          {
            return integral_signed (l, r);
          }
          value_type operator() (unsigned int l, unsigned int r) const
          {
            return integral_unsigned (l, r);
          }
          value_type operator() (unsigned long l, unsigned long r) const
          {
            return integral_unsigned (l, r);
          }
          value_type operator() (float l, float r) const
          {
            return fractional (l, r);
          }
          value_type operator() (double l, double r) const
          {
            return fractional (l, r);
          }
          value_type operator() (char l, char r) const
          {
            switch (_token)
            {
            case expr::token::lt: return l < r;
            case expr::token::le: return l <= r;
            case expr::token::gt: return l > r;
            case expr::token::ge: return l >= r;
            case expr::token::ne: return l != r;
            case expr::token::eq: return l == r;
            case expr::token::add: { std::string s; s += l; s += r; return s; }
            case expr::token::min: return std::min (l, r);
            case expr::token::max: return std::max (l, r);
            default: throw exception::eval (_token, l, r);
            }
          }
          value_type operator() (std::string l, std::string r) const
          {
            switch (_token)
            {
            case expr::token::lt: return l < r;
            case expr::token::le: return l <= r;
            case expr::token::gt: return l > r;
            case expr::token::ge: return l >= r;
            case expr::token::ne: return l != r;
            case expr::token::eq: return l == r;
            case expr::token::add: { std::string s; s += l; s += r; return s; }
            case expr::token::min: return std::min (l, r);
            case expr::token::max: return std::max (l, r);
            default: throw exception::eval (_token, l, r);
            }
          }
          value_type operator() (bitsetofint::type l, bitsetofint::type r) const
          {
            switch (_token)
            {
            case expr::token::ne: return ! (l == r);
            case expr::token::eq: return l == r;
            case expr::token::_bitset_or: return l | r;
            case expr::token::_bitset_and: return l & r;
            case expr::token::_bitset_xor: return l ^ r;
            default: throw exception::eval (_token, l, r);
            }
          }
          value_type operator() (bitsetofint::type l, unsigned long r) const
          {
            switch (_token)
            {
            case expr::token::_bitset_insert: l.ins (r); return l;
            case expr::token::_bitset_delete: l.del (r); return l;
            case expr::token::_bitset_is_element: return l.is_element (r);
            default: throw exception::eval (_token, l, r);
            }
          }
          value_type operator() (we::type::bytearray l, we::type::bytearray r) const
          {
            switch (_token)
            {
            case expr::token::ne: return ! (l == r);
            case expr::token::eq: return l == r;
            default: throw exception::eval (_token, l, r);
            }
          }
          value_type operator() ( std::list<value_type> l
                                , std::list<value_type> r
                                ) const
          {
            switch (_token)
            {
            case expr::token::_stack_join:
              while (!r.empty())
              {
                l.push_back (r.front()); r.pop_front();
              }
              return l;
            default: throw exception::eval (_token, l, r);
            }
          }
          template<typename T>
            value_type operator() (std::list<value_type> l, T r) const
          {
            switch (_token)
            {
            case expr::token::_stack_push: l.push_back (r); return l;
            default: throw exception::eval (_token, l, r);
            }
          }
          value_type operator() ( std::set<value_type> l
                                , std::set<value_type> r
                                ) const
          {
            switch (_token)
            {
            case expr::token::_set_is_subset:
              for (const value_type& lv : l)
              {
                if (!r.count (lv))
                {
                  return false;
                }
              }
              return true;
            default: throw exception::eval (_token, l, r);
            }
          }
          template<typename T>
            value_type operator() (std::set<value_type> l, T r) const
          {
            switch (_token)
            {
            case expr::token::_set_insert: l.insert (r); return l;
            case expr::token::_set_erase: l.erase (r); return l;
            case expr::token::_set_is_element: return l.find (r) != l.end();
            default: throw exception::eval (_token, l, r);
            }
          }
          template<typename T>
            value_type operator() (std::map<value_type,value_type> l, T r) const
          {
            switch (_token)
            {
            case expr::token::_map_unassign: l.erase (r); return l;
            case expr::token::_map_is_assigned: return l.find (r) != l.end();
            case expr::token::_map_get_assignment: return l.at (r);
            default: throw exception::eval (_token, l, r);
            }
          }
          value_type operator() (structured_type l, structured_type r) const
          {
            auto const traverse
              ([this, &l, &r] (bool early) -> bool
               {
                 structured_type::const_iterator lpos (l.begin());
                 structured_type::const_iterator rpos (r.begin());

                 while (  lpos != l.end() && rpos!=r.end()
                       && lpos->first == rpos->first
                       )
                 {
                   if (  boost::apply_visitor
                           (*this, lpos->second, rpos->second)
                      == value_type (early)
                      )
                   {
                     return early;
                   }

                   ++lpos; ++rpos;
                 }
                 if (lpos != l.end() || rpos != r.end())
                 {
                   throw exception::eval (_token, l, r);
                 }

                 return !early;
               }
              );

            switch (_token)
            {
            case expr::token::ne: return traverse (true);
            case expr::token::eq: return traverse (false);
            default: throw exception::eval (_token, l, r);
            }
          }
          template<typename L, typename R>
            value_type operator() (const L& l, const R& r) const
          {
            throw exception::eval (_token, l, r);
          }

        private:
          const expr::token::type& _token;

          template<typename T>
            value_type integral_signed (const T l, const T r) const
          {
            switch (_token)
            {
            case expr::token::_or_integral: return l | r;
            case expr::token::_and_integral: return l & r;
            case expr::token::lt: return l < r;
            case expr::token::le: return l <= r;
            case expr::token::gt: return l > r;
            case expr::token::ge: return l >= r;
            case expr::token::ne: return l != r;
            case expr::token::eq: return l == r;
            case expr::token::add: return l + r;
            case expr::token::sub: return l - r;
            case expr::token::mul: return l * r;
            case expr::token::divint:
              if (r == 0) throw expr::exception::eval::divide_by_zero();
              return l / r;
            case expr::token::modint:
              if (r == 0) throw expr::exception::eval::divide_by_zero();
              return l % r;
            case expr::token::_powint:
              if (r < 0) throw expr::exception::eval::negative_exponent();
              {
                T x (1);

                for (T i (0); i < r; ++i) x *= l;

                return x;
              }
            case expr::token::min: return std::min (l, r);
            case expr::token::max: return std::max (l, r);
            default: throw exception::eval (_token, l, r);
            }
          }
          template<typename T>
            value_type integral_unsigned (const T l, const T r) const
          {
            switch (_token)
            {
            case expr::token::_or_integral: return l | r;
            case expr::token::_and_integral: return l & r;
            case expr::token::lt: return l < r;
            case expr::token::le: return l <= r;
            case expr::token::gt: return l > r;
            case expr::token::ge: return l >= r;
            case expr::token::ne: return l != r;
            case expr::token::eq: return l == r;
            case expr::token::add: return l + r;
            case expr::token::sub:
              if (r > l) throw std::runtime_error ("r > l => neg result");
              return l - r;
            case expr::token::mul: return l * r;
            case expr::token::divint:
              if (r == 0) throw expr::exception::eval::divide_by_zero();
              return l / r;
            case expr::token::modint:
              if (r == 0) throw expr::exception::eval::divide_by_zero();
              return l % r;
            case expr::token::_powint:
              {
                T x (1);

                for (T i (0); i < r; ++i) x *= l;

                return x;
              }
            case expr::token::min: return std::min (l, r);
            case expr::token::max: return std::max (l, r);
            default: throw exception::eval (_token, l, r);
            }
          }

          template<typename T>
            value_type fractional (const T l, const T r) const
          {
            switch (_token)
            {
            case expr::token::lt: return l < r;
            case expr::token::le: return l <= r;
            case expr::token::gt: return l > r;
            case expr::token::ge: return l >= r;
            case expr::token::add: return l + r;
            case expr::token::sub: return l - r;
            case expr::token::mul: return l * r;
            case expr::token::div:
              if (std::abs (r) < std::numeric_limits<T>::min())
              {
                throw expr::exception::eval::divide_by_zero();
              }
              return l / r;
            case expr::token::_pow: return std::pow (l, r);
            case expr::token::min: return std::min (l,r);
            case expr::token::max: return std::max (l,r);
            default: throw exception::eval (_token, l, r);
            }
          }
        };
      }

      value_type unary (const expr::token::type& t, const value_type& x)
      {
        return boost::apply_visitor (visitor_unary (t), x);
      }
      value_type binary ( const expr::token::type& t
                        , const value_type& l
                        , const value_type& r
                        )
      {
        return boost::apply_visitor (visitor_binary (t), l, r);
      }
    }
  }
}
