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
 \file cross.hpp
 \brief Helper functions and interfaces

 This file contains helper functions and the base classes
 for bins, coverpoints and covergroups.
 */

#ifndef FC4SC_CROSS_HPP
#define FC4SC_CROSS_HPP

#include <array>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include "fc4sc_base.hpp"
#include "fc4sc_bin.hpp"
#include "fc4sc_coverpoint.hpp"
#include "fc4sc_binsof.hpp"

namespace fc4sc
{

/*!
 * \brief Maximum number of products of bins of a cross which has user
 * defined bins, ignore bins or illegal bins
 */
static const uint64_t cross_max_products = 1 << 24;

/*!
 * \brief Maximum number of value tuples enumerated per cross bin product
 * when evaluating cross with(predicate) filters
 */
static const uint64_t cross_with_max_tuples = 65536;

/*!
 *  \class crs_data_model fc_base.hpp
 *  \brief class for storing coverage data of crosses
 *
 *  Data storage for crosses
 */
class cross_data_model : public cross_base_data_model {
public:

  /*! Hit cross bins storage */
  std::map<std::vector<size_t>, uint64_t> bins;

  /*! Get cross bins storage */
  virtual const std::map<std::vector<size_t>, uint64_t>& get_cross_bins() const
  {
    return bins;
  }

  /*! Adds hits to an automatically created bin */
  virtual void add_cross_bin_hits(const std::vector<size_t>& product, uint64_t count)
  {
    // the map holds the bins which are hit
    if (count == 0) return;
    uint64_t& hits = bins[product];
    hits = (hits > std::numeric_limits<uint64_t>::max() - count) ? std::numeric_limits<uint64_t>::max()
                                                                  : hits + count;
  }

  uint64_t size() const
  {
    if (owner_ != nullptr && !in_ensure_bins_)
      owner_->ensure_bins();
    uint64_t total = 1;
    for (auto& cvp : cross_cvps) 
    {
      total *= cvp->size();
    }
    return total;
  }

  void accept_visitor(covVisitorBase& visitor)
  {
    sync_options_from_parent();
    if (owner_ != nullptr && !in_ensure_bins_)
      owner_->ensure_bins();
    visitor.visit(*this);
  }

};

/*!
 * \brief Defines a class for crosses
 * \tparam Args Type of coverpoints being crossed
 */
template <typename... Args>
class cross : public cross_base
{
public:
  typedef std::tuple<Args...> CrossValType;
  typedef std::vector<CrossValType> CrossQueueType;

private:
  friend class dynamic_covergroup_factory;

  /*! Pointer to this object's data */
  cross_data_model* crs_data = new cross_data_model;

  /*! For ensuring the object's data has not been deallocated */
  std::weak_ptr<bool> valid_data = crs_data->valid;

  /*! Sampling switch */
  bool collect = true;

  /*! Total number of bins in this cross */
  uint64_t total_coverpoints = 0;

  mutable bool has_sampled_ = false;
  mutable std::vector<uint64_t> cvp_epochs_;

  /*! Typed pointers to crossed coverpoints for sampled-value extraction */
  std::tuple<coverpoint<Args>*...> typed_cvps_{};

  /*! Parent covergroup pointer (used to update cvps on move) */
  cvg_base* parent_cvg_ptr_ = nullptr;

  /*! Optional cross-level value-tuple predicate from .with(...) / cross_with(...) */
  std::function<bool(const void*)> cross_with_pred_;

  /*! Optional cross-level sampling guard from .iff(...) / cross_iff(...) */
  std::function<bool()> iff_cond_;

  template <size_t... I>
  void bind_typed_cvps_impl(detail::index_seq<I...>)
  {
    (void)std::initializer_list<int>{
        ((std::get<I>(typed_cvps_) =
              dynamic_cast<coverpoint<typename std::tuple_element<I, std::tuple<Args...>>::type>*>(
                  cvps_vec[I])),
         0)...};
  }

  void bind_typed_cvps_from_vec()
  {
    if (cvps_vec.size() == sizeof...(Args))
      bind_typed_cvps_impl(detail::make_index_seq<sizeof...(Args)>());
  }

  template <size_t I>
  typename std::enable_if<I == sizeof...(Args), bool>::type
  extract_sampled_tuple_impl(std::tuple<Args...>&,
                             std::array<const void*, sizeof...(Args)>&) const
  {
    return true;
  }

  template <size_t I>
  typename std::enable_if<(I < sizeof...(Args)), bool>::type
  extract_sampled_tuple_impl(std::tuple<Args...>& out,
                             std::array<const void*, sizeof...(Args)>& elem_ptrs) const
  {
    auto* cvp = std::get<I>(typed_cvps_);
    if (cvp == nullptr || !cvp->get_last_sampled_value(std::get<I>(out)))
      return false;
    elem_ptrs[I] = &std::get<I>(out);
    return extract_sampled_tuple_impl<I + 1>(out, elem_ptrs);
  }

  template <size_t I>
  typename std::enable_if<I == sizeof...(Args), bool>::type
  enumerate_dim_impl(const std::vector<size_t>&, uint64_t, uint64_t,
                     std::tuple<std::vector<Args>...>&) const
  {
    return true;
  }

  template <size_t I>
  typename std::enable_if<(I < sizeof...(Args)), bool>::type
  enumerate_dim_impl(const std::vector<size_t>& product, uint64_t max_tuples,
                     uint64_t running_count,
                     std::tuple<std::vector<Args>...>& dim_vals) const
  {
    auto* cvp = std::get<I>(typed_cvps_);
    if (cvp == nullptr) return false;
    auto& vals = std::get<I>(dim_vals);
    if (!cvp->try_enumerate_bin_values(product[I], max_tuples, vals) || vals.empty())
      return false;
    const uint64_t sz = static_cast<uint64_t>(vals.size());
    if (running_count > max_tuples / sz)
      return false;
    return enumerate_dim_impl<I + 1>(product, max_tuples, running_count * sz, dim_vals);
  }

  template <size_t I>
  typename std::enable_if<I == sizeof...(Args), void>::type
  collect_tuples_impl(const std::tuple<std::vector<Args>...>&,
                      std::tuple<Args...>& cur,
                      std::vector<std::tuple<Args...>>& out) const
  {
    out.push_back(cur);
  }

  template <size_t I>
  typename std::enable_if<(I < sizeof...(Args)), void>::type
  collect_tuples_impl(const std::tuple<std::vector<Args>...>& dim_vals,
                      std::tuple<Args...>& cur,
                      std::vector<std::tuple<Args...>>& out) const
  {
    for (const auto& v : std::get<I>(dim_vals)) {
      std::get<I>(cur) = v;
      collect_tuples_impl<I + 1>(dim_vals, cur, out);
    }
  }

  template <size_t I>
  typename std::enable_if<I == sizeof...(Args), void>::type
  fill_elem_ptrs_impl(const std::tuple<Args...>&,
                      std::array<const void*, sizeof...(Args)>&) const {}

  template <size_t I>
  typename std::enable_if<(I < sizeof...(Args)), void>::type
  fill_elem_ptrs_impl(const std::tuple<Args...>& tup,
                      std::array<const void*, sizeof...(Args)>& ptrs) const
  {
    ptrs[I] = &std::get<I>(tup);
    fill_elem_ptrs_impl<I + 1>(tup, ptrs);
  }

  bool try_enumerate_product(const std::vector<size_t>& product,
                             uint64_t max_tuples,
                             std::vector<std::tuple<Args...>>& out) const
  {
    out.clear();
    if (product.size() != sizeof...(Args)) return false;
    std::tuple<std::vector<Args>...> dim_vals;
    if (!enumerate_dim_impl<0>(product, max_tuples, 1, dim_vals))
      return false;
    std::tuple<Args...> cur;
    collect_tuples_impl<0>(dim_vals, cur, out);
    return true;
  }

  /*!
   *  \brief Create a dynamic copy of this cross with same type definition
   */
  cvp_base* create_instance(cvg_base* cvg_inst) override
  {
    cross<Args...>* crs = new cross<Args...>;
    crs->total_coverpoints = this->total_coverpoints;
    crs->crs_data->name = this->crs_data->name;
    crs->crs_data->option = this->option();
    crs->crs_data->own_type_option = this->crs_data->own_type_option;
    crs->cross_with_pred_ = this->cross_with_pred_;
    crs->iff_cond_ = this->iff_cond_;
    for (auto cvp : this->cvps_vec)
    {
      crs->cvps_vec.push_back(&(cvg_inst->get_coverpoint(cvp->name())));
      crs->crs_data->cross_cvps.push_back(cvg_inst->get_coverpoint(cvp->name()).get_data());
    }
    crs->bind_typed_cvps_from_vec();
    crs->crs_data->user_bins = this->crs_data->user_bins;
    crs->crs_data->auto_bins_size = this->crs_data->auto_bins_size;
    for (auto &bin : crs->crs_data->user_bins) {
      bin.hits = 0;
      if (bin.select)
        bin.select = bin.select->resolve(crs->cvps_vec, &this->cvps_vec, &crs->cross_with_pred_);
    }
    crs->build_cross_bins();
    return crs;
  }

  /*!
   * \brief Moves to the next product of bins, the last coverpoint counting
   * fastest
   * \param product The index of a bin of each coverpoint
   * \param sizes The number of bins of each coverpoint
   * \returns false after the last product
   */
  static bool next_product(std::vector<size_t>& product, const std::vector<size_t>& sizes)
  {
    size_t i = product.size();
    while (i > 0 && ++product[i - 1] == sizes[i - 1]) {
      product[i - 1] = 0;
      --i;
    }
    return i != 0;
  }

  /*! Adds the user defined bins and cross modifiers given to a constructor */
  void add_cross_bins() { }

  template <typename... Tail>
  void add_cross_bins(const cross_bin& bin, const Tail&... tail)
  {
    crs_data->user_bins.push_back(bin.data);
    add_cross_bins(tail...);
  }

  template <typename... Tail>
  void add_cross_bins(const cross_bin_array& arr, const Tail&... tail)
  {
    for (const auto& b : arr.bins)
      crs_data->user_bins.push_back(b.data);
    add_cross_bins(tail...);
  }

  template <typename... Tail>
  void add_cross_bins(const cross_iff_t& iff_arg, const Tail&... tail)
  {
    if (iff_arg.cond) {
      if (iff_cond_) {
        auto prev = iff_cond_;
        auto next = iff_arg.cond;
        iff_cond_ = [prev, next]() -> bool { return prev() && next(); };
      } else {
        iff_cond_ = iff_arg.cond;
      }
    }
    add_cross_bins(tail...);
  }

  template <typename F, typename... Tail>
  void add_cross_bins(const cross_with_t<F>& w, const Tail&... tail)
  {
    auto wrapped = detail::wrap_cross_predicate<Args...>(w.pred);
    if (cross_with_pred_) {
      auto prev = cross_with_pred_;
      cross_with_pred_ = [prev, wrapped](const void* ptr) -> bool {
        return prev(ptr) && wrapped(ptr);
      };
    } else {
      cross_with_pred_ = wrapped;
    }
    add_cross_bins(tail...);
  }

  /*!
   * \brief Resolves the select expressions of the user defined bins and
   * counts their products of bins, and the automatically created bins
   */
  void build_cross_bins()
  {
    for (auto cvp : cvps_vec)
      cvp->ensure_bins();
    cvp_epochs_.clear();
    for (auto cvp : cvps_vec)
      cvp_epochs_.push_back(cvp->bins_epoch());

    auto& user_bins = crs_data->user_bins;
    crs_data->auto_bins_size = 0;
    crs_data->excluded_auto_products_.clear();
    crs_data->has_default_bin_ = false;
    for (auto &bin : user_bins) {
      bin.size = 0;
      if (bin.bin_type == default_bin_)
        crs_data->has_default_bin_ = true;
    }
    crs_data->has_bin_filter_ = !user_bins.empty() || static_cast<bool>(cross_with_pred_);
    if (!crs_data->has_bin_filter_) return;

    const std::type_info* expected_tuple_type = &typeid(std::tuple<Args...>);
    bool any_with_or_queue = static_cast<bool>(cross_with_pred_);
    std::vector<const cross_select_node*> queue_nodes(user_bins.size(), nullptr);
    std::vector<bool> is_queue_backed(user_bins.size(), false);
    std::vector<bool> bin_has_with(user_bins.size(), false);
    std::vector<bool> bin_has_queue(user_bins.size(), false);

    for (size_t i = 0; i < user_bins.size(); ++i) {
      auto& bin = user_bins[i];
      if (!bin.select) continue;
      bin.select->validate_tuple_type(expected_tuple_type);
      bin.select = bin.select->resolve(cvps_vec, nullptr, &cross_with_pred_);
      bin_has_with[i] = bin.select->has_with_node();
      bin_has_queue[i] = bin.select->has_queue_node();
      queue_nodes[i] = bin.select->get_underlying_queue_node();
      is_queue_backed[i] = (queue_nodes[i] != nullptr);
      if (bin_has_with[i] || bin_has_queue[i])
        any_with_or_queue = true;
    }

    std::vector<size_t> sizes;
    uint64_t products = 1;
    for (auto cvp : cvps_vec)
    {
      sizes.push_back(cvp->size());
      products *= cvp->size();
      if (products > cross_max_products)
        throw std::invalid_argument("Cross " + crs_data->name + " has too many products of bins for user defined bins");
    }
    if (products == 0) return;

    auto add_default_bin_size = [&]() {
      for (auto& b : user_bins)
        if (b.bin_type == default_bin_)
          b.size++;
    };

    if (!any_with_or_queue) {
      std::vector<size_t> product(sizes.size(), 0);
      std::vector<bool> matched(user_bins.size());
      do {
        bool any = false;
        bool excluded = false;
        for (size_t i = 0; i < user_bins.size(); ++i)
        {
          if (user_bins[i].bin_type == default_bin_ || !user_bins[i].select) {
            matched[i] = false;
            continue;
          }
          matched[i] = user_bins[i].select->matches(product);
          any = any || matched[i];
          excluded = excluded || (matched[i] && (user_bins[i].bin_type == ignore_ || user_bins[i].bin_type == illegal_));
        }
        for (size_t i = 0; i < user_bins.size(); ++i)
          if (matched[i] && (user_bins[i].bin_type != default_ || !excluded))
            user_bins[i].size++;
        if (crs_data->has_default_bin_) {
          if (!any) add_default_bin_size();
        } else {
          if (!any) crs_data->auto_bins_size++;
          else crs_data->excluded_auto_products_.insert(product);
        }
      } while (next_product(product, sizes));
      return;
    }

    std::vector<size_t> product(sizes.size(), 0);
    std::vector<std::tuple<Args...>> tuples;
    std::vector<bool> matched(user_bins.size());
    std::vector<bool> partial_matched(user_bins.size());
    std::array<const void*, sizeof...(Args)> elem_ptrs{};

    do {
      if (try_enumerate_product(product, cross_with_max_tuples, tuples)) {
        std::fill(matched.begin(), matched.end(), false);
        bool product_has_uncovered = false;

        for (const auto& tup : tuples) {
          if (cross_with_pred_ && !cross_with_pred_(&tup))
            continue;
          fill_elem_ptrs_impl<0>(tup, elem_ptrs);

          bool any = false;
          bool excluded = false;
          std::vector<bool> tmatch(user_bins.size(), false);
          for (size_t i = 0; i < user_bins.size(); ++i) {
            if (user_bins[i].bin_type == default_bin_ || !user_bins[i].select)
              continue;
            tmatch[i] = user_bins[i].select->matches_tuple(product, &tup, elem_ptrs.data());
            any = any || tmatch[i];
            excluded = excluded || (tmatch[i] && (user_bins[i].bin_type == ignore_ || user_bins[i].bin_type == illegal_));
          }
          for (size_t i = 0; i < user_bins.size(); ++i) {
            if (tmatch[i] && (user_bins[i].bin_type != default_ || !excluded))
              matched[i] = true;
          }
          if (!any)
            product_has_uncovered = true;
        }

        for (size_t i = 0; i < user_bins.size(); ++i) {
          if (!is_queue_backed[i] && matched[i])
            user_bins[i].size++;
        }
        if (crs_data->has_default_bin_) {
          if (product_has_uncovered) add_default_bin_size();
        } else {
          if (product_has_uncovered) crs_data->auto_bins_size++;
          else crs_data->excluded_auto_products_.insert(product);
        }
      } else {
        // Non-enumerable product (e.g. double intervals or wide auto-bins):
        // resolve whole-product binsof bins cleanly and treat non-queue with() bins as Partial.
        std::fill(matched.begin(), matched.end(), false);
        std::fill(partial_matched.begin(), partial_matched.end(), false);
        bool whole_any = false;
        bool whole_excluded = false;

        for (size_t i = 0; i < user_bins.size(); ++i) {
          if (user_bins[i].bin_type == default_bin_ || !user_bins[i].select)
            continue;
          if (!bin_has_with[i] && !bin_has_queue[i]) {
            matched[i] = user_bins[i].select->matches(product);
            whole_any = whole_any || matched[i];
            whole_excluded = whole_excluded || (matched[i] && (user_bins[i].bin_type == ignore_ || user_bins[i].bin_type == illegal_));
          } else if (bin_has_with[i] && !is_queue_backed[i]) {
            partial_matched[i] = user_bins[i].select->matches(product);
          }
        }

        for (size_t i = 0; i < user_bins.size(); ++i) {
          if (is_queue_backed[i]) continue;
          if ((matched[i] || partial_matched[i]) && (user_bins[i].bin_type != default_ || !whole_excluded))
            user_bins[i].size++;
        }
        if (crs_data->has_default_bin_) {
          if (!whole_any) add_default_bin_size();
        } else {
          if (!whole_any) crs_data->auto_bins_size++;
          else crs_data->excluded_auto_products_.insert(product);
        }
      }
    } while (next_product(product, sizes));

    for (size_t i = 0; i < user_bins.size(); ++i) {
      if (!is_queue_backed[i] || !queue_nodes[i]->queue_count_tuples)
        continue;
      const auto& target_bin = user_bins[i];
      user_bins[i].size = queue_nodes[i]->queue_count_tuples(
          cvps_vec,
          [&](const std::vector<size_t>& prod,
              const void* tup_ptr,
              const void* const* elem_ptrs) -> bool {
            if (cross_with_pred_ && !cross_with_pred_(tup_ptr))
              return false;
            if (!target_bin.select->matches_tuple(prod, tup_ptr, elem_ptrs))
              return false;
            if (target_bin.bin_type == default_) {
              for (size_t j = 0; j < user_bins.size(); ++j) {
                if ((user_bins[j].bin_type == ignore_ || user_bins[j].bin_type == illegal_) &&
                    user_bins[j].select &&
                    user_bins[j].select->matches_tuple(prod, tup_ptr, elem_ptrs)) {
                  return false;
                }
              }
            }
            return true;
          });
    }
  }

  /*!
   * \brief Calls build_cross_bins() eagerly when no crossed coverpoint has
   * deferred bins, or validates coverpoint membership and non-deferred binsof
   * conditions while deferring full resolution until ensure_bins().
   */
  void build_or_defer_cross_bins_or_free()
  {
    try {
      const std::type_info* expected_tuple_type = &typeid(std::tuple<Args...>);
      for (const auto& bin : crs_data->user_bins) {
        if (bin.select)
          bin.select->validate_tuple_type(expected_tuple_type);
      }
      bool any_deferred = false;
      for (auto* cvp : cvps_vec) {
        if (cvp->has_deferred_bins()) {
          any_deferred = true;
          break;
        }
      }
      if (!any_deferred) {
        build_cross_bins();
      } else {
        for (auto& bin : crs_data->user_bins) {
          if (bin.select)
            bin.select->validate_non_deferred(cvps_vec);
        }
      }
    }
    catch (...) {
      delete crs_data;
      crs_data = nullptr;
      throw;
    }
  }

  /*!
   * \brief Hits the bins of the products of a sample, for a cross which
   * has user defined bins
   * \param products The products of bins hit by the sample
   */
  void sample_user_bins(const std::vector<std::vector<size_t>>& products,
                        const void* tuple_ptr,
                        const void* const* elem_ptrs)
  {
    auto& user_bins = crs_data->user_bins;
    std::vector<bool> hit(user_bins.size(), false);
    std::vector<const std::vector<size_t>*> auto_products;
    const std::vector<size_t>* illegal_product = nullptr;
    const cross_bin_data_model* illegal = nullptr;
    bool hit_default_bin = false;

    for (auto &product : products)
    {
      std::vector<size_t> matched;
      bool excluded = false;
      for (size_t i = 0; i < user_bins.size(); ++i)
      {
        if (user_bins[i].bin_type == default_bin_) continue;
        if (user_bins[i].iff_cond && !user_bins[i].iff_cond()) continue;
        if (!user_bins[i].select || !user_bins[i].select->matches_tuple(product, tuple_ptr, elem_ptrs))
          continue;
        matched.push_back(i);
        excluded = excluded || (user_bins[i].bin_type == ignore_ || user_bins[i].bin_type == illegal_);
        if (user_bins[i].bin_type == illegal_ && illegal == nullptr) {
          illegal = &user_bins[i];
          illegal_product = &product;
        }
      }
      if (matched.empty()) {
        if (crs_data->has_default_bin())
          hit_default_bin = true;
        else if (crs_data->is_auto_bin(product))
          auto_products.push_back(&product);
      }
      for (auto i : matched)
        if (user_bins[i].bin_type != default_ || !excluded)
          hit[i] = true;
    }

    if (hit_default_bin) {
      for (size_t i = 0; i < user_bins.size(); ++i) {
        if (user_bins[i].bin_type == default_bin_) {
          if (!user_bins[i].iff_cond || user_bins[i].iff_cond())
            hit[i] = true;
        }
      }
    }

    if (illegal != nullptr) {
      for (size_t i = 0; i < user_bins.size(); ++i)
        if (hit[i] && user_bins[i].bin_type == illegal_)
          user_bins[i].hits++;

      // the names of the bins of the product
      std::stringstream ss;
      for (size_t i = 0; i < illegal_product->size(); ++i)
      {
        auto cvp = dynamic_cast<coverpoint_base*>(cvps_vec[i]);
        ss << ((i == 0) ? "<" : ",");
        if (cvp != nullptr)
          ss << cvp->get_bins_base()[(*illegal_product)[i]]->name();
        else
          ss << (*illegal_product)[i];
      }
      ss << ">";
      illegal_bin_sample_exception e;
      e.update_bin_info(illegal->name, ss.str());
      e.update_cvp_info(crs_data->name);
      throw e;
    }

    bool counted = !auto_products.empty();
    for (size_t i = 0; i < user_bins.size(); ++i)
    {
      if (!hit[i]) continue;
      user_bins[i].hits++;
      counted = counted || (user_bins[i].bin_type == default_ || user_bins[i].bin_type == default_bin_);
    }
    for (auto product : auto_products)
      crs_data->bins[*product]++;
    if (!counted)
      crs_data->misses++;
  }

  /*!
   * \brief Helper function to recursively determine number of bins in a cross
   * \tparam Head Type of current processed coverpoint
   * \tparam Tail Rest of coverpoints
   * \param h Current coverpoint
   * \param t Rest of coverpoints
   * \returns number of bins in this cross
   */
  template <typename Head, typename... Tail>
  int get_size(Head h, Tail... t)
  {
    return h->size() * get_size(t...);
  }

  /*!
   * \brief End of recursion
   * \returns Default value 1
   */
  int get_size()
  {
    return 1;
  }

  /*!
   *  \brief Private constructor used by dynamic_covergroups
   *  \param cross name
   *  \param args Coverpoints to be crossed
   */
  template <typename... Restrictions>
  cross(const std::string& name, coverpoint<Args> *... args, Restrictions... binsofs)
  {
    this->crs_data->owner_ = this;
    this->crs_data->name = name;

    cvps_vec = std::vector<cvp_base*>{args...};
    typed_cvps_ = std::make_tuple(args...);
    for(auto cvp_it : cvps_vec) {
      crs_data->cross_cvps.push_back(cvp_it->get_data());
    }
    add_cross_bins(binsofs...);
    build_or_defer_cross_bins_or_free();
  };

public:

  /*! Crossed coverpoints storage */
  std::vector<cvp_base *> cvps_vec;

  /*!
   *  \brief Main constructor
   *  \param n Parent covergroup
   *  \param args Coverpoints to be crossed
   *  \param binsofs User defined bins, ignore bins and illegal bins of the
   *  cross, see \link fc4sc::cross_bin \endlink
   */
  template <typename... Restrictions>
  cross(cvg_base *n, coverpoint<Args> *... args, Restrictions... binsofs)
  {
    this->crs_data->owner_ = this;
    cvps_vec = std::vector<cvp_base*>{args...};
    typed_cvps_ = std::make_tuple(args...);
    for(auto cvp_it : cvps_vec) {
      crs_data->cross_cvps.push_back(cvp_it->get_data());
    }
    add_cross_bins(binsofs...);
    build_or_defer_cross_bins_or_free();

    // registered once it is built: a cross which throws is not sampled
    parent_cvg_ptr_ = n;
    n->cvps.push_back(this);
    n->get_cvg_data()->add_cvp_data(crs_data);
  };

  /*!
   *  \brief Constructor which takes the name of the cross
   *  \param n Parent covergroup
   *  \param name Name of the cross
   *  \param args Coverpoints to be crossed
   *  \param binsofs User defined bins, ignore bins and illegal bins of the
   *  cross, see \link fc4sc::cross_bin \endlink
   */
  template <typename... Restrictions>
  cross(cvg_base *n, const std::string& name, coverpoint<Args> *... args, Restrictions... binsofs)
    : cross(n, args..., binsofs...) {
    this->crs_data->name = name;
  };

  cross(const cross&) = delete;
  cross& operator=(const cross&) = delete;

  cross(cross&& other) noexcept
    : crs_data(other.crs_data),
      valid_data(std::move(other.valid_data)),
      collect(other.collect),
      total_coverpoints(other.total_coverpoints),
      has_sampled_(other.has_sampled_),
      cvp_epochs_(std::move(other.cvp_epochs_)),
      typed_cvps_(other.typed_cvps_),
      parent_cvg_ptr_(other.parent_cvg_ptr_),
      cross_with_pred_(std::move(other.cross_with_pred_)),
      iff_cond_(std::move(other.iff_cond_)),
      cvps_vec(std::move(other.cvps_vec))
  {
    if (crs_data != nullptr) {
      crs_data->owner_ = this;
    }
    if (parent_cvg_ptr_ != nullptr) {
      for (auto& ptr : parent_cvg_ptr_->cvps) {
        if (ptr == &other) {
          ptr = this;
          break;
        }
      }
    }
    other.crs_data = nullptr;
    other.parent_cvg_ptr_ = nullptr;
  }

  cross& operator=(cross&&) = delete;

  /*!
   *  \brief Default constructor
   */
  cross(){
    this->crs_data->owner_ = this;
  }

  ~cross() {
    if (valid_data.use_count() > 0 && crs_data != nullptr && crs_data->owner_ == this) {
      try {
        ensure_bins();
      } catch (...) {
      }
      crs_data->owner_ = nullptr;
    }
  }

private:
  void rebuild_after_modifier()
  {
    if (valid_data.use_count() == 0 || crs_data == nullptr) return;
    bool any_deferred = false;
    for (auto* cvp : cvps_vec) {
      if (cvp->has_deferred_bins()) {
        any_deferred = true;
        break;
      }
    }
    if (!any_deferred) {
      build_cross_bins();
    } else {
      cvp_epochs_.clear();
    }
  }

public:
  template <typename F>
  cross& with(F pred) &
  {
    auto wrapped = detail::wrap_cross_predicate<Args...>(std::move(pred));
    if (cross_with_pred_) {
      auto prev = cross_with_pred_;
      cross_with_pred_ = [prev, wrapped](const void* ptr) -> bool {
        return prev(ptr) && wrapped(ptr);
      };
    } else {
      cross_with_pred_ = wrapped;
    }
    rebuild_after_modifier();
    return *this;
  }

  template <typename F>
  cross with(F pred) &&
  {
    this->with(std::move(pred));
    return std::move(*this);
  }

  cross& iff(bool constant_cond) &
  {
    auto next = [constant_cond]() -> bool { return constant_cond; };
    if (iff_cond_) {
      auto prev = iff_cond_;
      iff_cond_ = [prev, next]() -> bool { return prev() && next(); };
    } else {
      iff_cond_ = next;
    }
    return *this;
  }

  cross iff(bool constant_cond) &&
  {
    this->iff(constant_cond);
    return std::move(*this);
  }

  cross& iff(const bool* flag_ptr) &
  {
    if (flag_ptr == nullptr)
      throw std::invalid_argument("cross::iff: null bool pointer");
    auto next = [flag_ptr]() -> bool { return *flag_ptr; };
    if (iff_cond_) {
      auto prev = iff_cond_;
      iff_cond_ = [prev, next]() -> bool { return prev() && next(); };
    } else {
      iff_cond_ = next;
    }
    return *this;
  }

  cross iff(const bool* flag_ptr) &&
  {
    this->iff(flag_ptr);
    return std::move(*this);
  }

  template <typename F,
            typename std::enable_if<
                !std::is_same<detail::decay_t<F>, bool>::value &&
                !std::is_convertible<detail::decay_t<F>, const bool*>::value &&
                !std::is_same<detail::decay_t<F>, std::nullptr_t>::value,
                int>::type = 0>
  cross& iff(F&& cond) &
  {
    std::function<bool()> next(std::forward<F>(cond));
    if (iff_cond_) {
      auto prev = iff_cond_;
      iff_cond_ = [prev, next]() -> bool { return prev() && next(); };
    } else {
      iff_cond_ = std::move(next);
    }
    return *this;
  }

  template <typename F,
            typename std::enable_if<
                !std::is_same<detail::decay_t<F>, bool>::value &&
                !std::is_convertible<detail::decay_t<F>, const bool*>::value &&
                !std::is_same<detail::decay_t<F>, std::nullptr_t>::value,
                int>::type = 0>
  cross iff(F&& cond) &&
  {
    this->iff(std::forward<F>(cond));
    return std::move(*this);
  }

  void ensure_bins() const {
    if (valid_data.use_count() == 0 || crs_data == nullptr) return;
    if (crs_data->in_ensure_bins_ || has_sampled_) return;
    struct Guard {
      bool& flag;
      explicit Guard(bool& f) : flag(f) { flag = true; }
      ~Guard() { flag = false; }
    } guard(crs_data->in_ensure_bins_);

    for (size_t i = 0; i < cvps_vec.size(); ++i) {
      if (i < crs_data->cross_cvps.size() && crs_data->cross_cvps[i]->owner_ == nullptr)
        return;
    }
    for (auto* cvp : cvps_vec) {
      if (cvp->get_data() != nullptr && cvp->get_data()->parent_cvg != nullptr &&
          !cvp->get_data()->in_ensure_bins_)
        cvp->get_data()->sync_options_from_parent();
      cvp->ensure_bins();
    }
    bool changed = (cvp_epochs_.size() != cvps_vec.size());
    for (size_t i = 0; !changed && i < cvps_vec.size(); ++i) {
      if (cvps_vec[i]->bins_epoch() != cvp_epochs_[i])
        changed = true;
    }
    if (changed) {
      const_cast<cross<Args...>*>(this)->build_cross_bins();
    }
  }

  /*!
   *  \brief Sampling function at cross level
   */
  virtual void sample() 
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }

    crs_data->sync_options_from_parent();
    ensure_bins();
    if (!this->collect || (crs_data->parent_cvg != nullptr && !crs_data->parent_cvg->collect))
      return;
    if (iff_cond_ && !iff_cond_())
      return;
    has_sampled_ = true;

    // the bins hit by the sample, for each coverpoint
    std::vector<std::vector<size_t>> hit_bins;
    for (auto& cvp : cvps_vec) {
      if (!cvp->last_sample_success) {
        crs_data->misses++;
        return;
      }
      if (cvp->last_bins_hit.empty())
        hit_bins.push_back(std::vector<size_t>(1, cvp->last_bin_index_hit));
      else
        hit_bins.push_back(cvp->last_bins_hit);
    }

    std::tuple<Args...> sampled_tuple;
    std::array<const void*, sizeof...(Args)> elem_ptrs{};
    const bool has_tuple_values = extract_sampled_tuple_impl<0>(sampled_tuple, elem_ptrs);
    if (cross_with_pred_ && has_tuple_values && !cross_with_pred_(&sampled_tuple)) {
      crs_data->misses++;
      return;
    }

    // As in SystemVerilog, every combination of the hit bins is hit once
    std::vector<std::vector<size_t>> products;
    std::vector<size_t> pos(hit_bins.size(), 0);
    std::vector<size_t> sizes;
    for (auto &bins : hit_bins)
      sizes.push_back(bins.size());
    do {
      std::vector<size_t> key(hit_bins.size());
      for (size_t i = 0; i < hit_bins.size(); ++i)
        key[i] = hit_bins[i][pos[i]];
      products.push_back(key);
    } while (next_product(pos, sizes));

    if (!crs_data->user_bins.empty()) {
      sample_user_bins(products,
                       has_tuple_values ? static_cast<const void*>(&sampled_tuple) : nullptr,
                       has_tuple_values ? elem_ptrs.data() : nullptr);
      return;
    }
    for (auto &product : products)
      crs_data->bins[product]++;
  }

  /*!
   *  \brief Computes coverage for this instance. With the strict_sv_options
   *  and merge_instances type options, it is the type coverage unless the
   *  get_inst_coverage option of the covergroup instance is set.
   *  \returns Coverage value as a double between 0 and 100
   */
  double get_inst_coverage() const 
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }

    crs_data->sync_options_from_parent();
    ensure_bins();
    if (inst_coverage_is_type_coverage(crs_data->parent_cvg))
      return this->get_coverage();

    int covered = 0;
    int total = 0;
    crs_data->get_coverage_bins(covered, total);

    if (total == 0)
      return (this->crs_data->option.weight == 0) ? 100 : 0;

    double real = 100.0 * covered / total;
    return (real >= this->crs_data->option.goal) ? 100 : real;
  }

  /*!
   *  \brief Computes coverage for this instance, or the type coverage as
   *  get_inst_coverage()
   *  \param covered Number of covered bins
   *  \param total Total number of bins in this cross
   *  \returns Coverage value as a double between 0 and 100
   */
  double get_inst_coverage(int &covered, int &total) const 
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }

    crs_data->sync_options_from_parent();
    ensure_bins();
    if (inst_coverage_is_type_coverage(crs_data->parent_cvg))
      return this->get_coverage(covered, total);

    crs_data->get_coverage_bins(covered, total);

    if (total == 0)
      return (this->crs_data->option.weight == 0) ? 100 : 0;

    double real = 100.0 * covered / total;
    return (real >= this->crs_data->option.goal) ? 100 : real;
  }

  cvp_base_data_model* get_data()
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }

    return crs_data;
  }

  /*!
   *  \brief Computes the type coverage of this cross: its coverage
   *  across all the instances of the covergroup type it is in
   *  \returns Coverage value as a double between 0 and 100
   */
  double get_coverage() const
  {
    int covered = 0;
    int total = 0;
    return this->get_coverage(covered, total);
  }

  /*!
   *  \brief Computes the type coverage of this cross
   *  \param covered Number of covered bins, summed over the instances
   *  \param total Total number of bins, summed over the instances
   *  \returns Coverage value as a double between 0 and 100
   */
  double get_coverage(int &covered, int &total) const
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    return fc4sc::global::get_cvp_coverage(crs_data, covered, total);
  }

  /*!
   * \brief Enables sampling
   */
  void start() override
  {
    collect = true;
  }

  /*!
   * \brief Disables sampling
   */
  void stop() override
  {
    collect = false;
  }

  bool is_collecting() const override
  {
    if (valid_data.use_count() == 0 || crs_data == nullptr) return false;
    return collect && (crs_data->parent_cvg == nullptr || crs_data->parent_cvg->collect);
  }

  void on_stopped_sample() override
  {
    if (valid_data.use_count() == 0 || crs_data == nullptr) return;
    crs_data->sync_options_from_parent();
    ensure_bins();
  }


  /*!
   * \brief Returns instance name
   */
  std::string& name() 
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }

    return crs_data->name;
  }

  /*!
   * \brief Returns instance options
   */
  cross_option& option() 
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }

    crs_data->sync_options_from_parent();
    return crs_data->option;
  }

  /*!
   * \brief Returns cross bin misses
   */
  uint64_t get_misses()
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }

    return crs_data->misses;
  }

  /*!
   * \brief Returns cross coverage weight
   */
  uint64_t get_weight()
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }

    return this->crs_data->option.weight;
  };

  uint64_t size() const
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    ensure_bins();

    uint64_t total = 1;
    for (auto& cvp : cvps_vec) 
    {
      total *= cvp->size();
    }
    return total;
    //return total_coverpoints;
  };

  /*! Get cross bins storage */
  const std::map<std::vector<size_t>, uint64_t>& get_cross_bins() const
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    ensure_bins();

    return crs_data->bins;
  }

  /*! Get crossed coverpoints storage */
  const std::vector<cvp_base *>& get_cross_coverpoints() const
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }

    return cvps_vec;
  }

};

} // namespace fc4sc

#endif /* FC4SC_CROSS_HPP */
