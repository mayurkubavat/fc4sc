/******************************************************************************

   Copyright 2003-2018 AMIQ Consulting s.r.l.

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

#ifndef FC4SC_BINSONF_HPP
#define FC4SC_BINSONF_HPP

#include <algorithm>
#include <array>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <vector>

#include "fc4sc_intervals.hpp"
#include "fc4sc_coverpoint.hpp"

namespace fc4sc {

template <typename... Args>
using CrossValType = std::tuple<Args...>;

template <typename... Args>
using CrossQueueType = std::vector<std::tuple<Args...>>;

namespace detail {

template <typename T>
struct is_cross_tuple_vector : std::false_type {};

template <typename... Args>
struct is_cross_tuple_vector<std::vector<std::tuple<Args...>>> : std::true_type {};

template <typename Fn>
struct is_cross_queue_generator {
  template <typename G>
  static auto test(int)
      -> typename std::enable_if<is_cross_tuple_vector<decay_t<decltype(std::declval<G>()())>>::value,
                                 std::true_type>::type;
  template <typename>
  static std::false_type test(...);
  static constexpr bool value = decltype(test<Fn>(0))::value;
};

template <size_t I, size_t N, typename Tuple>
struct cross_tuple_cmp_impl {
  static bool less(const Tuple& a, const Tuple& b) {
    typedef decay_t<typename std::tuple_element<I, Tuple>::type> elem_t;
    typedef fc4sc_type_traits<elem_t> traits;
    if (!traits::equal(std::get<I>(a), std::get<I>(b)))
      return traits::less(std::get<I>(a), std::get<I>(b));
    return cross_tuple_cmp_impl<I + 1, N, Tuple>::less(a, b);
  }
  static bool equal(const Tuple& a, const Tuple& b) {
    typedef decay_t<typename std::tuple_element<I, Tuple>::type> elem_t;
    typedef fc4sc_type_traits<elem_t> traits;
    if (!traits::equal(std::get<I>(a), std::get<I>(b)))
      return false;
    return cross_tuple_cmp_impl<I + 1, N, Tuple>::equal(a, b);
  }
  static void validate(const Tuple& src, Tuple& dst) {
    typedef decay_t<typename std::tuple_element<I, Tuple>::type> elem_t;
    typedef fc4sc_type_traits<elem_t> traits;
    std::get<I>(dst) = traits::validate_point(std::get<I>(src));
    cross_tuple_cmp_impl<I + 1, N, Tuple>::validate(src, dst);
  }
  static void matching_bins(const Tuple& t,
                            const std::vector<cvp_base*>& cvps,
                            std::vector<std::vector<size_t>>& out) {
    typedef decay_t<typename std::tuple_element<I, Tuple>::type> elem_t;
    const auto* typed_cvp = dynamic_cast<const coverpoint<elem_t>*>(cvps[I]);
    if (typed_cvp == nullptr)
      throw std::invalid_argument("CrossQueueType coverpoint type mismatch");
    out[I] = typed_cvp->matching_bin_indices(std::get<I>(t));
    if (out[I].empty()) return;
    cross_tuple_cmp_impl<I + 1, N, Tuple>::matching_bins(t, cvps, out);
  }
  static void fill_elem_ptrs(const Tuple& t, const void** out) {
    out[I] = &std::get<I>(t);
    cross_tuple_cmp_impl<I + 1, N, Tuple>::fill_elem_ptrs(t, out);
  }
};

template <size_t N, typename Tuple>
struct cross_tuple_cmp_impl<N, N, Tuple> {
  static bool less(const Tuple&, const Tuple&) { return false; }
  static bool equal(const Tuple&, const Tuple&) { return true; }
  static void validate(const Tuple&, Tuple&) {}
  static void matching_bins(const Tuple&, const std::vector<cvp_base*>&,
                            std::vector<std::vector<size_t>>&) {}
  static void fill_elem_ptrs(const Tuple&, const void**) {}
};

template <typename... Args>
struct cross_tuple_less {
  bool operator()(const std::tuple<Args...>& a, const std::tuple<Args...>& b) const {
    return cross_tuple_cmp_impl<0, sizeof...(Args), std::tuple<Args...>>::less(a, b);
  }
};

template <typename... Args>
inline std::shared_ptr<cross_select_node> build_cross_queue_node(
    const std::vector<std::tuple<Args...>>& raw_q) {
  typedef std::tuple<Args...> tuple_t;
  std::vector<tuple_t> sorted_q;
  sorted_q.reserve(raw_q.size());
  for (const auto& raw_t : raw_q) {
    tuple_t validated;
    cross_tuple_cmp_impl<0, sizeof...(Args), tuple_t>::validate(raw_t, validated);
    sorted_q.push_back(std::move(validated));
  }
  std::sort(sorted_q.begin(), sorted_q.end(), cross_tuple_less<Args...>());
  sorted_q.erase(
      std::unique(sorted_q.begin(), sorted_q.end(), [](const tuple_t& a, const tuple_t& b) {
        return cross_tuple_cmp_impl<0, sizeof...(Args), tuple_t>::equal(a, b);
      }),
      sorted_q.end());

  auto node = std::make_shared<cross_select_node>();
  node->op = cross_select_node::queue_;
  node->tuple_type = &typeid(tuple_t);
  node->queue_contains = [sorted_q](const void* ptr) -> bool {
    const auto* tup = static_cast<const tuple_t*>(ptr);
    return std::binary_search(sorted_q.begin(), sorted_q.end(), *tup,
                              cross_tuple_less<Args...>());
  };
  node->queue_hit_products = [sorted_q](
      const std::vector<cvp_base*>& cvps,
      const std::function<bool(const void*)>& cross_with_pred,
      uint64_t& valid_tuple_count) -> std::vector<std::vector<size_t>> {
    valid_tuple_count = 0;
    std::vector<std::vector<size_t>> out;
    if (cvps.size() != sizeof...(Args))
      throw std::invalid_argument("CrossQueueType arity does not match cross coverpoints");
    for (const auto& t : sorted_q) {
      if (cross_with_pred && !cross_with_pred(&t))
        continue;
      std::vector<std::vector<size_t>> dims(sizeof...(Args));
      cross_tuple_cmp_impl<0, sizeof...(Args), tuple_t>::matching_bins(t, cvps, dims);
      bool ok = true;
      for (const auto& d : dims) {
        if (d.empty()) {
          ok = false;
          break;
        }
      }
      if (!ok) continue;
      ++valid_tuple_count;
      std::vector<size_t> pos(dims.size(), 0);
      while (true) {
        std::vector<size_t> prod(dims.size());
        for (size_t k = 0; k < dims.size(); ++k)
          prod[k] = dims[k][pos[k]];
        out.push_back(std::move(prod));
        size_t p = dims.size();
        while (p > 0 && ++pos[p - 1] == dims[p - 1].size()) {
          pos[p - 1] = 0;
          --p;
        }
        if (p == 0) break;
      }
    }
    return out;
  };
  node->queue_count_tuples = [sorted_q](
      const std::vector<cvp_base*>& cvps,
      const std::function<bool(const std::vector<size_t>&, const void*, const void* const*)>& accept_product) -> uint64_t {
    if (cvps.size() != sizeof...(Args))
      throw std::invalid_argument("CrossQueueType arity does not match cross coverpoints");
    uint64_t count = 0;
    std::array<const void*, sizeof...(Args)> elem_ptrs{};
    for (const auto& t : sorted_q) {
      std::vector<std::vector<size_t>> dims(sizeof...(Args));
      cross_tuple_cmp_impl<0, sizeof...(Args), tuple_t>::matching_bins(t, cvps, dims);
      bool ok = true;
      for (const auto& d : dims) {
        if (d.empty()) {
          ok = false;
          break;
        }
      }
      if (!ok) continue;
      cross_tuple_cmp_impl<0, sizeof...(Args), tuple_t>::fill_elem_ptrs(t, elem_ptrs.data());
      bool tuple_accepted = false;
      std::vector<size_t> pos(dims.size(), 0);
      while (true) {
        std::vector<size_t> prod(dims.size());
        for (size_t k = 0; k < dims.size(); ++k)
          prod[k] = dims[k][pos[k]];
        if (!accept_product || accept_product(prod, &t, elem_ptrs.data())) {
          tuple_accepted = true;
          break;
        }
        size_t p = dims.size();
        while (p > 0 && ++pos[p - 1] == dims[p - 1].size()) {
          pos[p - 1] = 0;
          --p;
        }
        if (p == 0) break;
      }
      if (tuple_accepted)
        ++count;
    }
    return count;
  };
  node->queue_for_each_tuple = [sorted_q](
      const std::vector<cvp_base*>& cvps,
      const std::function<bool(const std::vector<size_t>&, const void*, const void* const*)>& visitor) -> void {
    if (!visitor) return;
    if (cvps.size() != sizeof...(Args))
      throw std::invalid_argument("CrossQueueType arity does not match cross coverpoints");
    std::array<const void*, sizeof...(Args)> elem_ptrs{};
    for (const auto& t : sorted_q) {
      std::vector<std::vector<size_t>> dims(sizeof...(Args));
      cross_tuple_cmp_impl<0, sizeof...(Args), tuple_t>::matching_bins(t, cvps, dims);
      bool ok = true;
      for (const auto& d : dims) {
        if (d.empty()) {
          ok = false;
          break;
        }
      }
      if (!ok) continue;
      cross_tuple_cmp_impl<0, sizeof...(Args), tuple_t>::fill_elem_ptrs(t, elem_ptrs.data());
      std::vector<size_t> pos(dims.size(), 0);
      while (true) {
        std::vector<size_t> prod(dims.size());
        for (size_t k = 0; k < dims.size(); ++k)
          prod[k] = dims[k][pos[k]];
        if (!visitor(prod, &t, elem_ptrs.data()))
          return;
        size_t p = dims.size();
        while (p > 0 && ++pos[p - 1] == dims[p - 1].size()) {
          pos[p - 1] = 0;
          --p;
        }
        if (p == 0) break;
      }
    }
  };
  return node;
}

} // namespace detail

/*!
 * \brief Defines a class for select expressions of the bins of a cross
 *
 *   A select expression is made of binsof conditions combined with the
 * operators !, && and ||, as in SystemVerilog (IEEE 1800-2017, 19.6.1):
 * binsof(a) && !binsof(b).intersect(1). It selects products of the bins of
 * the crossed coverpoints, for a user defined bin of a cross. See
 * fc4sc::binsof and fc4sc::cross_bin.
 */
class select_expression
{
protected:

  select_expression() { }

  explicit select_expression(std::shared_ptr<const cross_select_node> node) : node(node) { }

  /*! Root of the expression */
  std::shared_ptr<const cross_select_node> node;

  friend class cross_bin;
  friend select_expression operator!(const select_expression& operand);
  friend select_expression operator&&(const select_expression& lhs, const select_expression& rhs);
  friend select_expression operator||(const select_expression& lhs, const select_expression& rhs);

  static select_expression combine(cross_select_node::op_t op, const select_expression& lhs,
                                   const select_expression* rhs)
  {
    auto node = std::make_shared<cross_select_node>();
    node->op = op;
    node->lhs = lhs.node;
    if (rhs != nullptr) node->rhs = rhs->node;
    return select_expression(node);
  }

public:
  template <typename... Args, typename F>
  select_expression with(F pred) const
  {
    auto wnode = std::make_shared<cross_select_node>();
    wnode->op = cross_select_node::with_;
    wnode->lhs = this->node;
    wnode->tuple_type = &typeid(std::tuple<Args...>);
    wnode->tuple_pred = detail::wrap_cross_predicate<Args...>(std::move(pred));
    return select_expression(wnode);
  }
};

/*! \brief Selects the products which the operand does not select */
inline select_expression operator!(const select_expression& operand)
{
  return select_expression::combine(cross_select_node::not_, operand, nullptr);
}

/*! \brief Selects the products which both operands select */
inline select_expression operator&&(const select_expression& lhs, const select_expression& rhs)
{
  return select_expression::combine(cross_select_node::and_, lhs, &rhs);
}

/*! \brief Selects the products which either operand selects */
inline select_expression operator||(const select_expression& lhs, const select_expression& rhs)
{
  return select_expression::combine(cross_select_node::or_, lhs, &rhs);
}

/*!
 * \brief Defines a class for the binsof conditions of select expressions
 * \tparam T Type of the values of the coverpoint
 *
 *   binsof<T>(&cvp) selects the products of a cross which have any bin of
 * the coverpoint cvp, and binsof<T>(&cvp, "b") the products which have
 * its bin b, as the SystemVerilog binsof(cvp) and binsof(cvp.b). If b is
 * a bin array, all its bins are selected; a single bin of the array is
 * named as "b[2]". The coverpoint may also be given by reference, and
 * with C++17 the type can be left out, as in binsof(&cvp).
 *
 *   intersect() keeps the selected bins which have a value in the values
 * it is given, as the SystemVerilog binsof(cvp) intersect {[0:3], 7},
 * which is written binsof<int>(&cvp).intersect(interval(0, 3), 7). It
 * takes values, intervals and vectors of them. The values of a transition
 * bin are the values of its sequences. As in SystemVerilog,
 * !binsof<int>(&cvp).intersect(7) selects the products whose bin of cvp
 * has no value 7.
 */
template <typename T>
class binsof : public select_expression
{
  /*! The condition: its coverpoint, bin and intersect values */
  coverpoint<T>* cvp = nullptr;
  std::string bin_name;
  bool has_values = false;
  std::vector<interval_t<T>> values;
  std::vector<std::function<bool(const void*)>> single_cvp_preds;

  void add_value(const T& value)
  {
    const T v = fc4sc_type_traits<T>::validate_point(value);
    values.push_back(interval_t<T>(v, v));
  }
  void add_value(const interval_t<T>& intv)
  {
    values.push_back(fc4sc_type_traits<T>::validate_interval(intv.first, intv.second));
  }
  void add_value(const std::vector<T>& vec) { for (const T& v : vec) add_value(v); }
  void add_value(const std::vector<interval_t<T>>& vec) { for (auto &v : vec) add_value(v); }

  void add_values() { }

  template <typename Head, typename... Tail>
  void add_values(const Head& head, const Tail&... tail)
  {
    add_value(head);
    add_values(tail...);
  }

  bool has_value(T low, T high) const
  {
    typedef fc4sc_type_traits<T> traits;
    for (auto &intv : values)
      if (traits::less_equal(intv.first, high) && traits::less_equal(low, intv.second))
        return true;
    return false;
  }

  /*! Returns which bins of the coverpoint this condition selects */
  std::vector<bool> select_bins(const cvp_base* target_cvp = nullptr) const
  {
    const coverpoint<T>* target =
        (target_cvp != nullptr) ? dynamic_cast<const coverpoint<T>*>(target_cvp) : cvp;
    if (target == nullptr) target = cvp;
    target->ensure_bins();
    std::vector<bool> selected(target->bins.size(), false);
    bool found = false;
    for (size_t i = 0; i < target->bins.size(); ++i)
    {
      bin_data_model<T>* data = target->bins[i].bin_data;
      const std::string& name = data->name;
      if (!bin_name.empty() && name != bin_name &&
          name.compare(0, bin_name.size() + 1, bin_name + "[") != 0)
        continue;
      found = true;
      if (!has_values) {
        selected[i] = true;
        continue;
      }
      for (auto &intv : data->intervals)
        selected[i] = selected[i] || has_value(intv.first, intv.second);
      auto trans_data = dynamic_cast<transition_bin_data_model<T>*>(data);
      if (trans_data != nullptr) {
        for (auto &seq : trans_data->sequences)
          for (const T& value : seq)
            selected[i] = selected[i] || has_value(value, value);
        for (auto &intv : trans_data->get_pattern_values())
          selected[i] = selected[i] || has_value(intv.first, intv.second);
      }
    }
    if (!found) {
      // as in SystemVerilog, the ignore and illegal bins of a coverpoint
      // are not in its crosses
      for (auto &bin : target->ignore_bins)
        if (bin.bin_data->name == bin_name)
          throw std::invalid_argument("binsof: " + bin_name + " is an ignore bin of coverpoint " +
                                      target->cvp_data->name + " and cannot be selected");
      for (auto &bin : target->illegal_bins)
        if (bin.bin_data->name == bin_name)
          throw std::invalid_argument("binsof: " + bin_name + " is an illegal bin of coverpoint " +
                                      target->cvp_data->name + " and cannot be selected");
      throw std::invalid_argument("binsof: no bin " + bin_name + " in coverpoint " + target->cvp_data->name);
    }
    return selected;
  }

  /*! Builds the node of this condition */
  void make_node()
  {
    auto node = std::make_shared<cross_select_node>();
    node->cvp = cvp;
    binsof<T> condition = *this;
    condition.node.reset();
    condition.single_cvp_preds.clear();
    node->select_bins = [condition](const cvp_base* target_cvp) {
      return condition.select_bins(target_cvp);
    };
    std::shared_ptr<const cross_select_node> cur = node;
    for (const auto& sp : single_cvp_preds) {
      auto wnode = std::make_shared<cross_select_node>();
      wnode->op = cross_select_node::with_;
      wnode->lhs = cur;
      wnode->cvp = cvp;
      wnode->single_cvp_pred = sp;
      cur = wnode;
    }
    this->node = cur;
  }

public:

  /*!
   * \brief Selects the bins of a coverpoint
   * \param cvp The coverpoint
   * \param bin_name Name of the bin to select, or empty for all the bins
   */
  explicit binsof(coverpoint<T>* cvp, const std::string& bin_name = "") : cvp(cvp), bin_name(bin_name)
  {
    if (cvp == nullptr)
      throw std::invalid_argument("binsof: no coverpoint");
    make_node();
  }

  /*!
   * \brief Selects the bins of a coverpoint
   * \param cvp The coverpoint
   * \param bin_name Name of the bin to select, or empty for all the bins
   */
  explicit binsof(coverpoint<T>& cvp, const std::string& bin_name = "") : binsof(&cvp, bin_name) { }

  /*!
   * \brief Keeps the selected bins which have a value in the given ones
   * \param args Values, intervals, or vectors of values or intervals
   * \returns The new condition
   */
  template <typename... Args>
  binsof<T> intersect(const Args&... args) const
  {
    binsof<T> condition = *this;
    condition.has_values = true;
    condition.add_values(args...);
    condition.make_node();
    return condition;
  }

  template <typename F>
  binsof<T> with(F pred) const
  {
    binsof<T> condition = *this;
    condition.single_cvp_preds.push_back([pred](const void* ptr) -> bool {
      return static_cast<bool>(pred(*static_cast<const T*>(ptr)));
    });
    condition.make_node();
    return condition;
  }
};

struct cross_iff_t {
  std::function<bool()> cond;
};

inline cross_iff_t cross_iff(bool constant_cond) {
  return cross_iff_t{[constant_cond]() -> bool { return constant_cond; }};
}

inline cross_iff_t cross_iff(const bool* flag_ptr) {
  if (flag_ptr == nullptr)
    throw std::invalid_argument("cross_iff: null bool pointer");
  return cross_iff_t{[flag_ptr]() -> bool { return *flag_ptr; }};
}

template <typename F,
          typename std::enable_if<
              !std::is_same<detail::decay_t<F>, bool>::value &&
              !std::is_convertible<detail::decay_t<F>, const bool*>::value &&
              !std::is_same<detail::decay_t<F>, std::nullptr_t>::value,
              int>::type = 0>
inline cross_iff_t cross_iff(F&& cond) {
  return cross_iff_t{std::function<bool()>(std::forward<F>(cond))};
}

template <typename F>
struct cross_with_t {
  F pred;
};

template <typename F>
inline cross_with_t<detail::decay_t<F>> cross_with(F pred) {
  return cross_with_t<detail::decay_t<F>>{std::move(pred)};
}

/*!
 * \brief Defines a class for the user defined bins of crosses
 *
 *   A user defined bin of a cross holds the products of the bins of the
 * crossed coverpoints which its select expression selects. The
 * SystemVerilog
 *
 *   cross a, b {
 *     bins lo_one = binsof(a.lo) && binsof(b) intersect {1};
 *     ignore_bins ign = binsof(a.hi);
 *     illegal_bins bad = binsof(a.lo) && binsof(b.zero);
 *   }
 *
 * is written
 *
 *   cross<int, int>(this, "a_x_b", &a, &b,
 *     cross_bin("lo_one", binsof<int>(&a, "lo") && binsof<int>(&b).intersect(1)),
 *     ignore_cross_bin("ign", binsof<int>(&a, "hi")),
 *     illegal_cross_bin("bad", binsof<int>(&a, "lo") && binsof<int>(&b, "zero")))
 *
 *   As in SystemVerilog (IEEE 1800-2017, 19.6), the cross has a bin
 * created automatically for each product of bins which is in no user
 * defined bin, ignore bin or illegal bin. The products of ignore bins and
 * illegal bins are in no other bin, and are not counted in the coverage.
 * A sample of an illegal product throws an illegal_bin_sample_exception,
 * as an illegal bin of a coverpoint does. A user defined bin is hit once
 * by a sample which hits any of its products, and a bin which has no
 * products is not counted in the coverage. The UCIS XML file has a
 * crossBin for each user defined bin which has products, with its name,
 * and the index -1 for each coverpoint, as it has no single product of
 * bins. The ignore and illegal bins are not written, as they are not
 * counted in the coverage, and readers of UCIS files may not look at the
 * type of a crossBin.
 */
class cross_bin
{
public:

  /*! Data of the bin */
  cross_bin_data_model data;

  /*!
   * \brief Creates a cross bin matching all products (intended for .with(pred))
   * \param name Name of the bin
   */
  explicit cross_bin(const std::string& name)
  {
    data.name = name;
    data.bin_type = default_;
    auto all_node = std::make_shared<cross_select_node>();
    all_node->op = cross_select_node::all_;
    data.select = all_node;
  }

  /*!
   * \brief Takes the bin name and its select expression
   * \param name Name of the bin
   * \param select Select expression of the bin
   */
  cross_bin(const std::string& name, const select_expression& select)
  {
    data.name = name;
    data.bin_type = default_;
    data.select = select.node;
  }

  /*!
   * \brief Creates a cross bin from a CrossQueueType value-tuple vector
   */
  template <typename... Args>
  cross_bin(const std::string& name, const std::vector<std::tuple<Args...>>& queue)
  {
    data.name = name;
    data.bin_type = default_;
    data.select = detail::build_cross_queue_node(queue);
  }

  /*!
   * \brief Creates a cross bin from a zero-argument generator returning CrossQueueType
   */
  template <typename Fn,
            typename std::enable_if<detail::is_cross_queue_generator<detail::decay_t<Fn>>::value, int>::type = 0>
  cross_bin(const std::string& name, Fn&& gen)
    : cross_bin(name, gen())
  { }

  template <typename F, typename Traits = detail::callable_traits<F>>
  cross_bin& with(F pred)
  {
    auto wnode = std::make_shared<cross_select_node>();
    wnode->op = cross_select_node::with_;
    wnode->lhs = data.select;
    wnode->tuple_type = Traits::type();
    wnode->tuple_pred = Traits::wrap(std::move(pred));
    data.select = wnode;
    return *this;
  }

  template <typename FirstArg, typename... RestArgs, typename F>
  cross_bin& with(F pred)
  {
    auto wnode = std::make_shared<cross_select_node>();
    wnode->op = cross_select_node::with_;
    wnode->lhs = data.select;
    wnode->tuple_type = &typeid(std::tuple<FirstArg, RestArgs...>);
    wnode->tuple_pred = detail::wrap_cross_predicate<FirstArg, RestArgs...>(std::move(pred));
    data.select = wnode;
    return *this;
  }

private:
  void add_iff_cond(std::function<bool()> next)
  {
    if (!next) return;
    if (data.iff_cond) {
      auto prev = data.iff_cond;
      data.iff_cond = [prev, next]() -> bool { return prev() && next(); };
    } else {
      data.iff_cond = std::move(next);
    }
  }

public:

  cross_bin& iff(bool constant_cond)
  {
    add_iff_cond([constant_cond]() -> bool { return constant_cond; });
    return *this;
  }

  cross_bin& iff(const bool* flag_ptr)
  {
    if (flag_ptr == nullptr)
      throw std::invalid_argument("cross_bin::iff: null bool pointer");
    add_iff_cond([flag_ptr]() -> bool { return *flag_ptr; });
    return *this;
  }

  template <typename F,
            typename std::enable_if<
                !std::is_same<detail::decay_t<F>, bool>::value &&
                !std::is_convertible<detail::decay_t<F>, const bool*>::value &&
                !std::is_same<detail::decay_t<F>, std::nullptr_t>::value,
                int>::type = 0>
  cross_bin& iff(F&& cond)
  {
    add_iff_cond(std::function<bool()>(std::forward<F>(cond)));
    return *this;
  }

  virtual ~cross_bin() = default;
};

/*!
 * \brief Defines a class for ignore bins of crosses, as the SystemVerilog
 * "ignore_bins b = binsof(a.lo);". See fc4sc::cross_bin.
 */
class ignore_cross_bin : public cross_bin
{
public:
  explicit ignore_cross_bin(const std::string& name) : cross_bin(name)
  {
    data.bin_type = ignore_;
  }
  ignore_cross_bin(const std::string& name, const select_expression& select) : cross_bin(name, select)
  {
    data.bin_type = ignore_;
  }
  template <typename... Args>
  ignore_cross_bin(const std::string& name, const std::vector<std::tuple<Args...>>& queue)
    : cross_bin(name, queue)
  {
    data.bin_type = ignore_;
  }
  template <typename Fn,
            typename std::enable_if<detail::is_cross_queue_generator<detail::decay_t<Fn>>::value, int>::type = 0>
  ignore_cross_bin(const std::string& name, Fn&& gen)
    : cross_bin(name, std::forward<Fn>(gen))
  {
    data.bin_type = ignore_;
  }
};

/*!
 * \brief Defines a class for illegal bins of crosses, as the SystemVerilog
 * "illegal_bins b = binsof(a.lo);". See fc4sc::cross_bin.
 */
class illegal_cross_bin : public cross_bin
{
public:
  explicit illegal_cross_bin(const std::string& name) : cross_bin(name)
  {
    data.bin_type = illegal_;
  }
  illegal_cross_bin(const std::string& name, const select_expression& select) : cross_bin(name, select)
  {
    data.bin_type = illegal_;
  }
  template <typename... Args>
  illegal_cross_bin(const std::string& name, const std::vector<std::tuple<Args...>>& queue)
    : cross_bin(name, queue)
  {
    data.bin_type = illegal_;
  }
  template <typename Fn,
            typename std::enable_if<detail::is_cross_queue_generator<detail::decay_t<Fn>>::value, int>::type = 0>
  illegal_cross_bin(const std::string& name, Fn&& gen)
    : cross_bin(name, std::forward<Fn>(gen))
  {
    data.bin_type = illegal_;
  }
};

/*!
 * \brief Defines a class for default bins of crosses, catching uncovered valid
 * cross combinations without counting in coverage.
 */
class default_cross_bin : public cross_bin
{
public:
  explicit default_cross_bin(const std::string& name) : cross_bin(name)
  {
    data.bin_type = default_bin_;
    data.select.reset();
  }

  template <typename... TArgs>
  cross_bin& with(TArgs...) = delete;
};

/*!
 * \brief Expands a CrossQueueType into an array of named cross bins name[0], name[1], ...
 */
class cross_bin_array
{
public:
  std::vector<cross_bin> bins;

  template <typename... Args>
  cross_bin_array(const std::string& name, const std::vector<std::tuple<Args...>>& queue)
  {
    bins.reserve(queue.size());
    for (size_t i = 0; i < queue.size(); ++i) {
      std::vector<std::tuple<Args...>> single(1, queue[i]);
      bins.emplace_back(name + "[" + std::to_string(i) + "]", single);
    }
  }

  template <typename Fn,
            typename std::enable_if<detail::is_cross_queue_generator<detail::decay_t<Fn>>::value, int>::type = 0>
  cross_bin_array(const std::string& name, Fn&& gen)
    : cross_bin_array(name, gen())
  { }
};

}	// namespace fc4sc

#endif
