/******************************************************************************

   Copyright 2026 Mayur Kubavat

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

******************************************************************************/

/*!
 * \file fc4sc_crave.hpp
 * \brief Optional CRAVE and crave2uvm closed-loop CDV bridge for FC4SC.
 *
 * Translates FC4SC uncovered intervals, bin deficits, and uncovered cross tuples
 * into CRAVE constraints and weighted distributions compatible with
 * crave::crv_sequence_item::randomize_with(), uvm_randomized_sequence_item,
 * uvm_randomized_sequence, and UVM_DO_WITH().
 *
 * This header is intentionally NOT included by includes/fc4sc.hpp so
 * standalone C++11 builds remain dependency-free.
 */

#ifndef FC4SC_CRAVE_HPP
#define FC4SC_CRAVE_HPP

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "fc4sc.hpp"

#include <crave/ConstrainedRandom.hpp>
#include <crave/experimental/Experimental.hpp>
#if defined(SYSTEMC_INCLUDED) || defined(SC_SYSTEMC_H) || \
    (defined(__has_include) && __has_include(<sysc/datatypes/int/sc_int.h>))
#include <crave/experimental/SystemC.hpp>
#endif

namespace fc4sc {

template <typename T, typename Enable = void>
struct is_crave_bridgeable : std::false_type {};

template <typename T>
struct is_crave_bridgeable<
    T, typename std::enable_if<std::is_integral<T>::value || std::is_enum<T>::value>::type>
    : std::true_type {};

template <int W>
struct is_crave_bridgeable<sc_dt::sc_int<W>>
    : std::integral_constant<bool, (W >= 1 && W <= 64)> {};

template <int W>
struct is_crave_bridgeable<sc_dt::sc_uint<W>>
    : std::integral_constant<bool, (W >= 1 && W <= 64)> {};

template <int W>
struct is_crave_bridgeable<sc_dt::sc_bigint<W>>
    : std::integral_constant<bool, (W >= 1 && W <= 64)> {};

template <int W>
struct is_crave_bridgeable<sc_dt::sc_biguint<W>>
    : std::integral_constant<bool, (W >= 1 && W <= 64)> {};

template <int W>
struct is_crave_bridgeable<sc_dt::sc_bv<W>>
    : std::integral_constant<bool, (W >= 1)> {};

namespace detail {

template <typename T>
struct is_wide_sc_bv : std::false_type {};

template <int W>
struct is_wide_sc_bv<sc_dt::sc_bv<W>> : std::integral_constant<bool, (W > 64)> {};

template <typename T>
struct is_sc_bv_type : std::false_type {};

template <int W>
struct is_sc_bv_type<sc_dt::sc_bv<W>> : std::true_type {};

template <typename Lhs,
          typename std::enable_if<std::is_base_of<crave::crv_var, typename std::decay<Lhs>::type>::value, int>::type = 0>
inline auto as_crave_term(Lhs& lhs) -> decltype(lhs()) {
  return lhs();
}

template <typename Lhs,
          typename std::enable_if<!std::is_base_of<crave::crv_var, typename std::decay<Lhs>::type>::value, int>::type = 0>
inline Lhs&& as_crave_term(Lhs&& lhs) {
  return std::forward<Lhs>(lhs);
}

template <typename Term>
struct crave_term_scalar {
  typedef void type;
  static constexpr bool has_bounds = false;
};

template <typename T>
struct crave_term_scalar<crave::crv_variable<T>> {
  typedef T type;
  static constexpr bool has_bounds = !is_wide_sc_bv<T>::value;
};

template <typename T>
struct crave_term_scalar<crave::WriteReference<T>> {
  typedef T type;
  static constexpr bool has_bounds = !is_wide_sc_bv<T>::value;
};

template <typename T>
struct crave_term_scalar<crave::ReadReference<T>> {
  typedef T type;
  static constexpr bool has_bounds = !is_wide_sc_bv<T>::value;
};

template <typename T>
struct crave_term_scalar<crave::Variable<T>> {
  typedef T type;
  static constexpr bool has_bounds = !is_wide_sc_bv<T>::value;
};

template <typename T>
struct crave_term_scalar<crave::randv<T>> {
  typedef T type;
  static constexpr bool has_bounds = !is_wide_sc_bv<T>::value;
};

template <typename T, typename Enable = void>
struct scalar_i128_conv {
  static __int128 to_i128(const T& v) { return static_cast<__int128>(v); }
  static __int128 min_i128() { return static_cast<__int128>(std::numeric_limits<T>::min()); }
  static __int128 max_i128() { return static_cast<__int128>(std::numeric_limits<T>::max()); }
};

template <>
struct scalar_i128_conv<bool> {
  static __int128 to_i128(bool v) { return v ? 1 : 0; }
  static __int128 min_i128() { return 0; }
  static __int128 max_i128() { return 1; }
};

template <typename E>
struct scalar_i128_conv<E, typename std::enable_if<std::is_enum<E>::value>::type> {
  typedef typename std::underlying_type<E>::type under_t;
  static __int128 to_i128(const E& v) { return static_cast<__int128>(static_cast<under_t>(v)); }
  static __int128 min_i128() { return static_cast<__int128>(std::numeric_limits<under_t>::min()); }
  static __int128 max_i128() { return static_cast<__int128>(std::numeric_limits<under_t>::max()); }
};

template <int W>
struct scalar_i128_conv<sc_dt::sc_int<W>> {
  static __int128 to_i128(const sc_dt::sc_int<W>& v) { return static_cast<__int128>(v.to_int64()); }
  static __int128 min_i128() { return static_cast<__int128>(fc4sc_type_traits<sc_dt::sc_int<W>>::min_i64()); }
  static __int128 max_i128() { return static_cast<__int128>(fc4sc_type_traits<sc_dt::sc_int<W>>::max_i64()); }
};

template <int W>
struct scalar_i128_conv<sc_dt::sc_uint<W>> {
  static __int128 to_i128(const sc_dt::sc_uint<W>& v) { return static_cast<__int128>(v.to_uint64()); }
  static __int128 min_i128() { return 0; }
  static __int128 max_i128() { return static_cast<__int128>(fc4sc_type_traits<sc_dt::sc_uint<W>>::max_u64()); }
};

template <int W>
struct scalar_i128_conv<sc_dt::sc_bigint<W>> {
  static_assert(W <= 64, "sc_bigint > 64 bits is not supported by CRAVE");
  static __int128 to_i128(const sc_dt::sc_bigint<W>& v) { return static_cast<__int128>(v.to_int64()); }
  static __int128 min_i128() {
    return (W >= 64) ? static_cast<__int128>(std::numeric_limits<int64_t>::min())
                     : -static_cast<__int128>(uint64_t(1) << (W - 1));
  }
  static __int128 max_i128() {
    return (W >= 64) ? static_cast<__int128>(std::numeric_limits<int64_t>::max())
                     : static_cast<__int128>((uint64_t(1) << (W - 1)) - 1);
  }
};

template <int W>
struct scalar_i128_conv<sc_dt::sc_biguint<W>> {
  static_assert(W <= 64, "sc_biguint > 64 bits is not supported by CRAVE");
  static __int128 to_i128(const sc_dt::sc_biguint<W>& v) { return static_cast<__int128>(v.to_uint64()); }
  static __int128 min_i128() { return 0; }
  static __int128 max_i128() {
    return (W >= 64) ? static_cast<__int128>(std::numeric_limits<uint64_t>::max())
                     : static_cast<__int128>((uint64_t(1) << W) - 1);
  }
};

template <int W>
struct scalar_i128_conv<sc_dt::sc_bv<W>> {
  static __int128 to_i128(const sc_dt::sc_bv<W>& v) {
    static_assert(W <= 64, "scalar_i128_conv requires sc_bv width <= 64");
    return static_cast<__int128>(v.to_uint64());
  }
  static __int128 min_i128() { return 0; }
  static __int128 max_i128() {
    static_assert(W <= 64, "scalar_i128_conv requires sc_bv width <= 64");
    return (W >= 64) ? static_cast<__int128>(std::numeric_limits<uint64_t>::max())
                     : static_cast<__int128>((uint64_t(1) << W) - 1);
  }
};

template <typename CvpT, typename Enable = void>
struct crave_range_elem_type {
  typedef CvpT type;
};

template <typename E>
struct crave_range_elem_type<E, typename std::enable_if<std::is_enum<E>::value>::type> {
  typedef typename std::underlying_type<E>::type type;
};

template <int W>
struct crave_range_elem_type<sc_dt::sc_int<W>> {
  typedef int64_t type;
};

template <int W>
struct crave_range_elem_type<sc_dt::sc_bigint<W>> {
  typedef int64_t type;
};

template <int W>
struct crave_range_elem_type<sc_dt::sc_uint<W>> {
  typedef uint64_t type;
};

template <int W>
struct crave_range_elem_type<sc_dt::sc_biguint<W>> {
  typedef uint64_t type;
};

template <int W>
struct crave_range_elem_type<sc_dt::sc_bv<W>> {
  typedef uint64_t type;
};

template <typename LhsScalar, bool HasBounds = (!std::is_void<LhsScalar>::value && !is_wide_sc_bv<LhsScalar>::value)>
struct lhs_bounds_helper {
  typedef void target_hint_t;
  static bool clamp(__int128&, __int128&) { return true; }
};

template <typename LhsScalar>
struct lhs_bounds_helper<LhsScalar, true> {
  typedef typename crave_range_elem_type<LhsScalar>::type target_hint_t;
  static bool clamp(__int128& lo, __int128& hi) {
    const __int128 lmin = scalar_i128_conv<LhsScalar>::min_i128();
    const __int128 lmax = scalar_i128_conv<LhsScalar>::max_i128();
    if (hi < lmin || lo > lmax) return false;
    if (lo < lmin) lo = lmin;
    if (hi > lmax) hi = lmax;
    return lo <= hi;
  }
};

template <typename A, typename B>
struct choose_range_elem {
  typedef typename std::conditional<std::is_void<A>::value, B, A>::type type;
};

template <int W>
inline uint64_t extract_sc_bv_limb(const sc_dt::sc_bv<W>& v, int lo_bit, int hi_bit) {
  const int w_idx = lo_bit / 32;
  const uint64_t lo_w = static_cast<uint64_t>(v.get_word(w_idx));
  const unsigned sw = static_cast<unsigned>(hi_bit - lo_bit + 1);
  const uint64_t hi_w = (sw > 32 && (w_idx + 1) < v.size())
                            ? static_cast<uint64_t>(v.get_word(w_idx + 1))
                            : uint64_t(0);
  uint64_t val = lo_w | (hi_w << 32);
  if (sw < 64) {
    val &= (uint64_t(1) << sw) - 1ULL;
  }
  return val;
}

template <int W>
inline crave::NodePtr make_sc_bv_equal_node(const crave::NodePtr& lhs_node,
                                            const sc_dt::sc_bv<W>& val) {
  const int num_limbs = (W + 63) / 64;
  crave::NodePtr acc;
  for (int k = 0; k < num_limbs; ++k) {
    const int lo_bit = k * 64;
    const int hi_bit = std::min(W - 1, lo_bit + 63);
    const unsigned sw = static_cast<unsigned>(hi_bit - lo_bit + 1);
    const uint64_t limb = extract_sc_bv_limb(val, lo_bit, hi_bit);
    crave::NodePtr slice(new crave::Bitslice(lhs_node, hi_bit, lo_bit));
    crave::NodePtr eq(new crave::EqualOpr(slice, new crave::Constant(limb, sw, false)));
    acc = acc ? crave::NodePtr(new crave::LogicalAndOpr(acc, eq)) : eq;
  }
  return acc;
}

template <int W>
inline crave::NodePtr make_sc_bv_interval_node(const crave::NodePtr& lhs_node,
                                               const sc_dt::sc_bv<W>& lo,
                                               const sc_dt::sc_bv<W>& hi) {
  if (lo == hi) {
    return make_sc_bv_equal_node(lhs_node, lo);
  }
  const int num_limbs = (W + 63) / 64;
  crave::NodePtr ge_node;
  crave::NodePtr le_node;
  for (int k = 0; k < num_limbs; ++k) {
    const int lo_bit = k * 64;
    const int hi_bit = std::min(W - 1, lo_bit + 63);
    const unsigned sw = static_cast<unsigned>(hi_bit - lo_bit + 1);
    const uint64_t lo_k = extract_sc_bv_limb(lo, lo_bit, hi_bit);
    const uint64_t hi_k = extract_sc_bv_limb(hi, lo_bit, hi_bit);
    crave::NodePtr slice(new crave::Bitslice(lhs_node, hi_bit, lo_bit));
    crave::NodePtr clo(new crave::Constant(lo_k, sw, false));
    crave::NodePtr chi(new crave::Constant(hi_k, sw, false));

    if (k == 0) {
      ge_node = crave::NodePtr(new crave::GreaterEqualOpr(slice, clo));
      le_node = crave::NodePtr(new crave::LessEqualOpr(slice, chi));
    } else {
      crave::NodePtr gt_k(new crave::GreaterOpr(slice, clo));
      crave::NodePtr eq_lo_k(new crave::EqualOpr(slice, clo));
      ge_node = crave::NodePtr(
          new crave::LogicalOrOpr(gt_k, new crave::LogicalAndOpr(eq_lo_k, ge_node)));

      crave::NodePtr lt_k(new crave::LessOpr(slice, chi));
      crave::NodePtr eq_hi_k(new crave::EqualOpr(slice, chi));
      le_node = crave::NodePtr(
          new crave::LogicalOrOpr(lt_k, new crave::LogicalAndOpr(eq_hi_k, le_node)));
    }
  }
  return crave::NodePtr(new crave::LogicalAndOpr(ge_node, le_node));
}

template <typename Lhs, int W>
inline crave::expression build_hole_constraint_impl(
    Lhs&& lhs,
    const std::vector<interval_t<sc_dt::sc_bv<W>>>& holes,
    std::true_type /*wide_bv*/) {
  if (holes.empty()) {
    return crave::value_to_expression(true);
  }
  crave::NodePtr lhs_node = boost::proto::value(crave::make_expression(as_crave_term(std::forward<Lhs>(lhs))));
  crave::NodePtr acc;
  for (const auto& iv : holes) {
    crave::NodePtr iv_node = make_sc_bv_interval_node(lhs_node, iv.first, iv.second);
    acc = acc ? crave::NodePtr(new crave::LogicalOrOpr(acc, iv_node)) : iv_node;
  }
  return boost::proto::make_expr<boost::proto::tag::terminal, crave::Constraint_Domain>(acc);
}

template <typename Lhs, typename CvpT>
inline crave::expression build_hole_constraint_impl(
    Lhs&& lhs,
    const std::vector<interval_t<CvpT>>& holes,
    std::false_type /*wide_bv*/) {
  if (holes.empty()) {
    return crave::value_to_expression(true);
  }
  typedef typename std::decay<Lhs>::type decayed_lhs_t;
  typedef typename crave_term_scalar<decayed_lhs_t>::type lhs_scalar_t;
  typedef lhs_bounds_helper<lhs_scalar_t> bounds_t;
  typedef typename choose_range_elem<typename bounds_t::target_hint_t,
                                     typename crave_range_elem_type<CvpT>::type>::type range_elem_t;

  crave::NodePtr lhs_node = boost::proto::value(
      crave::make_expression(as_crave_term(std::forward<Lhs>(lhs))));
  crave::NodePtr acc;
  for (const auto& iv : holes) {
    __int128 lo = scalar_i128_conv<CvpT>::to_i128(iv.first);
    __int128 hi = scalar_i128_conv<CvpT>::to_i128(iv.second);
    if (!bounds_t::clamp(lo, hi))
      continue;
    crave::NodePtr iv_node;
    if (lo == hi) {
      iv_node = crave::NodePtr(new crave::EqualOpr(
          lhs_node, crave::Context::new_value(static_cast<range_elem_t>(lo))));
    } else {
      crave::NodePtr ge(new crave::GreaterEqualOpr(
          lhs_node, crave::Context::new_value(static_cast<range_elem_t>(lo))));
      crave::NodePtr le(new crave::LessEqualOpr(
          lhs_node, crave::Context::new_value(static_cast<range_elem_t>(hi))));
      iv_node = crave::NodePtr(new crave::LogicalAndOpr(ge, le));
    }
    acc = acc ? crave::NodePtr(new crave::LogicalOrOpr(acc, iv_node)) : iv_node;
  }
  if (!acc) {
    return crave::value_to_expression(false);
  }
  return boost::proto::make_expr<boost::proto::tag::terminal, crave::Constraint_Domain>(acc);
}

template <typename Lhs, typename ValT>
inline crave::NodePtr build_scalar_eq_node(Lhs&& lhs, const ValT& val, std::true_type /*wide_bv*/) {
  crave::NodePtr lhs_node = boost::proto::value(crave::make_expression(as_crave_term(std::forward<Lhs>(lhs))));
  return make_sc_bv_equal_node(lhs_node, val);
}

template <typename Lhs, typename ValT>
inline crave::NodePtr build_scalar_eq_node(Lhs&& lhs, const ValT& val, std::false_type /*wide_bv*/) {
  typedef typename std::decay<Lhs>::type decayed_lhs_t;
  typedef typename crave_term_scalar<decayed_lhs_t>::type lhs_scalar_t;
  typedef lhs_bounds_helper<lhs_scalar_t> bounds_t;
  typedef typename choose_range_elem<typename bounds_t::target_hint_t,
                                     typename crave_range_elem_type<ValT>::type>::type cast_t;

  __int128 v128 = scalar_i128_conv<ValT>::to_i128(val);
  __int128 lo = v128;
  __int128 hi = v128;
  if (!bounds_t::clamp(lo, hi)) {
    return crave::NodePtr(new crave::Constant(false));
  }
  return boost::proto::value(
      crave::make_expression(as_crave_term(std::forward<Lhs>(lhs)) == static_cast<cast_t>(v128)));
}

template <size_t I, typename Tuple, typename LhsTuple>
inline typename std::enable_if<I == std::tuple_size<Tuple>::value, crave::NodePtr>::type
build_tuple_match_node(const Tuple&, LhsTuple&&) {
  return crave::NodePtr();
}

template <size_t I, typename Tuple, typename LhsTuple>
inline typename std::enable_if<(I < std::tuple_size<Tuple>::value), crave::NodePtr>::type
build_tuple_match_node(const Tuple& tup, LhsTuple&& lhs_tup) {
  typedef typename std::tuple_element<I, Tuple>::type elem_t;
  static_assert(is_crave_bridgeable<elem_t>::value,
                "fc4sc::cross_hole_constraint: cross element type is not bridgeable to CRAVE");
  crave::NodePtr cur = build_scalar_eq_node(
      std::get<I>(lhs_tup),
      std::get<I>(tup),
      std::integral_constant<bool, is_wide_sc_bv<elem_t>::value>());
  crave::NodePtr rest = build_tuple_match_node<I + 1>(tup, std::forward<LhsTuple>(lhs_tup));
  if (!rest) return cur;
  return crave::NodePtr(new crave::LogicalAndOpr(cur, rest));
}

template <typename... Args, typename LhsTuple, size_t... I>
inline crave::expression cross_hole_constraint_impl(
    const cross<Args...>& crs,
    LhsTuple&& lhs_tup,
    uint64_t max_tuples,
    index_seq<I...>) {
  auto holes = crs.get_uncovered_cross_tuples(max_tuples);
  if (holes.empty()) {
    return crave::value_to_expression(true);
  }
  crave::NodePtr acc;
  for (const auto& tup : holes) {
    crave::NodePtr tnode = build_tuple_match_node<0>(tup, std::forward<LhsTuple>(lhs_tup));
    acc = acc ? crave::NodePtr(new crave::LogicalOrOpr(acc, tnode)) : tnode;
  }
  return boost::proto::make_expr<boost::proto::tag::terminal, crave::Constraint_Domain>(acc);
}

template <typename... Args, typename RestTuple>
inline crave::expression dispatch_cross_hole(
    const cross<Args...>& crs,
    RestTuple&& rest,
    std::true_type /*no explicit max_tuples*/) {
  return cross_hole_constraint_impl(
      crs, std::forward<RestTuple>(rest), uint64_t(256), make_index_seq<sizeof...(Args)>());
}

template <typename... Args, typename RestTuple>
inline crave::expression dispatch_cross_hole(
    const cross<Args...>& crs,
    RestTuple&& rest,
    std::false_type /*last arg is max_tuples*/) {
  const uint64_t max_tuples = static_cast<uint64_t>(std::get<sizeof...(Args)>(rest));
  return cross_hole_constraint_impl(
      crs, std::forward<RestTuple>(rest), max_tuples, make_index_seq<sizeof...(Args)>());
}

template <typename CvpT, typename DistValT, typename BoundsT>
inline crave::distribution_tag<bool> build_hole_dist(
    const coverpoint<CvpT>& cvp,
    std::true_type /*is_bool*/) {
  double w_false = 0.0;
  double w_true = 0.0;
  const auto infos = cvp.get_uncovered_bin_infos();
  for (const auto& info : infos) {
    if (info.is_transition) continue;
    const uint64_t deficit = (info.at_least > info.hits) ? (info.at_least - info.hits) : 0ULL;
    if (deficit == 0ULL) continue;
    const auto ivs = cvp.get_uncovered_intervals(info.index);
    for (const auto& iv : ivs) {
      __int128 lo = scalar_i128_conv<CvpT>::to_i128(iv.first);
      __int128 hi = scalar_i128_conv<CvpT>::to_i128(iv.second);
      if (!BoundsT::clamp(lo, hi)) continue;
      if (lo <= 0 && hi >= 0) w_false += static_cast<double>(deficit);
      if (lo <= 1 && hi >= 1) w_true += static_cast<double>(deficit);
    }
  }
  const double total = w_false + w_true;
  const double prob = (total > 0.0) ? (w_true / total) : 0.5;
  return crave::distribution<bool>::create(prob);
}

template <typename CvpT, typename DistValT, typename BoundsT>
inline crave::distribution_tag<DistValT> build_hole_dist(
    const coverpoint<CvpT>& cvp,
    std::false_type /*is_bool*/) {
  crave::distribution<DistValT> dist_obj;
  const auto infos = cvp.get_uncovered_bin_infos();
  for (const auto& info : infos) {
    if (info.is_transition) continue;
    const uint64_t deficit = (info.at_least > info.hits) ? (info.at_least - info.hits) : 0ULL;
    if (deficit == 0ULL) continue;
    const auto ivs = cvp.get_uncovered_intervals(info.index);
    for (const auto& iv : ivs) {
      __int128 lo = scalar_i128_conv<CvpT>::to_i128(iv.first);
      __int128 hi = scalar_i128_conv<CvpT>::to_i128(iv.second);
      if (!BoundsT::clamp(lo, hi)) continue;
      dist_obj(crave::weighted_range<DistValT>(
          static_cast<DistValT>(lo),
          static_cast<DistValT>(hi),
          deficit));
    }
  }
  return crave::distribution_tag<DistValT>(dist_obj);
}

} // namespace detail

/*!
 * \brief Builds a hard CRAVE constraint restricting \p lhs to uncovered intervals of \p cvp.
 *        When \p cvp is 100% covered (no holes), returns a trivially satisfied constraint.
 */
template <typename Lhs, typename CvpT>
inline crave::expression hole_constraint(Lhs&& lhs, const coverpoint<CvpT>& cvp) {
  static_assert(is_crave_bridgeable<CvpT>::value,
                "fc4sc::hole_constraint: coverpoint type is not bridgeable to CRAVE");
  return detail::build_hole_constraint_impl(
      std::forward<Lhs>(lhs),
      cvp.get_uncovered_intervals(),
      std::integral_constant<bool, detail::is_wide_sc_bv<CvpT>::value>());
}

/*!
 * \brief Soft variant of hole_constraint() (IEEE 1800-2023 18.5.14).
 */
template <typename Lhs, typename CvpT>
inline auto soft_hole_constraint(Lhs&& lhs, const coverpoint<CvpT>& cvp)
    -> decltype(crave::soft(hole_constraint(std::forward<Lhs>(lhs), cvp))) {
  return crave::soft(hole_constraint(std::forward<Lhs>(lhs), cvp));
}

/*!
 * \brief Builds a weighted CRAVE dist() expression over uncovered bins of \p cvp,
 *        weighting each uncovered bin by its remaining deficit (at_least - hits).
 *        When 100% covered, returns an unconstrained uniform distribution over \p lhs.
 */
template <typename Lhs, typename CvpT>
inline auto hole_distribution(Lhs&& lhs, const coverpoint<CvpT>& cvp)
    -> decltype(crave::dist(
        detail::as_crave_term(std::forward<Lhs>(lhs)),
        crave::distribution_tag<typename crave::distribution_value_type<
            typename std::conditional<
                !std::is_void<typename detail::crave_term_scalar<typename std::decay<Lhs>::type>::type>::value,
                typename detail::crave_term_scalar<typename std::decay<Lhs>::type>::type,
                typename detail::crave_range_elem_type<CvpT>::type>::type>::type>(
            crave::distribution<typename crave::distribution_value_type<
                typename std::conditional<
                    !std::is_void<typename detail::crave_term_scalar<typename std::decay<Lhs>::type>::type>::value,
                    typename detail::crave_term_scalar<typename std::decay<Lhs>::type>::type,
                    typename detail::crave_range_elem_type<CvpT>::type>::type>::type>()))) {
  static_assert(is_crave_bridgeable<CvpT>::value && !detail::is_wide_sc_bv<CvpT>::value,
                "fc4sc::hole_distribution requires a <= 64-bit discrete bridgeable coverpoint type");

  typedef typename std::decay<Lhs>::type decayed_lhs_t;
  typedef typename detail::crave_term_scalar<decayed_lhs_t>::type lhs_scalar_t;
  typedef typename std::conditional<
      !std::is_void<lhs_scalar_t>::value,
      lhs_scalar_t,
      typename detail::crave_range_elem_type<CvpT>::type>::type domain_t;
  typedef typename crave::distribution_value_type<domain_t>::type dist_val_t;
  typedef detail::lhs_bounds_helper<lhs_scalar_t> bounds_t;

  return crave::dist(
      detail::as_crave_term(std::forward<Lhs>(lhs)),
      detail::build_hole_dist<CvpT, dist_val_t, bounds_t>(
          cvp, std::integral_constant<bool, std::is_same<dist_val_t, bool>::value>()));
}

/*!
 * \brief Builds a hard CRAVE constraint targeting uncovered cross tuples of \p crs.
 *        Accepts (crs, lhs_0, ..., lhs_{k-1}) or (crs, lhs_0, ..., lhs_{k-1}, max_tuples).
 *        Returns Constant(true) when 100% covered.
 */
template <typename... Args, typename... Rest>
inline crave::expression cross_hole_constraint(const cross<Args...>& crs, Rest&&... rest) {
  static_assert(sizeof...(Rest) == sizeof...(Args) || sizeof...(Rest) == sizeof...(Args) + 1,
                "fc4sc::cross_hole_constraint expects N CRAVE variables (plus optional max_tuples)");
  return detail::dispatch_cross_hole(
      crs,
      std::forward_as_tuple(std::forward<Rest>(rest)...),
      std::integral_constant<bool, (sizeof...(Rest) == sizeof...(Args))>());
}

/*!
 * \brief Soft variant of cross_hole_constraint().
 */
template <typename... Args, typename... Rest>
inline auto soft_cross_hole_constraint(const cross<Args...>& crs, Rest&&... rest)
    -> decltype(crave::soft(cross_hole_constraint(crs, std::forward<Rest>(rest)...))) {
  return crave::soft(cross_hole_constraint(crs, std::forward<Rest>(rest)...));
}

/*!
 * \brief Attempts item.randomize_with(hole_cstr, extra_cstrs...); if conflicting,
 *        falls back to item.randomize_with(extra_cstrs...) (or item.randomize()).
 */
template <typename Item, typename HoleExpr>
inline bool randomize_with_holes(Item& item, HoleExpr&& hole_cstr) {
  if (item.randomize_with(std::forward<HoleExpr>(hole_cstr)))
    return true;
  return item.randomize();
}

template <typename Item, typename HoleExpr, typename Extra0, typename... ExtraRest>
inline bool randomize_with_holes(Item& item,
                                 HoleExpr&& hole_cstr,
                                 Extra0&& extra0,
                                 ExtraRest&&... extra_rest) {
  if (item.randomize_with(std::forward<HoleExpr>(hole_cstr),
                          extra0,
                          extra_rest...)) {
    return true;
  }
  return item.randomize_with(std::forward<Extra0>(extra0),
                             std::forward<ExtraRest>(extra_rest)...);
}

} // namespace fc4sc

#endif /* FC4SC_CRAVE_HPP */
