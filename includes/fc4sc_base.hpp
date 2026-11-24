/******************************************************************************

   Copyright 2003-2018 AMIQ Consulting s.r.l.
   Copyright 2020 NVIDIA Corporation

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
/******************************************************************************
   Original Authors: Teodor Vasilache and Dragos Dospinescu,
                     AMIQ Consulting s.r.l. (contributors@amiq.com)

               Date: 2018-Feb-20
******************************************************************************/

/*!
 \file fc_base.hpp
 \brief Helper functions and interfaces

 This file contains helper functions and the base classes
 for bins, coverpoints and covergroups.
 */

#ifndef FC4SC_BASE_HPP
#define FC4SC_BASE_HPP

#include <cstdint>
#include <map>
#include <iostream>
#include <string>
#include <exception>
#include <algorithm>
#include <tuple>
#include <vector>
#include <utility>
#include <functional>
#include <unordered_map>
#include <assert.h>
#include <memory>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <typeinfo>

#include "fc4sc_options.hpp"
#include "fc4sc_type_traits.hpp"

/*
 * Template meta-programming tool used for checking that a parameter
 * pack doesn't contain any argument which is convertible to a specified type.
 * This is used in the bin & coverpoint constructors together with a
 * static_assert in order to make sure that the user does not provide
 * multiple arguments which must be provided only once.
 * For bin -> the name
 * For coverpoint -> pointer to the parent covergroup
 */
template<typename forbidden, typename head, typename...tail> struct forbid_type {
  static bool constexpr value = forbid_type<forbidden, tail...>::value &&
    (!std::is_convertible<head, forbidden>::value);
};
template<typename forbidden, typename first> struct forbid_type<forbidden, first> :
  public std::integral_constant<bool, !std::is_convertible<first, forbidden>::value> {};

/*!
 * \brief Macro to declare constructor for covergroup registration.
 *
 * This macro expands to a constructor declaration for your custom type, which calls a parent constructor
 * to register the new instance, with some additional info useful for displaying the final report.
 * The default arguments of the constructor are part of this declaration. Default values
 * for additional constructor arguments (args) have to be given here as well, and not in
 * the corresponding CG_CONS_DEF.
 */
#define CG_CONS_DECL(type, args...) \
  using covergroup::sample; \
  type(std::string inst_name = "", const char *auto_inst_file_name = "", int auto_line = 1, fc4sc::global* cntxt = fc4sc::global::getter(), ##args)

/*!
 * \brief Macro to define constructor for covergroup registration.
 *
 * This macro expands to a constructor definition for your custom type, which calls a parent constructor
 * to register the new instance, with some additional info useful for displaying the final report.
 * The default arguments of the constructor belong to the declaration (CG_CONS_DECL),
 * so additional constructor arguments (args) must be given without default values here.
 */
#define CG_CONS_DEF(type, args...) \
  type::type(std::string inst_name, const char *auto_inst_file_name, int auto_line, fc4sc::global* cntxt, ##args) : fc4sc::covergroup(#type, __FILE__, __LINE__, inst_name, auto_inst_file_name, auto_line, cntxt)

/*!
 * \brief Macro to register your covergroup.
 *
 * This macro expands to a constructor for your custom type, which calls a parent constructor
 * to register the new instance, with some additional info useful for displaying the final report.
 */
#define CG_CONS(type, args...) \
  using covergroup::sample; \
  type(std::string inst_name = "", const char *auto_inst_file_name = "", int auto_line = 1, fc4sc::global* cntxt = fc4sc::global::getter(), ##args) : fc4sc::covergroup(#type, __FILE__, __LINE__, inst_name, auto_inst_file_name, auto_line, cntxt)

/*!
 * \brief Macro to declare constructor for covergroup registration within a scope.
 *
 * This macro expands to a constructor declaration for your custom type, which calls a parent constructor
 * to register the new instance, with some additional info useful for displaying the final report.
 * Requires that the covergroup must be associated with a scope when instantiated
 * The default arguments of the constructor are part of this declaration. Default values
 * for additional constructor arguments (args) have to be given here as well, and not in
 * the corresponding CG_SCOPED_CONS_DEF.
 */
#define CG_SCOPED_CONS_DECL(type, scp_type, args...) \
  using covergroup::sample; \
  type(fc4sc::scp_base& scp, std::string inst_name = "", const char *auto_inst_file_name = "", int auto_line = 1, ##args)

/*!
 * \brief Macro to define constructor for covergroup registration within a scope.
 *
 * This macro expands to a constructor definition for your custom type, which calls a parent constructor
 * to register the new instance, with some additional info useful for displaying the final report.
 * Requires that the covergroup must be associated with a scope when instantiated
 * The default arguments of the constructor belong to the declaration (CG_SCOPED_CONS_DECL),
 * so additional constructor arguments (args) must be given without default values here.
 */
#define CG_SCOPED_CONS_DEF(type, scp_type, args...) \
  scp_type::type::type(fc4sc::scp_base& scp, std::string inst_name, const char *auto_inst_file_name, int auto_line, ##args) : fc4sc::covergroup(scp, #type, scp_type::scp_type_name(), __FILE__, __LINE__, inst_name, auto_inst_file_name, auto_line)


/*!
 * \brief Macro to register your covergroup within a scope.
 *
 * This macro expands to a constructor for your custom type, which calls a parent constructor
 * to register the new instance, with some additional info useful for displaying the final report.
 * Requires that the covergroup must be associated with a scope when instantiated.
 */
#define CG_SCOPED_CONS(type, scp_type, args...) \
  using covergroup::sample; \
  type(fc4sc::scp_base& scp, std::string inst_name = "", const char *auto_inst_file_name = "", int auto_line = 1, ##args) : fc4sc::covergroup(scp, #type, scp_type::scp_type_name(), __FILE__, __LINE__, inst_name, auto_inst_file_name, auto_line)

/*!
 * \brief Macro to register your covergroup within a scope.
 *
 * This macro expands to a constructor for your custom type, which calls a parent constructor
 * to register the new instance, with some additional info useful for displaying the final report.
 * Requires that the covergroup must be associated with a scope when instantiated.
 */
#define CG_SCOPED_INST(name, args...) \
  name(*this,#name,__FILE__,__LINE__,##args)

/*!
 * \brief Macro to declare a sampling point variable and register some names
 */
#define SAMPLE_POINT(variable_name, cvp) variable_name = static_cast<decltype(variable_name)> \
  (covergroup::set_strings(&cvp, &variable_name, #cvp, #variable_name));

/*
 * \brief Macro that creates a lambda function which returns the expression given
 * as argument when called. A pointer to the enclosing class will be bound in the
 * lambda capture so that local class variables can be used in the lambda body.
 * The purpose of this macro is to use in the instantiation of coverpoints inside
 * covergroups and will serve as a generator for lambda expressions to be used
 * when sampling (sample expression and sample conditions).
 */
#define CREATE_WRAP_F(expr, type) [&]() -> type { return (expr); }

#define GET_CVP_MACRO(_1,_2,_3,_4, NAME,...) NAME

/*
 * Macro used to define and automatically register a coverpoint inside a covergroup.
 * The macro accepts 3 or 4 arguments in the following order:
 * 1) Type: used for the values of the bins declared in the coverpoint
 * 2) Name: the name of the coverpoint
 * 3) Sample Expression: the expression that will be used for sampling this coverpoint
 * whenever the covergroup is sampled (the type has to be the same as Type argument)
 * 4) Sample Condition: a condition that has to be true in order for this coverpoint
 * to be sampled when the covergroup is sampled (the type has to be bool)
 */
#define COVERPOINT(...) GET_CVP_MACRO(__VA_ARGS__, CVP_4, CVP_3)(__VA_ARGS__)

// COVERPOINT macro for 3 arguments (no sample condition)
#define CVP_3(type, cvp_name, sample_expr) \
        coverpoint<fc4sc::detail::decay_t<type>> cvp_name = \
        covergroup::register_cvp<fc4sc::detail::decay_t<type>>(&cvp_name, #cvp_name, \
        CREATE_WRAP_F(sample_expr, fc4sc::detail::decay_t<type>), #sample_expr, \
        CREATE_WRAP_F(true, bool), std::string("")) =

// COVERPOINT macro for 4 arguments (sample condition included)
#define CVP_4(type, cvp_name, sample_expr, sample_cond) \
        coverpoint<fc4sc::detail::decay_t<type>> cvp_name = \
        covergroup::register_cvp<fc4sc::detail::decay_t<type>>(&cvp_name, #cvp_name, \
        CREATE_WRAP_F(sample_expr, fc4sc::detail::decay_t<type>), #sample_expr, \
        CREATE_WRAP_F(sample_cond, bool), #sample_cond) =

#define FC4SC_CAT_IMPL(a, b) a##b
#define FC4SC_CAT(a, b) FC4SC_CAT_IMPL(a, b)
#define FC4SC_EXPAND(x) x
#define FC4SC_UNPAREN(...) __VA_ARGS__

#define CG_EXTEND_DECL(type, base_type, args...) \
  using base_type::sample; \
  type(std::string inst_name = "", const char *auto_inst_file_name = "", int auto_line = 1, fc4sc::global* cntxt = fc4sc::global::getter(), ##args)

#define CG_EXTEND_DEF(type, base_type, args...) \
  type::type(std::string inst_name, const char *auto_inst_file_name, int auto_line, fc4sc::global* cntxt, ##args) \
    : base_type(fc4sc::detail::forward_derived_type(inst_name, fc4sc::detail::derived_type_guard(#type, __FILE__, __LINE__)), auto_inst_file_name, auto_line, cntxt)

#define CG_EXTEND_BASE_DEF(type, base_type, base_args, args...) \
  type::type(std::string inst_name, const char *auto_inst_file_name, int auto_line, fc4sc::global* cntxt, ##args) \
    : base_type(fc4sc::detail::forward_derived_type(inst_name, fc4sc::detail::derived_type_guard(#type, __FILE__, __LINE__)), auto_inst_file_name, auto_line, cntxt, FC4SC_UNPAREN base_args)

#define CG_EXTEND(type, base_type, args...) \
  using base_type::sample; \
  type(std::string inst_name = "", const char *auto_inst_file_name = "", int auto_line = 1, fc4sc::global* cntxt = fc4sc::global::getter(), ##args) \
    : base_type(fc4sc::detail::forward_derived_type(inst_name, fc4sc::detail::derived_type_guard(#type, __FILE__, __LINE__)), auto_inst_file_name, auto_line, cntxt)

#define CG_EXTEND_BASE(type, base_type, base_args, args...) \
  using base_type::sample; \
  type(std::string inst_name = "", const char *auto_inst_file_name = "", int auto_line = 1, fc4sc::global* cntxt = fc4sc::global::getter(), ##args) \
    : base_type(fc4sc::detail::forward_derived_type(inst_name, fc4sc::detail::derived_type_guard(#type, __FILE__, __LINE__)), auto_inst_file_name, auto_line, cntxt, FC4SC_UNPAREN base_args)

#define CG_SCOPED_EXTEND_DECL(type, base_type, scp_type, args...) \
  using base_type::sample; \
  type(fc4sc::scp_base& scp, std::string inst_name = "", const char *auto_inst_file_name = "", int auto_line = 1, ##args)

#define CG_SCOPED_EXTEND_DEF(type, base_type, scp_type, args...) \
  scp_type::type::type(fc4sc::scp_base& scp, std::string inst_name, const char *auto_inst_file_name, int auto_line, ##args) \
    : base_type(scp, fc4sc::detail::forward_derived_type(inst_name, fc4sc::detail::derived_type_guard(#type, __FILE__, __LINE__, scp_type::scp_type_name().c_str())), auto_inst_file_name, auto_line)

#define CG_SCOPED_EXTEND_BASE_DEF(type, base_type, scp_type, base_args, args...) \
  scp_type::type::type(fc4sc::scp_base& scp, std::string inst_name, const char *auto_inst_file_name, int auto_line, ##args) \
    : base_type(scp, fc4sc::detail::forward_derived_type(inst_name, fc4sc::detail::derived_type_guard(#type, __FILE__, __LINE__, scp_type::scp_type_name().c_str())), auto_inst_file_name, auto_line, FC4SC_UNPAREN base_args)

#define CG_SCOPED_EXTEND(type, base_type, scp_type, args...) \
  using base_type::sample; \
  type(fc4sc::scp_base& scp, std::string inst_name = "", const char *auto_inst_file_name = "", int auto_line = 1, ##args) \
    : base_type(scp, fc4sc::detail::forward_derived_type(inst_name, fc4sc::detail::derived_type_guard(#type, __FILE__, __LINE__, scp_type::scp_type_name().c_str())), auto_inst_file_name, auto_line)

#define CG_SCOPED_EXTEND_BASE(type, base_type, scp_type, base_args, args...) \
  using base_type::sample; \
  type(fc4sc::scp_base& scp, std::string inst_name = "", const char *auto_inst_file_name = "", int auto_line = 1, ##args) \
    : base_type(scp, fc4sc::detail::forward_derived_type(inst_name, fc4sc::detail::derived_type_guard(#type, __FILE__, __LINE__, scp_type::scp_type_name().c_str())), auto_inst_file_name, auto_line, FC4SC_UNPAREN base_args)

#define CG_IFF(expr) \
  bool FC4SC_CAT(fc4sc_iff_init_, __LINE__) = this->add_iff_init([&]() -> bool { return static_cast<bool>(expr); }, #expr)

#define CROSS_IFF(expr) \
  fc4sc::cross_iff([&]() -> bool { return static_cast<bool>(expr); })

#define FC4SC_NARGS_IMPL(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,N,...) N
#define FC4SC_NARGS(...) FC4SC_EXPAND(FC4SC_NARGS_IMPL(__VA_ARGS__,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0))

#define FC4SC_SCG_P1(m1) const fc4sc::detail::decay_t<decltype(m1)>& _fc4sc_arg_1
#define FC4SC_SCG_P2(m1, m2) FC4SC_SCG_P1(m1), const fc4sc::detail::decay_t<decltype(m2)>& _fc4sc_arg_2
#define FC4SC_SCG_P3(m1, m2, m3) FC4SC_SCG_P2(m1, m2), const fc4sc::detail::decay_t<decltype(m3)>& _fc4sc_arg_3
#define FC4SC_SCG_P4(m1, m2, m3, m4) FC4SC_SCG_P3(m1, m2, m3), const fc4sc::detail::decay_t<decltype(m4)>& _fc4sc_arg_4
#define FC4SC_SCG_P5(m1, m2, m3, m4, m5) FC4SC_SCG_P4(m1, m2, m3, m4), const fc4sc::detail::decay_t<decltype(m5)>& _fc4sc_arg_5
#define FC4SC_SCG_P6(m1, m2, m3, m4, m5, m6) FC4SC_SCG_P5(m1, m2, m3, m4, m5), const fc4sc::detail::decay_t<decltype(m6)>& _fc4sc_arg_6
#define FC4SC_SCG_P7(m1, m2, m3, m4, m5, m6, m7) FC4SC_SCG_P6(m1, m2, m3, m4, m5, m6), const fc4sc::detail::decay_t<decltype(m7)>& _fc4sc_arg_7
#define FC4SC_SCG_P8(m1, m2, m3, m4, m5, m6, m7, m8) FC4SC_SCG_P7(m1, m2, m3, m4, m5, m6, m7), const fc4sc::detail::decay_t<decltype(m8)>& _fc4sc_arg_8
#define FC4SC_SCG_P9(m1, m2, m3, m4, m5, m6, m7, m8, m9) FC4SC_SCG_P8(m1, m2, m3, m4, m5, m6, m7, m8), const fc4sc::detail::decay_t<decltype(m9)>& _fc4sc_arg_9
#define FC4SC_SCG_P10(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10) FC4SC_SCG_P9(m1, m2, m3, m4, m5, m6, m7, m8, m9), const fc4sc::detail::decay_t<decltype(m10)>& _fc4sc_arg_10
#define FC4SC_SCG_P11(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11) FC4SC_SCG_P10(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10), const fc4sc::detail::decay_t<decltype(m11)>& _fc4sc_arg_11
#define FC4SC_SCG_P12(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12) FC4SC_SCG_P11(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11), const fc4sc::detail::decay_t<decltype(m12)>& _fc4sc_arg_12
#define FC4SC_SCG_P13(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13) FC4SC_SCG_P12(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12), const fc4sc::detail::decay_t<decltype(m13)>& _fc4sc_arg_13
#define FC4SC_SCG_P14(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14) FC4SC_SCG_P13(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13), const fc4sc::detail::decay_t<decltype(m14)>& _fc4sc_arg_14
#define FC4SC_SCG_P15(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15) FC4SC_SCG_P14(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14), const fc4sc::detail::decay_t<decltype(m15)>& _fc4sc_arg_15
#define FC4SC_SCG_P16(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16) FC4SC_SCG_P15(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15), const fc4sc::detail::decay_t<decltype(m16)>& _fc4sc_arg_16

#define FC4SC_SCG_A1(m1) this->m1 = _fc4sc_arg_1;
#define FC4SC_SCG_A2(m1, m2) FC4SC_SCG_A1(m1) this->m2 = _fc4sc_arg_2;
#define FC4SC_SCG_A3(m1, m2, m3) FC4SC_SCG_A2(m1, m2) this->m3 = _fc4sc_arg_3;
#define FC4SC_SCG_A4(m1, m2, m3, m4) FC4SC_SCG_A3(m1, m2, m3) this->m4 = _fc4sc_arg_4;
#define FC4SC_SCG_A5(m1, m2, m3, m4, m5) FC4SC_SCG_A4(m1, m2, m3, m4) this->m5 = _fc4sc_arg_5;
#define FC4SC_SCG_A6(m1, m2, m3, m4, m5, m6) FC4SC_SCG_A5(m1, m2, m3, m4, m5) this->m6 = _fc4sc_arg_6;
#define FC4SC_SCG_A7(m1, m2, m3, m4, m5, m6, m7) FC4SC_SCG_A6(m1, m2, m3, m4, m5, m6) this->m7 = _fc4sc_arg_7;
#define FC4SC_SCG_A8(m1, m2, m3, m4, m5, m6, m7, m8) FC4SC_SCG_A7(m1, m2, m3, m4, m5, m6, m7) this->m8 = _fc4sc_arg_8;
#define FC4SC_SCG_A9(m1, m2, m3, m4, m5, m6, m7, m8, m9) FC4SC_SCG_A8(m1, m2, m3, m4, m5, m6, m7, m8) this->m9 = _fc4sc_arg_9;
#define FC4SC_SCG_A10(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10) FC4SC_SCG_A9(m1, m2, m3, m4, m5, m6, m7, m8, m9) this->m10 = _fc4sc_arg_10;
#define FC4SC_SCG_A11(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11) FC4SC_SCG_A10(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10) this->m11 = _fc4sc_arg_11;
#define FC4SC_SCG_A12(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12) FC4SC_SCG_A11(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11) this->m12 = _fc4sc_arg_12;
#define FC4SC_SCG_A13(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13) FC4SC_SCG_A12(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12) this->m13 = _fc4sc_arg_13;
#define FC4SC_SCG_A14(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14) FC4SC_SCG_A13(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13) this->m14 = _fc4sc_arg_14;
#define FC4SC_SCG_A15(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15) FC4SC_SCG_A14(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14) this->m15 = _fc4sc_arg_15;
#define FC4SC_SCG_A16(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16) FC4SC_SCG_A15(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15) this->m16 = _fc4sc_arg_16;

#define FC4SC_SAMPLE_CG_N(N, ...) \
  virtual void sample(FC4SC_EXPAND(FC4SC_CAT(FC4SC_SCG_P, N)(__VA_ARGS__))) { \
    FC4SC_EXPAND(FC4SC_CAT(FC4SC_SCG_A, N)(__VA_ARGS__)) \
    fc4sc::covergroup::sample(); \
  }

#define FC4SC_SAMPLE_CG_DISPATCH(N, ...) FC4SC_SAMPLE_CG_N(N, __VA_ARGS__)
#define SAMPLE_CG(...) FC4SC_SAMPLE_CG_DISPATCH(FC4SC_NARGS(__VA_ARGS__), __VA_ARGS__)

// global var for type name and instance name of default scopes
// covergroups that are not associated with user-defined scopes automatically are associated with the default scope
#define _FC4SC_DEFAULT_SCOPE_TYPE_ "default_scope"
#define _FC4SC_DEFAULT_SCOPE_NAME_ "default_scope_instance"

/*!
 *  \namespace fc4sc
 *  \brief Namespace containing the library
 */
namespace fc4sc
{
namespace detail {

template <size_t... I>
struct index_seq {};

template <size_t N, size_t... I>
struct make_index_seq_impl : make_index_seq_impl<N - 1, N - 1, I...> {};

template <size_t... I>
struct make_index_seq_impl<0, I...> {
  typedef index_seq<I...> type;
};

template <size_t N>
using make_index_seq = typename make_index_seq_impl<N>::type;

template <typename F, typename Tuple, size_t... I>
inline auto apply_tuple_impl(F&& f, Tuple&& t, index_seq<I...>)
    -> decltype(std::forward<F>(f)(std::get<I>(std::forward<Tuple>(t))...)) {
  return std::forward<F>(f)(std::get<I>(std::forward<Tuple>(t))...);
}

template <typename F, typename Tuple>
inline auto apply_tuple(F&& f, Tuple&& t)
    -> decltype(apply_tuple_impl(std::forward<F>(f), std::forward<Tuple>(t),
                                 make_index_seq<std::tuple_size<decay_t<Tuple>>::value>())) {
  return apply_tuple_impl(std::forward<F>(f), std::forward<Tuple>(t),
                          make_index_seq<std::tuple_size<decay_t<Tuple>>::value>());
}

template <typename F, typename Tuple, typename Seq = make_index_seq<std::tuple_size<Tuple>::value>>
struct is_tuple_unpack_invocable;

template <typename F, typename Tuple, size_t... I>
struct is_tuple_unpack_invocable<F, Tuple, index_seq<I...>> {
  template <typename G>
  static auto test(int)
      -> decltype(static_cast<bool>(std::declval<G>()(std::declval<const typename std::tuple_element<I, Tuple>::type&>()...)),
                  std::true_type());
  template <typename>
  static std::false_type test(...);
  static constexpr bool value = decltype(test<F>(0))::value;
};

template <typename F, typename Tuple>
struct is_tuple_arg_invocable {
  template <typename G>
  static auto test(int)
      -> decltype(static_cast<bool>(std::declval<G>()(std::declval<const Tuple&>())),
                  std::true_type());
  template <typename>
  static std::false_type test(...);
  static constexpr bool value = decltype(test<F>(0))::value;
};

template <typename... Args, typename F,
          typename std::enable_if<is_tuple_unpack_invocable<F, std::tuple<Args...>>::value, int>::type = 0>
inline std::function<bool(const void*)> wrap_cross_predicate(F pred) {
  return [pred](const void* ptr) -> bool {
    const auto* tup = static_cast<const std::tuple<Args...>*>(ptr);
    return static_cast<bool>(apply_tuple(pred, *tup));
  };
}

template <typename... Args, typename F,
          typename std::enable_if<!is_tuple_unpack_invocable<F, std::tuple<Args...>>::value &&
                                  is_tuple_arg_invocable<F, std::tuple<Args...>>::value, int>::type = 0>
inline std::function<bool(const void*)> wrap_cross_predicate(F pred) {
  return [pred](const void* ptr) -> bool {
    const auto* tup = static_cast<const std::tuple<Args...>*>(ptr);
    return static_cast<bool>(pred(*tup));
  };
}

template <typename... Args>
struct cross_tuple_traits {
  static const std::type_info* type() { return &typeid(std::tuple<Args...>); }
  template <typename F>
  static std::function<bool(const void*)> wrap(F pred) {
    return wrap_cross_predicate<Args...>(std::move(pred));
  }
};

template <typename T>
struct callable_sig;

template <typename R, typename... Args>
struct callable_sig<R(*)(Args...)> : cross_tuple_traits<decay_t<Args>...> {};

template <typename C, typename R, typename... Args>
struct callable_sig<R(C::*)(Args...) const> : cross_tuple_traits<decay_t<Args>...> {};

template <typename C, typename R, typename... Args>
struct callable_sig<R(C::*)(Args...)> : cross_tuple_traits<decay_t<Args>...> {};

template <typename R, typename... Args>
struct callable_sig<R(*)(const std::tuple<Args...>&)> : cross_tuple_traits<decay_t<Args>...> {};

template <typename R, typename... Args>
struct callable_sig<R(*)(std::tuple<Args...>)> : cross_tuple_traits<decay_t<Args>...> {};

template <typename C, typename R, typename... Args>
struct callable_sig<R(C::*)(const std::tuple<Args...>&) const> : cross_tuple_traits<decay_t<Args>...> {};

template <typename C, typename R, typename... Args>
struct callable_sig<R(C::*)(const std::tuple<Args...>&)> : cross_tuple_traits<decay_t<Args>...> {};

template <typename C, typename R, typename... Args>
struct callable_sig<R(C::*)(std::tuple<Args...>) const> : cross_tuple_traits<decay_t<Args>...> {};

template <typename C, typename R, typename... Args>
struct callable_sig<R(C::*)(std::tuple<Args...>)> : cross_tuple_traits<decay_t<Args>...> {};

template <typename F>
struct callable_traits : callable_sig<decltype(&decay_t<F>::operator())> {};

template <typename R, typename... Args>
struct callable_traits<R(*)(Args...)> : callable_sig<R(*)(Args...)> {};

template <typename R, typename... Args>
struct callable_traits<R(&)(Args...)> : callable_sig<R(*)(Args...)> {};

template <typename R, typename... Args>
struct callable_traits<std::function<R(Args...)>> : callable_sig<R(*)(Args...)> {};

struct derived_type_info {
  const char* type_name;
  const char* file_name;
  int line;
  const char* scp_type_name;
  bool active;
  int depth;
};

inline derived_type_info& current_derived_type_info() {
  static thread_local derived_type_info info = {nullptr, nullptr, 0, nullptr, false, 0};
  return info;
}

class derived_type_guard {
public:
  derived_type_guard(const char* type_name, const char* file_name, int line,
                     const char* scp_type_name = nullptr)
      : prev_(current_derived_type_info()), applied_(false) {
    derived_type_info& info = current_derived_type_info();
    if (info.depth == 0 || !info.active) {
      info.type_name = type_name;
      info.file_name = file_name;
      info.line = line;
      info.scp_type_name = scp_type_name;
      info.active = true;
      applied_ = true;
    }
    ++info.depth;
  }

  derived_type_guard(const derived_type_guard&) = delete;
  derived_type_guard& operator=(const derived_type_guard&) = delete;

  ~derived_type_guard() {
    derived_type_info& info = current_derived_type_info();
    if (applied_) {
      info = prev_;
    } else {
      --info.depth;
      if (info.depth <= 0) {
        info.depth = 0;
        info.active = false;
        info.type_name = nullptr;
        info.file_name = nullptr;
        info.line = 0;
        info.scp_type_name = nullptr;
      }
    }
  }

private:
  derived_type_info prev_;
  bool applied_;
};

inline const std::string& forward_derived_type(const std::string& inst_name,
                                               const derived_type_guard&) {
  return inst_name;
}

} // namespace detail

using cvp_metadata_t = std::tuple<void*, std::string, std::string>;

class covVisitorBase;
class covergroup;
class cvg_base;
class cvp_base;
class scp_base;
class cross_base;
class coverpoint_base;
class bin_base;
class dynamic_covergroup_factory;
class scp_base_data_model;
class cvg_base_data_model;
class coverpoint_base_data_model;
class cross_base_data_model;
class bin_base_data_model;
class global;
class cvg_metadata;
class scp_metadata;

struct uncovered_bin_info {
  std::string name;
  size_t index = 0;
  std::vector<size_t> cross_product;
  uint64_t hits = 0;
  unsigned int at_least = 1;
  bool is_cross = false;
  bool is_auto = false;
  bool is_transition = false;
};

namespace detail {

class strobe_backend {
public:
  virtual ~strobe_backend() {}
  virtual void schedule(covergroup* cg) = 0;
  virtual void cancel(covergroup* cg) = 0;
};

inline strobe_backend*& active_strobe_backend() {
  static strobe_backend* backend = nullptr;
  return backend;
}

} // namespace detail
/*!
 *  \brief Alias to std::make_pair
 *  \param t1 One end of the interval
 *  \param t2 Other end of the interval
 *  \returns Pair of t1 and t2
 *
 *    Just a wrapper over make_pair to declare value intervals
 *  in a more verbose way.
 */
template <typename T>
static interval_t<T> interval(T t1, T t2) {
  return interval_t<T>(t1, t2);
}

/*!
 * \brief Define a type for bin types
 */
typedef enum
{
  default_,
  illegal_,
  ignore_,
  /*!
   * A default bin, as the SystemVerilog "bins b = default;": it holds the
   * values which are in no other bin, and is not counted in the coverage.
   * default_ is the type of the regular bins.
   */
  default_bin_
} bin_t;

/*!
 *  \class covVisitorBase fc_base.hpp
 *  \brief Base class for coverage model visitor
 *
 *  Basic interface to introspect the model via Visitor Pattern.
 *  This class is intended to be extended by the user
 */
class covVisitorBase {
public:
 
  /*! visit a scope's data model */
  virtual void visit(scp_base_data_model&) = 0;

  /*! visit a covergroup's data model */
  virtual void visit(cvg_base_data_model&) = 0;

  /*! visit a coverpoint's data model */
  virtual void visit(coverpoint_base_data_model&) = 0;

  /*! visit a cross's data model */
  virtual void visit(cross_base_data_model&) = 0;

  /*! visit a bin's data model */
  virtual void visit(bin_base_data_model&) = 0;

};

/*!
 *  \class bin_base_data_model fc_base.hpp
 *  \brief Base class for bin_data_model class
 *
 *  Base class with getters to coverage data objects corresponding to bins.
 *  The purpose of the getters is for coverage model introspection.
 */
class bin_base_data_model {
public:
 
  /*! Indicates if the data within this object is valid */
  std::shared_ptr<bool> valid = std::make_shared<bool>(true);

  /*!
   * Get intervals in type int. The values which do not fit in an int, as
   * the values of a 64-bit type or the values of an unsigned int above
   * INT_MAX, are truncated: get_intervals_decimal() has them in full.
   */
  virtual std::vector<interval_t<int>> get_intervals_to_int() const = 0;

  /*!
   * Get the value sequences of a transition bin in type int. The hit
   * counts in get_interval_hits() belong to these sequences, and a bin has
   * either intervals or sequences. The values are truncated to an int as
   * in get_intervals_to_int(): get_sequences_decimal() has them in full.
   */
  virtual std::vector<std::vector<int>> get_sequences_to_int() const
  {
    return std::vector<std::vector<int>>();
  }

  /*!
   * Get the intervals with their limits written in decimal, in full: a
   * value of any integral type, up to 64 bits, signed or unsigned. These
   * are the values written to the coverage database.
   */
  virtual std::vector<interval_t<std::string>> get_intervals_decimal() const
  {
    std::vector<interval_t<std::string>> intervals;
    for (auto &intv : get_intervals_to_int())
      intervals.push_back(interval_t<std::string>(std::to_string(intv.first), std::to_string(intv.second)));
    return intervals;
  }

  /*!
   * Get the value sequences of a transition bin with their values written
   * in decimal, in full, as get_intervals_decimal()
   */
  virtual std::vector<std::vector<std::string>> get_sequences_decimal() const
  {
    std::vector<std::vector<std::string>> sequences;
    for (auto &seq : get_sequences_to_int())
    {
      sequences.push_back(std::vector<std::string>());
      for (auto value : seq)
        sequences.back().push_back(std::to_string(value));
    }
    return sequences;
  }

  /*!
   * For a default bin array which has values but no value hit, a range of
   * its first value with no hits, written in decimal: the file has a bin
   * of the array for it, so that the coverpoint has a bin. Empty otherwise.
   */
  virtual std::vector<std::tuple<std::string, std::string, uint64_t>> get_unhit_array_range() const
  {
    return std::vector<std::tuple<std::string, std::string, uint64_t>>();
  }

  /*!
   * Get the value ranges of a default bin with their hit counts, the
   * values written in decimal, as they may not fit in an int. For a default
   * bin array, as the SystemVerilog "bins b[] = default;", which has a bin
   * for each value hit, a range of one value for each value hit. Empty for
   * other bins.
   */
  virtual std::vector<std::tuple<std::string, std::string, uint64_t>> get_default_ranges() const
  {
    return std::vector<std::tuple<std::string, std::string, uint64_t>>();
  }

  /*! Whether this is a default bin array, with a bin for each value hit */
  virtual bool is_value_array() const
  {
    return false;
  }

  /*! Whether this is a transition bin */
  virtual bool is_transition_bin() const
  {
    return false;
  }

  /*!
   * Adds hits to a value of a default bin array, the value written in
   * decimal. Returns false if this is not a default bin array, or if the
   * value is not one of its values.
   */
  virtual bool add_value_hits(const std::string& value, uint64_t count)
  {
    (void)value;
    (void)count;
    return false;
  }

  virtual std::vector<xml_user_attr> get_user_attrs() const
  {
    return std::vector<xml_user_attr>();
  }

  /*! Get name of bin */
  virtual std::string& get_name() = 0;

  /*! Get bin type */
  virtual bin_t& get_bin_type() = 0;

  /*! Get hit count for each interval in bin */
  virtual std::vector<uint64_t>& get_interval_hits() = 0;

  /*! Visitor Pattern for introspection */
  virtual void accept_visitor(covVisitorBase& visitor) = 0;

  /*! Destructor */
  virtual ~bin_base_data_model() { }

};

/*!
 *  \class bin_base fc_base.hpp
 *  \brief Base class for bins
 *
 *  Basic interface to work with untemplated bins
 */
class bin_base
{
public:
  /*!
   * \brief Returns total number of hits
   * \returns number of times the sampled value was in the bin
   */
  virtual uint64_t get_hitcount() const = 0;
 
  /*! Get intervals casted as int type, the values which do not fit in an
   *  int truncated */
  virtual std::vector<interval_t<int>> get_intervals_to_int() const = 0;

  /*! Get intervals with their limits written in decimal, in full */
  virtual std::vector<interval_t<std::string>> get_intervals_decimal() const
  {
    std::vector<interval_t<std::string>> intervals;
    for (auto &intv : get_intervals_to_int())
      intervals.push_back(interval_t<std::string>(std::to_string(intv.first), std::to_string(intv.second)));
    return intervals;
  }

  /*! Get reference to bin name */
  virtual std::string& name() = 0;

  /*! Get const reference to bin name */
  virtual const std::string& name() const = 0;

  /*! Get reference to bin type */
  virtual bin_t& bin_type() = 0;

  /*! Get const reference to bin type */
  virtual const bin_t& bin_type() const = 0;

  /*! Get reference to bin counts */
  virtual std::vector<uint64_t>& interval_hits() = 0;

  /*! Get const reference to bin counts */
  virtual const std::vector<uint64_t>& interval_hits() const = 0;

  /*! Destructor */
  virtual ~bin_base(){}
};

/*!
 *  \class api_base_data_model fc_base.hpp
 *  \brief Base class for api_data_model class
 *
 *  Base class that contains the name of coverage object and valid ptr.
 */
class api_data_model {
public:

  /*! Valid pointer for garbage collection */
  std::shared_ptr<bool> valid = std::make_shared<bool>(true);

  /*! Coverage object name */
  std::string name;

};

/*!
 *  \class bin_base fc_base.hpp
 *  \brief Base class for bins
 *
 *  Basic interface for coverage objects that implements coverage calculation functions
 */
class api_base
{
public:

  /*! Get reference to coverage object name */
  virtual std::string& name() = 0;

  /*! Get const reference to coverage object name */
  virtual const std::string& name() const = 0;

  /** \name Coverage API
   *  API for getting and controlling coverage collection at run time
   */
  /**@{*/

  /*!
   * \brief Returns the coverage associated with this instance
   * \returns Double between 0 and 100
   */
  virtual double get_inst_coverage() const = 0;

  /*!
   * \brief Returns the coverage associated with this instance
   * \param hit Number of hit bins in this instance
   * \param total Total number of bins in this instance
   * \returns Double between 0 and 100
   */
  virtual double get_inst_coverage(int &hit, int &total) const = 0;

  /*!
   * \brief Enables sampling on this instance
   */
  virtual void start() = 0;

  /*!
   * \brief Stops sampling on this instance
   */
  virtual void stop() = 0;
  /**@}*/

  /*! Sample coverage object */
  virtual void sample() = 0;

  /*! Destructor */
  virtual ~api_base(){}
};

/*!
 *  \class cvp_base_data_model fc_base.hpp
 *  \brief Base class for coverpoints and crosses
 *
 *  Basic interface to work with untemplated coverpoints and crosses
 */
class cvp_base_data_model : public api_data_model
{
public:

  /*! Number of sample misses (no bin hit)*/
  uint64_t misses = 0;

  /*! Data of the covergroup instance this coverpoint or cross is in,
   *  or nullptr if it is in none */
  cvg_base_data_model* parent_cvg = nullptr;

  /*! Owning coverpoint/cross wrapper for deferred bin materialization */
  cvp_base* owner_ = nullptr;

  /*! Re-entrancy guard for ensure_bins / ensure_cross */
  mutable bool in_ensure_bins_ = false;

  /*! Type options of a coverpoint or cross which is in no covergroup */
  cvp_type_option own_type_option;

  /*!
   * \brief Returns the type options of this coverpoint or cross, which
   * are shared by the coverpoints or crosses with its name in all the
   * instances of its covergroup type
   */
  cvp_type_option& type_option();

  /*! Propagate inherited options from parent covergroup */
  virtual void sync_options_from_parent() {}

  /*! Visitor function for introspection */
  virtual void accept_visitor(covVisitorBase&) = 0;

  /*! Get coverpoint size */
  virtual uint64_t size() const = 0;

  /*! Destructor */
  virtual ~cvp_base_data_model() { }

};

/*!
 *  \class cvp_base fc_base.hpp
 *  \brief Base class for coverpoints and crosses
 *
 *  Basic interface to work with untemplated coverpoints and crosses
 */
class cvp_base : public api_base
{
protected:
  bool stop_sample_on_first_bin_hit = false;

public:
  size_t last_bin_index_hit = 0;
  bool last_sample_success = false;

  /*!
   * Indices of all the bins hit by the last sample, in increasing order.
   * A sample can hit several bins of a coverpoint: bins with overlapping
   * values, or a value bin and transition bins. A cross counts every
   * combination of them.
   */
  std::vector<size_t> last_bins_hit;

  virtual cvp_base* create_instance(cvg_base* cvg_inst) = 0;

  /*! Materialize automatic / distribute_first bins when needed */
  virtual void ensure_bins() const {}

  /*! Whether bin definitions depend on post-ctor options (auto_bin_max / distribute_first) */
  virtual bool has_deferred_bins() const { return false; }

  /*! Hook invoked when parent covergroup is sampled while stopped */
  virtual void on_stopped_sample() {}

  /*! Whether this coverpoint or cross is currently collecting samples */
  virtual bool is_collecting() const { return true; }

  /*! Epoch counter incremented whenever coverpoint bins are rebuilt */
  virtual uint64_t bins_epoch() const { return 0; }

  /*! Instance specific options */
  virtual uint64_t get_weight() = 0;

  /*! Get coverpoint size */
  virtual uint64_t size() const = 0;

  /*! Get total bin misses */
  virtual uint64_t get_misses() = 0;

  /*! Get pointer to data model */
  virtual cvp_base_data_model* get_data() = 0;

  /*! Get const pointer to data model */
  virtual const cvp_base_data_model* get_data() const = 0;

  /*!
   * \brief Returns the type options of this coverpoint or cross, shared by
   * all the instances of its covergroup type, see \link cvp_type_option
   * \endlink
   */
  cvp_type_option& type_option()
  {
    return get_data()->type_option();
  }

  /*! Const overload of type_option() */
  const cvp_type_option& type_option() const;

  /*! Get names of uncovered bins (hits < option.at_least, excluding ignore/illegal/default) */
  virtual std::vector<std::string> get_uncovered_bins() const = 0;

  /*! Get structured metadata for uncovered bins */
  virtual std::vector<uncovered_bin_info> get_uncovered_bin_infos() const = 0;

  /*!
   * \brief Returns the type coverage of this coverpoint or cross: its
   * coverage across all the instances of the covergroup type it is in
   * \returns Double between 0 and 100
   */
  virtual double get_coverage() const = 0;

  /*!
   * \brief Returns the type coverage of this coverpoint or cross
   * \param covered Number of covered bins, summed over the instances
   * \param total Total number of bins, summed over the instances
   * \returns Double between 0 and 100
   */
  virtual double get_coverage(int &covered, int &total) const = 0;

  /*! Destructor */
  virtual ~cvp_base(){}
};

/*!
 *  \class coverpoint_base_data_model fc_base.hpp
 *  \brief Base class for data of coverpoints
 *
 *  Basic interface to work with data of coverpoints
 */
class coverpoint_base_data_model : public cvp_base_data_model
{
public:

  /*! Coverpoint options */
  cvp_option option;

  /*! Last synced option values from parent covergroup */
  cvp_option synced_option_;

  /*! Per-field override flags for inherited options */
  bool at_least_overridden_ = false;
  bool auto_bin_max_overridden_ = false;
  bool detect_overlap_overridden_ = false;

  /*! Propagate inherited options from parent covergroup */
  void sync_options_from_parent() override;

  /*! Vector of pointers to regular bin data */
  std::vector<bin_base_data_model*> bins_data;

  /*! Vector of pointers to illegal bin data */
  std::vector<bin_base_data_model*> illegal_bins_data;

  /*! Vector of pointers to ignore bin data */
  std::vector<bin_base_data_model*> ignore_bins_data;

  /*! Vector of pointers to default bin data, see bin_t::default_bin_ */
  std::vector<bin_base_data_model*> default_bins_data;

  /*! Get reference to sample expression string */
  virtual std::string& get_sample_expression_str() = 0;

  /*! Get reference to sample condition string */
  virtual std::string& get_sample_condition_str() = 0;

  /*! Destructor */
  virtual ~coverpoint_base_data_model()
  {
    for(auto bin_it : this->bins_data) {
      delete bin_it;
    }
    for(auto bin_it : this->illegal_bins_data) {
      delete bin_it;
    }
    for(auto bin_it : this->ignore_bins_data) {
      delete bin_it;
    }
    for(auto bin_it : this->default_bins_data) {
      delete bin_it;
    }
  }

};

/*!
 *  \class coverpoint_base
 *  \brief non-template coverpoint class
 *
 *  Interface to introspect the coverage model
 */
class coverpoint_base : public cvp_base
{
public:
  
  /*! Get reference to coverpoint options */
  virtual cvp_option& option() = 0;

  /*! Get const reference to coverpoint options */
  virtual const cvp_option& option() const = 0;

  /*! Get reference to sample_expression string */
  virtual std::string& get_sample_expression_str() = 0;

  /*! Get vector of regular bins associated with this coverpoint */
  virtual std::vector<bin_base*> get_bins_base() = 0;

  /*! Get const vector of regular bins associated with this coverpoint */
  virtual std::vector<const bin_base*> get_bins_base() const = 0;

  /*! Get vector of illegal bins associated with this coverpoint */
  virtual std::vector<bin_base*> get_illegal_bins_base() = 0;

  /*! Get const vector of illegal bins associated with this coverpoint */
  virtual std::vector<const bin_base*> get_illegal_bins_base() const = 0;

  /*! Get vector of ignore bins associated with this coverpoint */
  virtual std::vector<bin_base*> get_ignore_bins_base() = 0;

  /*! Get const vector of ignore bins associated with this coverpoint */
  virtual std::vector<const bin_base*> get_ignore_bins_base() const = 0;

  /*! Get vector of default bins associated with this coverpoint */
  virtual std::vector<bin_base*> get_default_bins_base()
  {
    return std::vector<bin_base*>();
  }

  /*! Get const vector of default bins associated with this coverpoint */
  virtual std::vector<const bin_base*> get_default_bins_base() const
  {
    return std::vector<const bin_base*>();
  }

  /*! Get uncovered intervals formatted as strings */
  virtual std::vector<interval_t<std::string>> get_uncovered_intervals_str() const = 0;

  /*! Get uncovered intervals converted to int (truncated if wider than int) */
  virtual std::vector<interval_t<int>> get_uncovered_intervals_to_int() const = 0;

  /*! Destructor */
  virtual ~coverpoint_base() { }

};

/*!
 *  \class cross_select_node fc_base.hpp
 *  \brief Node of a select expression of the bins of a cross
 *
 *  A select expression, as binsof(a.b) && !binsof(c) intersect {1},
 *  selects products of the bins of the crossed coverpoints. A condition
 *  selects the products which have one of a set of bins of a coverpoint,
 *  and conditions are combined with !, && and ||. See fc4sc::binsof.
 */
class cross_select_node
{
public:

  /*! Kind of the node */
  enum op_t { condition_, not_, and_, or_, all_, with_, queue_ };

  op_t op = condition_;

  /*! Operands of !, &&, || and with_ */
  std::shared_ptr<const cross_select_node> lhs, rhs;

  /*! Coverpoint of a condition or single-coverpoint with_ */
  const cvp_base* cvp = nullptr;

  /*! Returns which bins of the coverpoint a condition selects */
  std::function<std::vector<bool>(const cvp_base*)> select_bins;

  /*! Position of the coverpoint in the cross, once resolved */
  size_t position = 0;

  /*! Bins of the coverpoint selected by a condition, once resolved */
  std::vector<bool> bins;

  /*! Value-level predicate on a single coverpoint (binsof<T>.with) */
  std::function<bool(const void*)> single_cvp_pred;

  /*! Value-tuple predicate (cross_bin.with / select_expression.with) */
  std::function<bool(const void*)> tuple_pred;

  /*! Runtime type of std::tuple<Args...> for tuple_pred and queue_ */
  const std::type_info* tuple_type = nullptr;

  /*! Sorted CrossQueueType membership predicate */
  std::function<bool(const void*)> queue_contains;

  /*! Resolves CrossQueueType into bin-index products and matched tuple count */
  std::function<std::vector<std::vector<size_t>>(
      const std::vector<cvp_base*>&,
      const std::function<bool(const void*)>&,
      uint64_t&)> queue_hit_products;

  /*! Evaluates finite CrossQueueType tuples with custom accept predicate (returns accepted tuple count) */
  std::function<uint64_t(
      const std::vector<cvp_base*>&,
      const std::function<bool(const std::vector<size_t>&, const void*, const void* const*)>&)> queue_count_tuples;

  /*! Visits finite CrossQueueType tuples (stops early when visitor returns false) */
  std::function<void(
      const std::vector<cvp_base*>&,
      const std::function<bool(const std::vector<size_t>&, const void*, const void* const*)>&)> queue_for_each_tuple;

  /*! Sorted unique bin-index products reachable by queue_, once resolved */
  std::vector<std::vector<size_t>> queue_products;

  bool has_with_node() const
  {
    if (op == with_) return true;
    if (lhs && lhs->has_with_node()) return true;
    if (rhs && rhs->has_with_node()) return true;
    return false;
  }

  bool has_queue_node() const
  {
    if (op == queue_) return true;
    if (lhs && lhs->has_queue_node()) return true;
    if (rhs && rhs->has_queue_node()) return true;
    return false;
  }

  bool is_pure_queue_node() const
  {
    return op == queue_;
  }

  const cross_select_node* get_underlying_queue_node() const
  {
    if (op == queue_) return this;
    if (op == with_ && lhs) return lhs->get_underlying_queue_node();
    return nullptr;
  }

  void validate_tuple_type(const std::type_info* expected) const
  {
    if (tuple_type != nullptr && expected != nullptr && *tuple_type != *expected) {
      throw std::invalid_argument("cross select expression or CrossQueueType tuple type does not match cross coverpoint types");
    }
    if (lhs) lhs->validate_tuple_type(expected);
    if (rhs) rhs->validate_tuple_type(expected);
  }

  /*!
   * \brief Checks if a product of bins is structurally selected
   * \param product The index of a bin of each crossed coverpoint
   */
  bool matches(const std::vector<size_t>& product) const
  {
    switch (op)
    {
      case all_: return true;
      case not_: return !lhs->matches(product);
      case and_: return lhs->matches(product) && rhs->matches(product);
      case or_: return lhs->matches(product) || rhs->matches(product);
      case with_: return lhs ? lhs->matches(product) : true;
      case queue_:
        return std::binary_search(queue_products.begin(), queue_products.end(), product);
      default:
        return position < product.size() && product[position] < bins.size() &&
               bins[product[position]];
    }
  }

  /*!
   * \brief Sound under-approximation: true only if every tuple in \p product matches
   */
  bool must_match(const std::vector<size_t>& product) const
  {
    switch (op)
    {
      case all_: return true;
      case not_: return lhs ? !lhs->may_match(product) : false;
      case and_: return lhs && rhs && lhs->must_match(product) && rhs->must_match(product);
      case or_: return (lhs && lhs->must_match(product)) || (rhs && rhs->must_match(product));
      case with_: return false;
      case queue_: return false;
      default:
        return position < product.size() && product[position] < bins.size() &&
               bins[product[position]];
    }
  }

  /*!
   * \brief Sound over-approximation: true if any tuple in \p product may match
   */
  bool may_match(const std::vector<size_t>& product) const
  {
    switch (op)
    {
      case all_: return true;
      case not_: return lhs ? !lhs->must_match(product) : false;
      case and_: return lhs && rhs && lhs->may_match(product) && rhs->may_match(product);
      case or_: return (lhs && lhs->may_match(product)) || (rhs && rhs->may_match(product));
      case with_: return lhs ? lhs->may_match(product) : true;
      case queue_:
        return std::binary_search(queue_products.begin(), queue_products.end(), product);
      default:
        return position < product.size() && product[position] < bins.size() &&
               bins[product[position]];
    }
  }

  /*!
   * \brief Checks if a value tuple (and its bin product) is selected
   */
  bool matches_tuple(const std::vector<size_t>& product,
                     const void* tuple_ptr,
                     const void* const* elem_ptrs) const
  {
    switch (op)
    {
      case all_: return true;
      case not_: return !lhs->matches_tuple(product, tuple_ptr, elem_ptrs);
      case and_:
        return lhs->matches_tuple(product, tuple_ptr, elem_ptrs) &&
               rhs->matches_tuple(product, tuple_ptr, elem_ptrs);
      case or_:
        return lhs->matches_tuple(product, tuple_ptr, elem_ptrs) ||
               rhs->matches_tuple(product, tuple_ptr, elem_ptrs);
      case with_:
        if (lhs && !lhs->matches_tuple(product, tuple_ptr, elem_ptrs))
          return false;
        if (single_cvp_pred) {
          if (elem_ptrs == nullptr || position >= product.size() || elem_ptrs[position] == nullptr)
            return true;
          return single_cvp_pred(elem_ptrs[position]);
        }
        if (tuple_pred) {
          if (tuple_ptr == nullptr) return true;
          return tuple_pred(tuple_ptr);
        }
        return true;
      case queue_:
        if (tuple_ptr != nullptr && queue_contains)
          return matches(product) && queue_contains(tuple_ptr);
        return matches(product);
      default:
        return matches(product);
    }
  }

  /*!
   * \brief Returns a copy of this expression whose conditions know the
   * positions and the selected bins of their coverpoints in a cross
   * \param cvps The crossed coverpoints
   */
  std::shared_ptr<const cross_select_node> resolve(
      const std::vector<cvp_base*>& cvps,
      const std::vector<cvp_base*>* orig_cvps = nullptr,
      const std::function<bool(const void*)>* cross_with_pred = nullptr) const
  {
    std::shared_ptr<cross_select_node> node = std::make_shared<cross_select_node>(*this);
    if (op == all_) {
      return node;
    }
    if (op == not_ || op == and_ || op == or_) {
      node->lhs = lhs->resolve(cvps, orig_cvps, cross_with_pred);
      if (rhs) node->rhs = rhs->resolve(cvps, orig_cvps, cross_with_pred);
      return node;
    }
    if (op == with_) {
      if (lhs) node->lhs = lhs->resolve(cvps, orig_cvps, cross_with_pred);
      if (single_cvp_pred) {
        const cvp_base* target_cvp = cvp;
        if (orig_cvps != nullptr && orig_cvps->size() == cvps.size()) {
          auto oit = std::find(orig_cvps->begin(), orig_cvps->end(), cvp);
          if (oit != orig_cvps->end()) {
            target_cvp = cvps[static_cast<size_t>(oit - orig_cvps->begin())];
            node->cvp = target_cvp;
          }
        }
        auto it = std::find(cvps.begin(), cvps.end(), target_cvp);
        if (it == cvps.end())
          throw std::invalid_argument("binsof refers to a coverpoint which is not in the cross");
        node->position = static_cast<size_t>(it - cvps.begin());
      }
      return node;
    }
    if (op == queue_) {
      if (queue_hit_products) {
        uint64_t dummy = 0;
        std::function<bool(const void*)> empty_pred;
        node->queue_products = queue_hit_products(
            cvps, cross_with_pred ? *cross_with_pred : empty_pred, dummy);
        std::sort(node->queue_products.begin(), node->queue_products.end());
        node->queue_products.erase(
            std::unique(node->queue_products.begin(), node->queue_products.end()),
            node->queue_products.end());
      }
      return node;
    }
    const cvp_base* target_cvp = cvp;
    if (orig_cvps != nullptr && orig_cvps->size() == cvps.size()) {
      auto oit = std::find(orig_cvps->begin(), orig_cvps->end(), cvp);
      if (oit != orig_cvps->end()) {
        target_cvp = cvps[static_cast<size_t>(oit - orig_cvps->begin())];
        node->cvp = target_cvp;
      }
    }
    auto it = std::find(cvps.begin(), cvps.end(), target_cvp);
    if (it == cvps.end())
      throw std::invalid_argument("binsof refers to a coverpoint which is not in the cross");
    node->position = it - cvps.begin();
    node->bins = select_bins(target_cvp);
    return node;
  }

  void validate_non_deferred(const std::vector<cvp_base*>& cvps) const
  {
    if (op == all_ || op == queue_) return;
    if (op == not_ || op == and_ || op == or_) {
      lhs->validate_non_deferred(cvps);
      if (rhs) rhs->validate_non_deferred(cvps);
      return;
    }
    if (op == with_) {
      if (lhs) lhs->validate_non_deferred(cvps);
      if (single_cvp_pred && cvp != nullptr) {
        auto it = std::find(cvps.begin(), cvps.end(), cvp);
        if (it == cvps.end())
          throw std::invalid_argument("binsof refers to a coverpoint which is not in the cross");
      }
      return;
    }
    auto it = std::find(cvps.begin(), cvps.end(), cvp);
    if (it == cvps.end())
      throw std::invalid_argument("binsof refers to a coverpoint which is not in the cross");
    if (!cvp->has_deferred_bins()) {
      (void)select_bins(cvp);
    }
  }
};

/*!
 *  \class cross_bin_data_model fc_base.hpp
 *  \brief Data of a user defined bin of a cross
 *
 *  A user defined bin of a cross holds the products of bins which its
 *  select expression selects, as the SystemVerilog
 *  "bins b = binsof(a.lo) && binsof(c);". An ignore bin or an illegal bin
 *  of a cross has the type bin_t::ignore_ or bin_t::illegal_.
 */
class cross_bin_data_model
{
public:

  /*! Name of the bin */
  std::string name;

  /*! Type of the bin: default_, ignore_ or illegal_ */
  bin_t bin_type = default_;

  /*! Select expression of the bin */
  std::shared_ptr<const cross_select_node> select;

  /*! Optional per-bin iff guard */
  std::function<bool()> iff_cond;

  /*!
   * Number of products of bins in the bin. The products of an ignore
   * or illegal bin are not in the other bins. A bin without products is
   * not counted in the coverage.
   */
  uint64_t size = 0;

  /*! Number of samples which hit the bin */
  uint64_t hits = 0;
};

/*!
 *  \class cross_base_data_model fc_base.hpp
 *  \brief Base class for data of crosses
 *
 *  Basic interface to work with data of crosses
 */
class cross_base_data_model : public cvp_base_data_model
{
public:

  /*! Cross options */
  cross_option option;

  /*! Last synced option values from parent covergroup */
  cross_option synced_option_;

  /*! Per-field override flags for inherited options */
  bool at_least_overridden_ = false;
  bool cross_num_print_missing_overridden_ = false;

  /*! Propagate inherited options from parent covergroup */
  void sync_options_from_parent() override;

  /*! Vector of crossed coverpoints */
  std::vector<cvp_base_data_model*> cross_cvps;

  /*!
   * User defined bins, ignore bins and illegal bins of the cross. When
   * there are any, get_cross_bins() holds the hits of the automatically
   * created bins only: the products of bins which are in none of them.
   */
  std::vector<cross_bin_data_model> user_bins;

  /*! Number of automatically created bins, when there are user_bins */
  uint64_t auto_bins_size = 0;

  /*! Whether auto bins are filtered by user_bins or cross-level with() */
  bool has_bin_filter_ = false;

  /*! Whether this cross contains at least one default_cross_bin */
  bool has_default_bin_ = false;

  /*! Cached products excluded from automatically created bins */
  std::set<std::vector<size_t>> excluded_auto_products_;

  bool has_default_bin() const
  {
    if (has_default_bin_) return true;
    for (const auto& b : user_bins)
      if (b.bin_type == default_bin_) return true;
    return false;
  }

  bool has_custom_auto_bins() const
  {
    return has_bin_filter_ || !user_bins.empty();
  }

  /*! Get cross bins */
  virtual const std::map<std::vector<size_t>, uint64_t>& get_cross_bins() const = 0;

  /*!
   * \brief Adds hits to an automatically created bin
   * \param product The index of a bin of each crossed coverpoint
   * \param count Number of hits to add
   */
  virtual void add_cross_bin_hits(const std::vector<size_t>& product, uint64_t count) = 0;

  /*!
   * \brief Checks if a product of bins is an automatically created bin,
   * which is in no user defined bin, ignore bin, illegal bin, or default bin
   * \param product The index of a bin of each crossed coverpoint
   */
  bool is_auto_bin(const std::vector<size_t>& product) const
  {
    if (has_default_bin()) return false;
    if (has_bin_filter_)
      return excluded_auto_products_.count(product) == 0;
    for (auto &bin : user_bins) {
      if (bin.bin_type == default_bin_) return false;
      if (bin.select && bin.select->matches(product))
        return false;
    }
    return true;
  }

  /*!
   * \brief Counts the bins of the cross which are in its coverage: the
   * automatically created bins and the user defined bins which have
   * products of bins, without the ignore, illegal, and default bins
   * \param covered Returns the number of bins hit at least option.at_least times
   * \param total Returns the number of bins
   */
  void get_coverage_bins(int &covered, int &total) const
  {
    covered = 0;
    if (!has_custom_auto_bins()) {
      total = 1;
      for (auto cvp : cross_cvps)
        total *= cvp->size();
    }
    else {
      total = auto_bins_size;
      for (auto &bin : user_bins)
      {
        if (bin.bin_type != default_ || bin.size == 0)
          continue;
        total++;
        covered += (bin.hits >= option.at_least);
      }
    }
    for (auto &bin : get_cross_bins())
      covered += (bin.second >= option.at_least);
  }

};

/*!
 *  \class cross_base
 *  \brief non-template cross class
 *
 *  Interface to introspect the coverage model
 */
class cross_base : public cvp_base
{
public:

  /*! Get reference to cross options */
  virtual cross_option& option() = 0;

  /*! Get const reference to cross options */
  virtual const cross_option& option() const = 0;

  /*! Get cross bins storage */
  virtual const std::map<std::vector<size_t>, uint64_t>& get_cross_bins() const = 0;

  /*! Get crossed coverpoints storage */
  virtual const std::vector<cvp_base *>& get_cross_coverpoints() const = 0;

  /*! Get uncovered cross tuples formatted as string vectors */
  virtual std::vector<std::vector<std::string>> get_uncovered_cross_tuples_str(
      uint64_t max_tuples = 65536,
      bool* truncated = nullptr) const = 0;

  /*! Destructor */
  virtual ~cross_base() { }
};

/*!
 *  \class cvg_base_data_model fc_base.hpp
 *  \brief Base class for data of crosses
 *
 *  Basic interface to work with data of covergroups
 */
class cvg_base_data_model : public api_data_model
{

friend class covergroup;
friend class dynamic_covergroup;

public:

  /*! Pointer for type data of the covergroup */
  cvg_metadata* type_data;

  /*! Indicator for optional covergroup */
  bool enable = true;

  /*! Independent covergroup collection switch controlled by covergroup::start()/stop() */
  bool collect = true;

  /*! Optional covergroup sampling guard (iff) */
  std::function<bool()> iff_cond;
  std::string iff_cond_str;

  /*! Vector of pointers to coverpoints/crosses of the covergroup */
  std::vector<cvp_base_data_model*> cvps;

  /*! Instance specific options */
  cvg_option option;
  
  /*! File ID associated with the instance */
  unsigned int inst_file_id;

  /*! Aproximate line where this instance is instantiated */
  uint32_t inst_line;

  /*! Pointer to data of parent scope instance */
  scp_base_data_model* parent_scp = nullptr;


  /*! Propagate inherited options to all child coverpoints and crosses */
  void sync_options()
  {
    for (auto cvp : cvps) {
      cvp->sync_options_from_parent();
    }
  }

  /*! Add coverpoint/cross data to the covergroup */
  void add_cvp_data(cvp_base_data_model* cvp_data)
  {
    cvp_data->parent_cvg = this;
    cvps.push_back(cvp_data);
    cvp_data->sync_options_from_parent();
  }
  
  /*!
   * \brief Visitor pattern for coverage model introspection
   */
  virtual void accept_visitor(covVisitorBase& visitor)
  {
    sync_options();
    visitor.visit(*this);
  }

  /*! Destructor */
  virtual ~cvg_base_data_model()
  {
    for(auto cvp_it : cvps) {
      delete cvp_it;
    }
  }

};

/*!
 *  \class cvg_base fc_base.hpp
 *  \brief Base class for covergroups
 *
 *  Basic interface to work with covergroups
 */
class cvg_base : public api_base
{
public:

  /*! get coverpoint reference by name within this covergroup*/ 
  cvp_base& get_coverpoint(std::string name)
  {
    for (auto& cvp : cvps)
    {
      if(name == cvp->name()) {
        return *cvp;
      }
    }
    throw("No coverpoint " + name + " in " + this->name());
  }

  /*! get const coverpoint reference by name within this covergroup*/
  const cvp_base& get_coverpoint(const std::string& name) const
  {
    for (const auto* cvp : cvps)
    {
      if (name == cvp->name()) {
        return *cvp;
      }
    }
    throw("No coverpoint " + name + " in " + this->name());
  }

  /*! Get pointer to covergroup data */
  virtual cvg_base_data_model* get_cvg_data() = 0;

  /*! Get const pointer to covergroup data */
  virtual const cvg_base_data_model* get_cvg_data() const = 0;

  /*! Indictor if covergorup is enabled */
  virtual bool is_enabled() const = 0;

  /*! Evaluate covergroup iff guard (returns true when unset) */
  virtual bool eval_iff() const { return true; }

  /*! Whether a covergroup iff guard is configured */
  virtual bool has_iff() const { return false; }

  /*! Register coverpoint to covergroup */
  virtual void register_cvp(cvp_base* const cvp) {
    // Runtime check to make sure that all the coverpoints are different.
    // Normally, this should be the case, but if for whatever reason this
    // function is used multiple times with pointers to the the same coverpoint,
    // we make sure that the damage is minimal at least.
    if (std::find(std::begin(cvps), std::end(cvps), cvp) != std::end(cvps))
      throw ("Coverpoint already registered in this covergroup!");
    cvps.push_back(cvp);
  }

  /*! Disable covergroup */
  virtual void disable_cvg() = 0;

  /*! Retrieve sampling point, instance name and sampling point name */ 
  virtual cvp_metadata_t get_strings(cvp_base *cvp) = 0;

  /*! Array of associated coverpoints and crosses */
  std::vector<cvp_base *> cvps;
  
  /*! Instance specific options */
  virtual cvg_option& option() const = 0;

  /*! Type specific options */
  virtual cvg_type_option& type_option() = 0;

  /*! Const type specific options */
  virtual const cvg_type_option& type_option() const = 0;

  /*! Name of the covergroup type */
  virtual std::string& type_name() = 0; 

  /*! Const name of the covergroup type */
  virtual const std::string& type_name() const = 0; 

  /*! File ID associated with where this type is declared */
  virtual unsigned int& file_id() = 0;

  /*! Aproximate line where this type is declared */
  virtual uint32_t& line() = 0;

  /*! File ID associated with the instance */
  virtual unsigned int& inst_file_id() = 0;

  /*! Aproximate line where this instance is instantiated */
  virtual uint32_t& inst_line() = 0;

  /*!
   * \brief pointer to parent scope that contains this covergroups. Note can be a nullptr
   * since covergroups are not required to be contained within scopes
   */
  fc4sc::scp_base* parent_scp = nullptr;

};

/*!
 *  \class scope_factory_base fc_base.hpp
 *  \brief Base class for scopes
 *
 *  Basic interface to work with dynamic_scope_factory
 */
class scope_factory_base {

public:

  /*! Register covergroup type to factory */
  virtual void register_sub_cvg_type(dynamic_covergroup_factory* cvg) = 0;

  /*! Get the type name this factory represents */
  virtual std::string get_scp_type_name() = 0;

};

/*!
 *  \class cvg_base_data_model fc_base.hpp
 *  \brief Base class for data of crosses
 *
 *  Basic interface to work with data of covergroups
 */
class scp_base_data_model {
public:


  /*! Valid ptr for garbage collection */
  std::shared_ptr<bool> valid = std::make_shared<bool>(true);

  /*! Counter for coverage objects without names instanced in this scope*/
  int anonymous_count = 0;

  /*! Scope instance name */
  std::string name;

  /*! File ID associated with the instance */
  unsigned int inst_file_id;

  /*! Aproximate line where this instance is instantiated */
  uint32_t inst_line;

  /*! Scope instance id */
  unsigned int instance_id;

  /*! Pointer to parent scope data */
  scp_base_data_model* parent_scp = nullptr;

  /*! Pointer to context this scope belongs */
  global* cntxt = nullptr;

  /*! Pointer to type data of this scope */
  scp_metadata* type_data;

  /*! Map from instance name to scope data pointer for sub scopes instanced in this scope */
  std::unordered_map<std::string,scp_base_data_model*> child_scp_insts;

  /*! Map from type name to Vector of scope data pointer for sub scopes instanced in this scope*/
  std::unordered_map<std::string,std::vector<scp_base_data_model*>> child_scps;

  /*! Map from instance name to covergroup data pointer for covergroups instanced in this scope */
  std::unordered_map<std::string,cvg_base_data_model*> cvg_insts;

  /*! Map from type name to Vector of covergroup data pointer for covergroups instanced in this scope */
  std::unordered_map<std::string,std::vector<cvg_base_data_model*>> cvgs;

  /*! Add covergroup instance to this scope */
  virtual void add_cvg_data(cvg_base_data_model* cvg_data,std::string cvg_type_name, std::string scp_type_name, unsigned int cvg_file_id, uint32_t cvg_line) = 0;

  /*! Add scope instance to this scope */
  virtual void add_scp_data(scp_base_data_model* scp_data, std::string scp_type_name, unsigned int scp_file_id, uint32_t scp_line) = 0;

  /*! Checks for instances names already being used in this scope instance */
  void name_check(std::string& name, std::string type_name)
  {
    if (name.empty())
    {
      std::size_t found = type_name.find_last_of(':');    
      if(found == std::string::npos) {
	name = type_name + "_" + std::to_string(anonymous_count++);
      }
      else {
        name = type_name.substr(found+1) + "_" + std::to_string(anonymous_count++);
      }
    }
    if(cvg_insts.count(name) > 0 || child_scp_insts.count(name) > 0)
    {
      std::cerr << name << " scope name already in use\n";
      throw;
    }
  }

  /*!
   * \brief Visitor pattern for coverage model introspection
   */
  virtual void accept_visitor(covVisitorBase& visitor)
  {
    visitor.visit(*this);
  }

  /*! Destructor */
  virtual ~scp_base_data_model() { }

};

/*!
 *  \class scp_base fc_base.hpp
 *  \brief Base class for scopes
 *
 *  Basic interface to work with scopes
 */
class scp_base {
public:

  /*! Register covergroup to this scope */
  void register_cvg(cvg_base* cvg)
  {
    cvg->parent_scp = this;
    cvg_insts[cvg->name()] = cvg;
  }

  /*! Register scope instance to this scope */
  void register_child_scope(scp_base* const scp)
  {
    child_scp_insts[scp->name()] = scp;
  }

  /*! Get sub/child scope instance by name */
  scp_base& get_child(std::string name)
  {
    if(child_scp_insts.count(name) > 0) {
      return *(child_scp_insts[name]);
    }
    throw("No child scope " + name + " in " + this->name());
  }

  /*! Get covergroup instance by name */
  cvg_base& get_covergroup(std::string name)
  {
    if(this->cvg_insts.count(name) > 0) {
      return *(this->cvg_insts[name]);
    }
    throw("No covergroup " + name + " in " + this->name());
  }


  /*! Get pointer to scope's data */
  virtual scp_base_data_model* get_scp_data() = 0;
  
  /*! Name of scope instance */
  virtual std::string& name() = 0;

  /*! Name of the scope type */
  virtual std::string type_name() = 0; 

  /*! File ID associated with where this type is declared */
  virtual unsigned int& file_id() = 0;

  /*! Aproximate line where this type is declared */
  virtual uint32_t& line() = 0;

  /*! File ID associated with the instance */
  virtual unsigned int& inst_file_id() = 0;

  /*! Aproximate line where this instance is instantiated */
  virtual uint32_t& inst_line() = 0;

  /*! Instance ID for the instance */
  virtual unsigned int& instance_id() = 0;

  /*! Pointer to parent instance */
  scp_base* parent_scp = nullptr;

  /*! Map from instance name to child scope instance */
  std::unordered_map<std::string,scp_base*> child_scp_insts;

  /*! Map from instance name to covergroup instance */
  std::unordered_map<std::string,cvg_base*> cvg_insts;

  /*! Destructor */
  virtual ~scp_base() { }

};

/*!
 *  \class cvg_metadata fc_base.hpp
 *  \brief Representation for covergroup type
 *
 *  Covergroup type data which holds the type name and type specific data of a covergroup
 */
struct cvg_metadata
{

  /*! Covergroup type options */
  cvg_type_option type_option;

  /*! Covergroup type name */
  std::string type_name;

  /*! Scope type name this covergroup is defined within */
  std::string scp_type_name;

  /*! File id where this covergroup is defined */
  unsigned int file_id;

  /*! Line in file where this covergroup is defined */
  uint32_t line;

  /*! Vector of covergroup instances of this covergroup type */
  std::vector<cvg_base_data_model*> cvg_insts;

  /*! Type options of the coverpoints and crosses, by their names */
  std::map<std::string, cvp_type_option> cvp_type_options;

  /*! Snapshot-diff tracking for distribute_first inheritance */
  std::map<std::string, bool> synced_cvp_distribute_first_;
  std::map<std::string, bool> cvp_distribute_first_overridden_;

  void sync_cvp_type_option(const std::string& cvp_name)
  {
    cvp_type_option& cvp_opt = cvp_type_options[cvp_name];
    bool& synced = synced_cvp_distribute_first_[cvp_name];
    bool& overridden = cvp_distribute_first_overridden_[cvp_name];
    if (cvp_opt.distribute_first != synced)
      overridden = true;
    if (!overridden) {
      cvp_opt.distribute_first = type_option.distribute_first;
      synced = cvp_opt.distribute_first;
    }
  }

  /*! Destructor */
  virtual ~cvg_metadata()
  {
    for(auto cvg_it : cvg_insts)
    {
      delete cvg_it;
    }
  }

};

inline cvp_type_option& cvp_base_data_model::type_option()
{
  if (parent_cvg == nullptr || parent_cvg->type_data == nullptr)
    return own_type_option;
  parent_cvg->type_data->sync_cvp_type_option(name);
  return parent_cvg->type_data->cvp_type_options[name];
}

inline const cvp_type_option& cvp_base::type_option() const
{
  const cvp_base_data_model* d = get_data();
  if (d != nullptr && d->parent_cvg != nullptr && d->parent_cvg->type_data != nullptr) {
    auto it = d->parent_cvg->type_data->cvp_type_options.find(d->name);
    if (it != d->parent_cvg->type_data->cvp_type_options.end())
      return it->second;
  }
  static const cvp_type_option default_opt;
  return (d != nullptr) ? d->own_type_option : default_opt;
}

inline void coverpoint_base_data_model::sync_options_from_parent()
{
  if (option.at_least != synced_option_.at_least)
    at_least_overridden_ = true;
  if (option.auto_bin_max != synced_option_.auto_bin_max)
    auto_bin_max_overridden_ = true;
  if (option.detect_overlap != synced_option_.detect_overlap)
    detect_overlap_overridden_ = true;

  if (parent_cvg != nullptr) {
    if (!at_least_overridden_) {
      option.at_least = parent_cvg->option.at_least;
      synced_option_.at_least = option.at_least;
    }
    if (!auto_bin_max_overridden_) {
      option.auto_bin_max = parent_cvg->option.auto_bin_max;
      synced_option_.auto_bin_max = option.auto_bin_max;
    }
    if (!detect_overlap_overridden_) {
      option.detect_overlap = parent_cvg->option.detect_overlap;
      synced_option_.detect_overlap = option.detect_overlap;
    }
    if (parent_cvg->type_data != nullptr) {
      parent_cvg->type_data->sync_cvp_type_option(name);
    }
  }
}

inline void cross_base_data_model::sync_options_from_parent()
{
  if (option.at_least != synced_option_.at_least)
    at_least_overridden_ = true;
  if (option.cross_num_print_missing != synced_option_.cross_num_print_missing)
    cross_num_print_missing_overridden_ = true;

  if (parent_cvg != nullptr) {
    if (!at_least_overridden_) {
      option.at_least = parent_cvg->option.at_least;
      synced_option_.at_least = option.at_least;
    }
    if (!cross_num_print_missing_overridden_) {
      option.cross_num_print_missing = parent_cvg->option.cross_num_print_missing;
      synced_option_.cross_num_print_missing = option.cross_num_print_missing;
    }
  }
}

/*!
 * \brief Whether a covergroup type gives the options their SystemVerilog
 * meaning, see cvg_type_option::strict_sv_options
 */
inline bool strict_sv_options(const cvg_metadata* type_data)
{
  return type_data != nullptr && type_data->type_option.strict_sv_options;
}

/*!
 * \brief Whether get_inst_coverage() of a covergroup instance, and of its
 * coverpoints and crosses, returns the type coverage: as in SystemVerilog,
 * when merge_instances is set and option.get_inst_coverage is not
 * \param cvg The covergroup instance, or nullptr for a coverpoint or cross
 * which is in no covergroup
 */
inline bool inst_coverage_is_type_coverage(const cvg_base_data_model* cvg)
{
  return cvg != nullptr && strict_sv_options(cvg->type_data) &&
         cvg->type_data->type_option.merge_instances && !cvg->option.get_inst_coverage;
}

/*!
 *  \class scp_metadata fc_base.hpp
 *  \brief Representation for scope type
 *
 *  Scope type data which holds the type name and type specific data of a scope
 */
struct scp_metadata 
{

  /*! Name of the scope type */
  std::string type_name;
 
  /*! File ID associated with where this type is declared */
  unsigned int file_id;

  /*! Aproximate line where this type is declared */
  uint32_t line;

  /*! Map from covergroup type name to covergroup type */
  std::unordered_map<std::string,cvg_metadata*> cvg_type_table;

  /*! Vector of scope instances of this scope type */
  std::vector<scp_base_data_model*> scp_insts;

  /*! Destructor */
  virtual ~scp_metadata()
  {
    for(auto cvg_it : cvg_type_table)
    {
      delete cvg_it.second;
    }
    for(auto scp_it : scp_insts)
    {
      delete scp_it;
    }
  }

};



class illegal_bin_sample_exception : public std::exception {
private:
  std::string strconcat(const std::string& last) const { return last; }
  template <typename... Args>
  std::string strconcat(const std::string& head, Args... args) const {
    return head + strconcat(args...);
  }

  std::string value_hit;
  std::string bin_name;
  std::string cvp_name;
  std::string cvg_name;
  std::string message;

  void update_message() {
    message = strconcat("Illegal sample in [", cvg_name, "/", cvp_name, "/", bin_name, "] on value [", value_hit, "]!");
  }
public:
  void update_bin_info(const std::string& bin_name, const std::string& value) {
    this->bin_name = bin_name;
    this->value_hit = value;
    update_message();
  }
  void update_cvp_info(const std::string& cvp_name) {
    this->cvp_name = cvp_name;
    update_message();
  }
  void update_cvg_info(const std::string& cvg_name) {
    this->cvg_name = cvg_name;
    update_message();
  }

  const char * what () const throw () {
    return message.c_str();
  }
};

} // namespace fc4sc

#endif /* FC4SC_BASE_HPP */
