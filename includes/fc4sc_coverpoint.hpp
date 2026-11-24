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
 \file coverpoint.hpp
 \brief Helper functions and interfaces

 This file contains helper functions and the base classes
 for bins, coverpoints and covergroups.
 */

#ifndef FC4SC_COVERPOINT_HPP
#define FC4SC_COVERPOINT_HPP

#include <type_traits>
#include <list>
#include <deque>
#include <unordered_map>
#include <functional>
#include <tuple>
#include <algorithm> // std::find
#include <set>

#include "fc4sc_bin.hpp"

namespace fc4sc
{

// Friend classes forward declaration
template <typename T>
class binsof;

template <typename ... Args>
class cross;

/*!
 * \brief Defines a class for coverpoints
 * \tparam T Type of bins that this coverpoint is holding
 */
template <class T>
class coverpoint;

class covergroup;

template<typename T, typename U>
class dynamic_coverpoint_factory;
class dynamic_covergroup_factory;

/*!
 *  \class coverpoint_data_model fc_coverpoint.hpp
 *  \brief class for storing coverage data of coverpoints
 *
 *  Data storage for coverpoints
 */
class coverpoint_data_model : public coverpoint_base_data_model {
public:

  /*! String-ified sample expression. */
  std::string sample_expression_str;

  /*!
   * String-ified sample condition. It can be used for reporting.
   * When the string is empty, no sample condition is registered.
   */
  std::string sample_condition_str;

  uint64_t size() const override {
    if (owner_ != nullptr && !in_ensure_bins_)
      owner_->ensure_bins();
    return bins_data.size();
  }

  std::string& get_sample_expression_str() override
  {
    return sample_expression_str;
  }

  std::string& get_sample_condition_str() override
  {
    return sample_condition_str;
  }

  void accept_visitor(covVisitorBase& visitor) override
  {
    sync_options_from_parent();
    if (owner_ != nullptr && !in_ensure_bins_)
      owner_->ensure_bins();
    visitor.visit(*this);
  }

};

template <class T>
class coverpoint final : public coverpoint_base
{
private:
  static_assert(fc4sc_type_traits<T>::is_supported, "Type is not supported by fc4sc!");
  /*
   * The covergroup class needs access to private members of the coverpoint
   * in order to set the sample condition, expression and name.
   */
  friend class covergroup;
  // binsof selects bins of the coverpoint for the bins of a cross
  friend class binsof<T>;
  template <typename... Args>
  friend class cross;
  // friend functions that need access to the bin vectors -> add_to_cvp(coverpoint<T>&)
  friend class bin<T>;
  friend class bin_array<T>;
  friend class ignore_bin<T>;
  friend class illegal_bin<T>;
  friend class transition_bin<T>;
  friend class default_bin<T>;

  template<typename U, typename V>
  friend class dynamic_coverpoint_factory;
  friend class dynamic_covergroup_factory;

  typedef std::pair<unsigned int, unsigned int> bin_range_t;

  struct IntervalComp {
    bool operator() (const fc4sc::interval_t<T>& a, const fc4sc::interval_t<T>& b) const {
      return fc4sc_type_traits<T>::less(a.second, b.second);
    }
  };

  typedef std::map<fc4sc::interval_t<T>,std::vector<bin_range_t>,IntervalComp> interval_map_t;
  typedef typename std::map<fc4sc::interval_t<T>,std::vector<bin_range_t>,IntervalComp>::iterator interval_map_iterator_t;

  bool has_sample_expression = false;
  mutable bool has_explicit_bins_ = false;
  mutable bool has_with_bin_array_ = false;
  mutable bool has_sampled_ = false;
  mutable bool auto_bins_built_ = false;
  mutable unsigned int built_auto_bin_max_ = 0;
  mutable bool bins_built_ = false;
  mutable bool built_distribute_first_ = false;
  mutable uint64_t bins_epoch_ = 0;
  std::vector<std::function<void(coverpoint<T>&, bool)>> regular_specs_;

  void invalidate_auto_bins() {
    if (!cvp_data->in_ensure_bins_)
      auto_bins_built_ = false;
  }

  void prepare_explicit_bin() {
    if (!cvp_data->in_ensure_bins_) {
      if (!has_explicit_bins_ && auto_bins_built_) {
        clear_regular_bins();
        auto_bins_built_ = false;
      }
      has_explicit_bins_ = true;
    }
  }

  void record_bin_spec(const bin<T>& b) {
    std::string bname = b.bin_data->name;
    std::vector<interval_t<T>> intvs = b.bin_data->intervals;
    regular_specs_.push_back([bname, intvs](coverpoint<T>& self, bool) {
      bin<T>(bname, intvs).add_to_cvp(self);
    });
  }

  void record_bin_array_spec(const bin_array<T>& ba) {
    std::string bname = ba.orig_name_;
    std::vector<interval_t<T>> orig_intvs = ba.orig_intervals_;
    std::vector<T> orig_vals = ba.orig_values_;
    uint64_t cnt = ba.orig_count_;
    bool sp = ba.orig_sparse_;
    bool fv = ba.orig_from_values_;
    bool dist = ba.distribute;
    auto preds = ba.with_preds_;
    std::vector<interval_t<T>> filtered_intvs = ba.bin_data->intervals;
    regular_specs_.push_back(
        [bname, orig_intvs, orig_vals, cnt, sp, fv, dist, preds, filtered_intvs](
            coverpoint<T>& self, bool df) {
          if (df && !preds.empty() && !sp) {
            if (fv) {
              bin_array<T> tmp(bname, cnt, orig_vals);
              tmp.with_preds_ = preds;
              tmp.add_to_cvp_with_mode(self, true);
            } else {
              bin_array<T> tmp(bname, cnt, orig_intvs[0]);
              tmp.with_preds_ = preds;
              tmp.add_to_cvp_with_mode(self, true);
            }
          } else if (dist) {
            bin_array<T> tmp(bname, orig_vals);
            tmp.bin_data->intervals = filtered_intvs;
            tmp.count = cnt;
            tmp.sparse = false;
            tmp.distribute = true;
            tmp.add_to_cvp_with_mode(self, false);
          } else if (sp) {
            bin_array<T> tmp(bname, std::vector<interval_t<T>>(filtered_intvs));
            tmp.add_to_cvp_with_mode(self, false);
          } else {
            bin_array<T> tmp(bname, cnt, orig_intvs[0]);
            tmp.add_to_cvp_with_mode(self, false);
          }
        });
  }

  void record_transition_spec(const transition_bin<T>& tb) {
    transition_bin_data_model<T> snap =
        *static_cast<const transition_bin_data_model<T>*>(tb.bin_data);
    bool arr = tb.array;
    regular_specs_.push_back([snap, arr](coverpoint<T>& self, bool) {
      transition_bin<T> tmp(snap.name);
      *static_cast<transition_bin_data_model<T>*>(tmp.bin_data) = snap;
      tmp.bin_data->interval_hits.assign(snap.interval_hits.size(), 0);
      tmp.array = arr;
      tmp.add_to_cvp(self);
    });
  }

  void clear_regular_bins() {
    for (auto* bd : cvp_data->bins_data) {
      delete bd;
    }
    cvp_data->bins_data.clear();
    bins.clear();
    regular_interval_map.clear();
    trans_bins.clear();
    trans_depth = 0;
    for (auto idx : trans_illegal_bins) {
      auto* td = static_cast<transition_bin_data_model<T>*>(illegal_bins[idx].bin_data);
      for (const auto& seq : td->sequences)
        trans_depth = std::max<size_t>(trans_depth, seq.size());
      if (!td->patterns.empty())
        trans_depth = std::max<size_t>(trans_depth, 1);
    }
    for (auto idx : trans_ignore_bins) {
      auto* td = static_cast<transition_bin_data_model<T>*>(ignore_bins[idx].bin_data);
      for (const auto& seq : td->sequences)
        trans_depth = std::max<size_t>(trans_depth, seq.size());
      if (!td->patterns.empty())
        trans_depth = std::max<size_t>(trans_depth, 1);
    }
  }

  /*! Pointer to this object's data */
  coverpoint_data_model* cvp_data = new coverpoint_data_model;

  /*! For ensuring the object's data has not been deallocated */
  std::weak_ptr<bool> valid_data = cvp_data->valid;

  /*! Expression used to sample the data */
  std::function<T()> sample_expression;

  /*!
   *  \brief Copies the data of a bin, as the data of its type, with the hit
   *  counts cleared
   */
  static bin_data_model<T>* copy_bin_data(const bin_data_model<T>* data)
  {
    bin_data_model<T>* copy;
    auto trans_data = dynamic_cast<const transition_bin_data_model<T>*>(data);
    if (trans_data != nullptr)
      copy = new transition_bin_data_model<T>(*trans_data);
    else
      copy = new bin_data_model<T>(*data);
    std::fill(copy->interval_hits.begin(), copy->interval_hits.end(), 0);
    return copy;
  }

  /*!
   *  \brief Create a dynamic copy of this coverpoint with matching bin types
   */
  cvp_base* create_instance(cvg_base*) override
  {
    coverpoint<T>* cvp = new coverpoint<T>;
    cvp->cvp_data->name = this->name();
    cvp->cvp_data->option = this->option();
    cvp->cvp_data->own_type_option = this->cvp_data->own_type_option;
    cvp->cvp_data->sample_expression_str = this->get_sample_expression_str();
    cvp->cvp_data->sample_condition_str = this->get_sample_condition_str();
    cvp->has_explicit_bins_ = this->has_explicit_bins_;
    cvp->has_with_bin_array_ = this->has_with_bin_array_;
    cvp->regular_specs_ = this->regular_specs_;
    if (this->has_explicit_bins_) {
      // the data of a transition bin is copied as such, with its sequences
      for(auto& bin_it : this->bins)
      {
        auto new_bin_data = copy_bin_data(bin_it.bin_data);
        cvp->bins.push_back(bin_it);
        cvp->bins.back().bin_data = new_bin_data;
        cvp->cvp_data->bins_data.push_back(new_bin_data);
      }
      cvp->regular_interval_map = this->regular_interval_map;
      cvp->trans_bins = this->trans_bins;
      cvp->trans_depth = this->trans_depth;
    }
    for(auto& bin_it : this->illegal_bins)
    {
      auto new_bin_data = copy_bin_data(bin_it.bin_data);
      cvp->illegal_bins.push_back(bin_it);
      cvp->illegal_bins.back().bin_data = new_bin_data;
      cvp->cvp_data->illegal_bins_data.push_back(new_bin_data);
    }
    for(auto& bin_it : this->ignore_bins)
    {
      auto new_bin_data = copy_bin_data(bin_it.bin_data);
      cvp->ignore_bins.push_back(bin_it);
      cvp->ignore_bins.back().bin_data = new_bin_data;
      cvp->cvp_data->ignore_bins_data.push_back(new_bin_data);
    }

    for(auto& bin_it : this->default_bins)
    {
      auto new_bin_data = new default_bin_data_model<T>;
      cvp->default_bins.push_back(bin_it);
      cvp->default_bins.back().bin_data = new_bin_data;
      *new_bin_data = *static_cast<default_bin_data_model<T>*>(bin_it.bin_data);
      cvp->cvp_data->default_bins_data.push_back(new_bin_data);
      std::fill(new_bin_data->interval_hits.begin(),new_bin_data->interval_hits.end(),0);
      new_bin_data->value_hits.clear();
    }

    cvp->has_sample_expression = this->has_sample_expression;
    cvp->sample_expression = this->sample_expression;
    cvp->sample_condition = this->sample_condition;

    cvp->illegal_interval_map = this->illegal_interval_map;
    cvp->ignore_interval_map = this->ignore_interval_map;

    // the instance has its own sample history, which starts empty
    cvp->trans_illegal_bins = this->trans_illegal_bins;
    cvp->trans_ignore_bins = this->trans_ignore_bins;
    if (!this->has_explicit_bins_) {
      for (auto idx : cvp->trans_illegal_bins) {
        auto* td = static_cast<transition_bin_data_model<T>*>(cvp->illegal_bins[idx].bin_data);
        for (const auto& seq : td->sequences)
          cvp->trans_depth = std::max<size_t>(cvp->trans_depth, seq.size());
        if (!td->patterns.empty())
          cvp->trans_depth = std::max<size_t>(cvp->trans_depth, 1);
      }
      for (auto idx : cvp->trans_ignore_bins) {
        auto* td = static_cast<transition_bin_data_model<T>*>(cvp->ignore_bins[idx].bin_data);
        for (const auto& seq : td->sequences)
          cvp->trans_depth = std::max<size_t>(cvp->trans_depth, seq.size());
        if (!td->patterns.empty())
          cvp->trans_depth = std::max<size_t>(cvp->trans_depth, 1);
      }
    }
    return cvp;
  }

  /*! Condition based on which the sampling takes place or not */
  std::function<bool()> sample_condition;

  // pointer sample variable (assigned via SAMPLE_POINT macro)
  T* sample_point = nullptr;

  /*! bins contained in this coverpoint */
  std::vector<bin<T>> bins;
  /*! Illegal bins contained in this coverpoint */
  std::vector<illegal_bin<T>> illegal_bins;
  /*! Ignore bins contained in this coverpoint */
  std::vector<ignore_bin<T>> ignore_bins;
  /*! Default bins contained in this coverpoint */
  std::vector<default_bin<T>> default_bins;

  /*!
   *  \brief Gives the default bins the values which are in no other bin.
   *  Called when a bin is added, as the values of the default bins depend
   *  on all the other bins.
   */
  void update_default_bins()
  {
    typedef fc4sc_type_traits<T> traits;
    if (default_bins.empty()) return;

    std::vector<fc4sc::interval_t<T>> used;
    for (auto &bin_it : bins)
      used.insert(used.end(), bin_it.bin_data->intervals.begin(), bin_it.bin_data->intervals.end());
    for (auto &bin_it : illegal_bins)
      used.insert(used.end(), bin_it.bin_data->intervals.begin(), bin_it.bin_data->intervals.end());
    for (auto &bin_it : ignore_bins)
      used.insert(used.end(), bin_it.bin_data->intervals.begin(), bin_it.bin_data->intervals.end());
    std::sort(used.begin(), used.end(), [](const fc4sc::interval_t<T>& a, const fc4sc::interval_t<T>& b) {
      if (!traits::equal(a.first, b.first)) return traits::less(a.first, b.first);
      return traits::less(a.second, b.second);
    });

    // the values between the used intervals
    std::vector<fc4sc::interval_t<T>> values;
    T next = traits::min_value();
    bool past_max = false;
    for (auto &intv : used)
    {
      if (traits::less(intv.second, next)) continue;
      if (traits::less(next, intv.first))
        values.push_back(fc4sc::interval<T>(next, traits::prev(intv.first)));
      if (traits::equal(intv.second, traits::max_value())) {
        past_max = true;
        break;
      }
      next = traits::next(intv.second);
    }
    if (!past_max)
      values.push_back(fc4sc::interval<T>(next, traits::max_value()));

    for (auto &bin_it : default_bins)
      bin_it.set_values(values);
  }

  /*! Indices of the transition bins in bins */
  std::vector<size_t> trans_bins;
  /*! Indices of the transition bins in illegal_bins */
  std::vector<size_t> trans_illegal_bins;
  /*! Indices of the transition bins in ignore_bins */
  std::vector<size_t> trans_ignore_bins;

  /*! Length of the longest sequence of the transition bins */
  size_t trans_depth = 0;

  /*! The last sampled values, as many as trans_depth */
  std::deque<T> trans_history;

  /*! Number of the last sampled values which are not ignored values */
  size_t trans_clean = 0;

  /*! Number of samples given to the transition bins */
  uint64_t trans_samples = 0;

  /*! Checks if the coverpoint has a default sequence bin */
  bool has_default_sequence()
  {
    for (auto &bin_it : default_bins)
      if (bin_it.default_data()->sequence)
        return true;
    return false;
  }

  /*! Sampling switch */
  bool collect = true;

  /*! Retained last canonicalized sampled value for cross tuple predicates */
  T last_sampled_value_{};
  bool has_last_sampled_value_ = false;

  /*! Whether the bins have been checked for option.detect_overlap */
  bool overlap_checked = false;

  /*!
   *  \brief Prints a warning for each pair of regular bins whose values
   *  overlap, for option.detect_overlap
   */
  void report_overlapping_bins()
  {
    std::set<std::pair<unsigned int, unsigned int>> reported;
    for (auto &entry : regular_interval_map)
    {
      std::set<unsigned int> bin_indexes;
      for (auto &bin_range : entry.second)
        bin_indexes.insert(bin_range.first);
      for (auto first = bin_indexes.begin(); first != bin_indexes.end(); ++first)
        for (auto second = std::next(first); second != bin_indexes.end(); ++second)
        {
          if (!reported.insert(std::make_pair(*first, *second)).second)
            continue;
          std::cerr << "Warning: bins " << bins[*first].name() << " and "
                    << bins[*second].name() << " of coverpoint "
                    << cvp_data->name << " overlap\n";
        }
    }
  }

  coverpoint<T>& operator=(coverpoint<T>& rh) = delete;

  /*
   * kept for backwards compatibility with previous instantiation method:
   * coverpoint<int> cvp = coverpoint<int> (...);
   */
  // coverpoint(coverpoint<T>&& rh) = delete;

  /*!
   *  \brief Build interval map from bin
   */
  void build_interval_map(interval_map_t& interval_map, bin<T>& bin, unsigned int bin_key)
  {
    auto map_it = interval_map.begin();
    unsigned int i = 0;
    std::vector<bin_range_t> entry{ {bin_key,i} };
    auto& elem_entry = entry[0];
    for(auto it = bin.bin_data->intervals.begin();it != bin.bin_data->intervals.end(); it++)
    {
      map_it = interval_map.insert(map_it,{*it, entry }); 
      elem_entry.second = ++i;
    }
  }


  /*!
   *  \brief Binary search in sorted map given a type T value
   */
  interval_map_iterator_t get_interval_bs(interval_map_t& interval_map, T val)
  {
    return interval_map.lower_bound(fc4sc::interval<T>(val,val));
  }

  typename interval_map_t::const_iterator get_interval_bs(const interval_map_t& interval_map, T val) const
  {
    return interval_map.lower_bound(fc4sc::interval<T>(val,val));
  }


  /*!
   *  \brief Helper for slicing intervals
   */
  interval_map_iterator_t slice_helper(interval_map_t& interval_map,interval_map_iterator_t map_it, fc4sc::interval_t<T> i0, fc4sc::interval_t<T> i1)
  {
    auto bin_range_vec = map_it->second;
   //To slice, we must erase the interval
   //cannot change a key in-place in std::map until C++17
    map_it = interval_map.erase(map_it);
    map_it = interval_map.insert(map_it, {i1,bin_range_vec} );
    return interval_map.insert(map_it, {i0,bin_range_vec} );
  }


  /*!
   *  \brief Helper for slicing the intervals of overlapping bins
   */
  interval_map_iterator_t slice_intervals_map(interval_map_iterator_t, interval_map_t& interval_map, fc4sc::interval_t<T> range)
  {
    typedef fc4sc_type_traits<T> traits;
    T start_cut = range.first;
    T end_cut = range.second;
    auto map_it = get_interval_bs(interval_map,end_cut);
    if(map_it != interval_map.end()) {
      auto it = map_it->first;
      if(traits::less(it.first, end_cut) && traits::less(end_cut, it.second)){
        map_it = slice_helper(interval_map,map_it,fc4sc::interval<T>(traits::next(end_cut),it.second),fc4sc::interval<T>(it.first,end_cut));
      }
      else if(traits::equal(end_cut, it.first) && !traits::equal(it.first, it.second)) {
        map_it = slice_helper(interval_map,map_it,fc4sc::interval<T>(traits::next(it.first),it.second),fc4sc::interval<T>(end_cut,end_cut));
      }
    }
    map_it = get_interval_bs(interval_map,start_cut);
    if(map_it != interval_map.end()) {
      auto it = map_it->first;
      if(traits::less(it.first, start_cut) && traits::less(start_cut, it.second)) {
        map_it = slice_helper(interval_map,map_it,fc4sc::interval<T>(it.first,traits::prev(start_cut)),fc4sc::interval<T>(start_cut,it.second));
      }
      else if(traits::equal(start_cut, it.second) && !traits::equal(it.first, it.second)) {
        map_it = slice_helper(interval_map,map_it,fc4sc::interval<T>(it.first,traits::prev(start_cut)),fc4sc::interval<T>(start_cut,start_cut));
      }
    }
    return map_it;
  }

  /*!
   *  \brief Helper for adding new ranges to the flat map representation of the coverpoint
   */
  interval_map_iterator_t add_range_map(interval_map_iterator_t hint_it,interval_map_t& interval_map,bin_range_t bin_range_key,fc4sc::interval_t<T> range)
  {
    typedef fc4sc_type_traits<T> traits;
    // slice_intervals_map() has cut the intervals of the map at the ends of
    // the range, so each interval of the map is either inside the range or
    // outside it. The bin is added to the intervals inside the range, and
    // the values of the range between them get intervals of their own.
    auto map_it = hint_it;
    while(map_it != interval_map.end() && traits::less(map_it->first.second, range.first))
      ++map_it;
    // lowest value of the range not yet in the map
    T next = range.first;
    while(map_it != interval_map.end() && !traits::less(range.second, map_it->first.first))
    {
      if(traits::less(next, map_it->first.first)) {
        std::vector<bin_range_t> bin_range_vec{bin_range_key};
        interval_map.insert(map_it, {fc4sc::interval<T>(next,traits::prev(map_it->first.first)), bin_range_vec } );
      }
      map_it->second.push_back(bin_range_key);
      if(traits::equal(map_it->first.second, range.second))
        return std::next(map_it);
      next = traits::next(map_it->first.second);
      ++map_it;
    }
    std::vector<bin_range_t> bin_range_vec{bin_range_key};
    return std::next(interval_map.insert(map_it, {fc4sc::interval<T>(next,range.second), bin_range_vec } ));
  }

  /*!
   *  \brief Helper for sorting the Flat map representation, adds a new bin to the coverpoint
   */
  void insert_intervals(interval_map_t& interval_map, bin<T>& new_bin, unsigned int bin_key)
  {
    if(interval_map.empty()) {
      build_interval_map(interval_map,new_bin,bin_key);
      return;
    }
    auto hint_it = interval_map.begin();
    unsigned int i = 0;
    for(auto bin_it = new_bin.bin_data->intervals.begin(); bin_it != new_bin.bin_data->intervals.end(); bin_it++)
    {
      hint_it = slice_intervals_map(hint_it,interval_map,*bin_it);
      hint_it = add_range_map(hint_it,interval_map,{bin_key,i++},*bin_it);
    }
  }

  /*! Flat map representation of coverpoint's bin for binary search sampling */
  interval_map_t regular_interval_map;

  /*! Flat map representation of coverpoint's bin for binary search sampling */
  interval_map_t illegal_interval_map;

  /*! Flat map representation of coverpoint's bin for binary search sampling */
  interval_map_t ignore_interval_map;
 
  /*!
   *  \brief Samples the transition bins
   *  \param cvp_val Value to be sampled for this coverpoint
   *  \param ignored Whether the value is in an ignore or illegal bin
   *
   *  Adds the value to the sample history and hits the transition bins
   *  whose transitions end with this sample. A transition which has an
   *  ignored or illegal value in it is not hit.
   */
  void sample_transitions(const T &cvp_val, bool ignored)
  {
    if (trans_depth == 0 && !has_default_sequence()) return;

    trans_history.push_back(cvp_val);
    if (trans_history.size() > trans_depth)
      trans_history.pop_front();
    trans_clean = ignored ? 0 : std::min(trans_clean + 1, trans_depth);
    trans_samples++;

    // the transitions with goto or nonconsecutive repetitions match each
    // sample, and no transition goes on past an ignored value. The matches
    // of the ignored ones are not matches of the others.
    std::vector<const trans_pattern<T>*> ignored_patterns;
    for (auto index : this->trans_ignore_bins)
      for (auto &pattern : static_cast<transition_bin_data_model<T>*>(this->ignore_bins[index].bin_data)->patterns)
        ignored_patterns.push_back(&pattern);
    const std::vector<const trans_pattern<T>*> no_patterns;
    auto step_patterns = [&](bin_data_model<T>* bin_data, const std::vector<const trans_pattern<T>*>& exclusions) {
      auto data = static_cast<transition_bin_data_model<T>*>(bin_data);
      if (data->patterns.empty()) return;
      if (ignored)
        data->reset_patterns();
      else
        data->step_patterns(cvp_val, trans_samples, exclusions, trans_depth);
    };
    for (auto index : this->trans_ignore_bins)
      step_patterns(this->ignore_bins[index].bin_data, no_patterns);
    for (auto index : this->trans_illegal_bins)
      step_patterns(this->illegal_bins[index].bin_data, ignored_patterns);
    for (auto index : this->trans_bins)
      step_patterns(this->bins[index].bin_data, ignored_patterns);
    if (ignored) return;

    // the lengths of the last values which are an ignored sequence, up to
    // the longest value sequence
    std::vector<size_t> ignored_lengths;
    bool ignored_hit = false;
    for (auto index : this->trans_ignore_bins)
    {
      auto data = static_cast<transition_bin_data_model<T>*>(this->ignore_bins[index].bin_data);
      if (data->sample(trans_history, trans_clean, nullptr, &ignored_lengths) >= 0)
        ignored_hit = true;
    }

    for (auto index : this->trans_illegal_bins)
    {
      auto data = static_cast<transition_bin_data_model<T>*>(this->illegal_bins[index].bin_data);
      int seq = data->sample(trans_history, trans_clean, &ignored_lengths);
      if (seq >= 0) {
        illegal_bin_sample_exception e;
        e.update_bin_info(data->name, data->sequence_text(seq));
        e.update_cvp_info(this->cvp_data->name);
        throw e;
      }
    }

    bool trans_hit = ignored_hit;
    for (auto index : this->trans_bins)
    {
      auto data = static_cast<transition_bin_data_model<T>*>(this->bins[index].bin_data);
      if (data->sample(trans_history, trans_clean, &ignored_lengths) >= 0) {
        this->last_bin_index_hit = index;
        this->last_sample_success = true;
        this->last_bins_hit.push_back(index);
        trans_hit = true;
      }
    }

    // the default sequence bins count the samples which end no transition
    if (!trans_hit)
      for (auto &bin_it : this->default_bins)
        if (bin_it.default_data()->sequence)
          bin_it.bin_data->interval_hits[0]++;
  }

  /*!
   *  \brief Sampling function at coverpoint level
   *  \param cvp_val Value to be sampled for this coverpoint
   *
   *  Takes a value and searches it in the owned bins
   */
  void sample(const T &cvp_val)  {
#ifdef FC4SC_DISABLE_SAMPLING
    return;
#endif
    cvp_data->sync_options_from_parent();
    ensure_bins();
    // The options are plain fields which are set after the bins are
    // defined, so option.detect_overlap is looked at when sampling: the
    // check is made once, on the first sample after the option is set,
    // even if sampling is stopped.
    if (!overlap_checked && cvp_data->option.detect_overlap) {
      overlap_checked = true;
      report_overlapping_bins();
    }
    this->last_sample_success = false;
    this->last_bin_index_hit = 0;
    this->last_bins_hit.clear();
    this->has_last_sampled_value_ = false;
    if (!collect || (cvp_data->parent_cvg != nullptr && !cvp_data->parent_cvg->collect))
      return;
    has_sampled_ = true;

    if (fc4sc_type_traits<T>::is_nan(cvp_val)) {
      cvp_data->misses++;
      return;
    }
    const T cval = fc4sc_type_traits<T>::canonicalize(cvp_val);
    this->last_sampled_value_ = cval;
    this->has_last_sampled_value_ = true;

    // 1) Search if the value is in the ignore bins
    bool ignored = false;
    auto range_it  = get_interval_bs(this->ignore_interval_map,cval);
    if(range_it != this->ignore_interval_map.end()) {
      for(auto bin_range_it : range_it->second)
      {
        if (this->ignore_bins[bin_range_it.first].sample(cval,bin_range_it.second)) {
          ignored = true;
          break;
        }
      }
    }

    // 2) Search if the value is in the illegal bins
    if (!ignored) {
      range_it  = get_interval_bs(this->illegal_interval_map,cval);
      if(range_it != this->illegal_interval_map.end()) {
        for(auto bin_range_it : range_it->second)
        {
          try { this->illegal_bins[bin_range_it.first].sample(cval,bin_range_it.second); }
          catch (illegal_bin_sample_exception &e) {
            // no transition with an illegal value in it is hit either
            sample_transitions(cval, true);
            e.update_cvp_info(this->cvp_data->name);
            throw e;
          }
        }
      }
    }

    // 3) Sample the transition bins
    sample_transitions(cval, ignored);

    if (ignored) {
      cvp_data->misses++;
      return;
    }

    // Sample the default bins, which hold the values in no other bin
    for (auto &bin_it : this->default_bins)
      bin_it.sample_default(cval);

    // 4) Sample regular bins    
    range_it  = get_interval_bs(this->regular_interval_map,cval);
    if(range_it != this->regular_interval_map.end()) {
      for(auto bin_range_it : range_it->second)
      {
        if (this->bins[bin_range_it.first].sample(cval,bin_range_it.second)) {
          this->last_bin_index_hit = bin_range_it.first;
          this->last_sample_success = true;
          this->last_bins_hit.push_back(bin_range_it.first);
          if (this->stop_sample_on_first_bin_hit) break;
        }
      }
    }

    std::sort(this->last_bins_hit.begin(), this->last_bins_hit.end());
    this->last_bins_hit.erase(std::unique(this->last_bins_hit.begin(), this->last_bins_hit.end()),
                              this->last_bins_hit.end());

    if (!this->last_sample_success) { cvp_data->misses++; }
  }

public:

  /*! Default constructor */
  coverpoint() {
    cvp_data->owner_ = this;
  }

private:

  /*!
   *  \brief Constructor that registers a new default bin
   */
  template <typename... Args>
  coverpoint(bin<T> n, Args... args) : coverpoint(args...)
  {
    // an empty bin is not added, with a warning, by add_to_cvp()
    std::reverse(n.bin_data->intervals.begin(), n.bin_data->intervals.end());
    //bins.push_back(n);
    n.add_to_cvp(*this);
  }

  /*!
   *  \brief Constructor that registers a transition bin
   */
  template <typename... Args>
  coverpoint(transition_bin<T> n, Args... args) : coverpoint(args...)
  {
    n.add_to_cvp(*this);
  }

  /*!
   *  \brief Constructor that a bin array
   */
  template < typename... Args>
  coverpoint(bin_array<T> n, Args... args) : coverpoint(args...)
  {
    n.add_to_cvp(*this);
  }

  /*!
   *  \brief Constructor that registers a new illegal bin
   */
  template <typename... Args>
  coverpoint(illegal_bin<T> n, Args... args) : coverpoint(args...)
  {
    // an empty bin is not added, with a warning, by add_to_cvp()
    std::reverse(n.bin_data->intervals.begin(), n.bin_data->intervals.end());
    //illegal_bins.push_back(n);
    n.add_to_cvp(*this);
  }

  /*!
   *  \brief Constructor that registers a new default bin
   */
  template <typename... Args>
  coverpoint(default_bin<T> n, Args... args) : coverpoint(args...)
  {
    n.add_to_cvp(*this);
  }

  /*!
   *  \brief Constructor that registers a new ignore bin
   */
  template <typename... Args>
  coverpoint(ignore_bin<T> n, Args... args) : coverpoint(args...)
  {
    // an empty bin is not added, with a warning, by add_to_cvp()
    std::reverse(n.bin_data->intervals.begin(), n.bin_data->intervals.end());
    //ignore_bins.push_back(n);
    n.add_to_cvp(*this);
  }

  /*!
   *  \brief Constructor that takes the parent covergroup.
   */
  template <typename... Args>
  coverpoint(const std::string& cvp_name, Args... args) : coverpoint(args...)
  {
    this->cvp_data->name = cvp_name;
  }

  /*! Indices of the elements of a tuple */
  template <size_t... I> struct index_list {};

  /*! The indices of a tuple of N elements from the last to the first */
  template <size_t N, size_t... I>
  struct reversed_indices : reversed_indices<N - 1, I..., N - 1> {};
  template <size_t... I>
  struct reversed_indices<0, I...> { typedef index_list<I...> type; };

  /*!
   *  \brief Constructor that passes the elements of a tuple, in the order of
   *  the given indices, to the constructors above. Each of them registers its
   *  bin after the constructor it delegates to has registered the bins of the
   *  arguments behind it, so the bins are registered from the last argument
   *  to the first.
   */
  template <typename Tuple, size_t... I>
  coverpoint(Tuple args, index_list<I...>) : coverpoint(std::get<I>(args)...) { }

public:

  // Initialization constructor. Needed for the use of COVERGROUP macro;
  coverpoint(const coverpoint<T>& rh)
    : coverpoint_base(rh),
      has_sample_expression(rh.has_sample_expression),
      has_explicit_bins_(rh.has_explicit_bins_),
      has_with_bin_array_(rh.has_with_bin_array_),
      has_sampled_(rh.has_sampled_),
      auto_bins_built_(rh.auto_bins_built_),
      built_auto_bin_max_(rh.built_auto_bin_max_),
      bins_built_(rh.bins_built_),
      built_distribute_first_(rh.built_distribute_first_),
      bins_epoch_(rh.bins_epoch_),
      regular_specs_(rh.regular_specs_),
      cvp_data(rh.cvp_data),
      valid_data(rh.valid_data),
      sample_expression(rh.sample_expression),
      sample_condition(rh.sample_condition),
      sample_point(rh.sample_point),
      bins(rh.bins),
      illegal_bins(rh.illegal_bins),
      ignore_bins(rh.ignore_bins),
      default_bins(rh.default_bins),
      trans_bins(rh.trans_bins),
      trans_illegal_bins(rh.trans_illegal_bins),
      trans_ignore_bins(rh.trans_ignore_bins),
      trans_depth(rh.trans_depth),
      trans_history(rh.trans_history),
      trans_clean(rh.trans_clean),
      trans_samples(rh.trans_samples),
      collect(rh.collect),
      last_sampled_value_(rh.last_sampled_value_),
      has_last_sampled_value_(rh.has_last_sampled_value_),
      overlap_checked(rh.overlap_checked),
      regular_interval_map(rh.regular_interval_map),
      illegal_interval_map(rh.illegal_interval_map),
      ignore_interval_map(rh.ignore_interval_map)
  {
    if (this->valid_data.use_count() > 0 && this->cvp_data != nullptr)
      this->cvp_data->owner_ = this;
  }
  /*
   * Move assignment operator. Needed for the use of COVERGROUP macro. This should not be
   * "manually" used!
   */
  coverpoint<T>& operator=(coverpoint<T>&& rh) {
    /*
     * It is very important that the only fields moved from the right hand are
     * the bins. This is a needed assumption which makes the COVERPOINT macro
     * syntax work!
     */
    this->bins = std::move(rh.bins);
    this->ignore_bins = std::move(rh.ignore_bins);
    this->illegal_bins = std::move(rh.illegal_bins);
    this->default_bins = std::move(rh.default_bins);
    this->regular_interval_map = std::move(rh.regular_interval_map);
    this->illegal_interval_map = std::move(rh.illegal_interval_map);
    this->ignore_interval_map = std::move(rh.ignore_interval_map);
    this->trans_bins = std::move(rh.trans_bins);
    this->trans_illegal_bins = std::move(rh.trans_illegal_bins);
    this->trans_ignore_bins = std::move(rh.trans_ignore_bins);
    this->trans_depth = rh.trans_depth;
    this->trans_history.clear();
    this->trans_clean = 0;

    this->has_explicit_bins_ = rh.has_explicit_bins_;
    this->has_with_bin_array_ = rh.has_with_bin_array_;
    this->has_sampled_ = rh.has_sampled_;
    this->auto_bins_built_ = rh.auto_bins_built_;
    this->built_auto_bin_max_ = rh.built_auto_bin_max_;
    this->bins_built_ = rh.bins_built_;
    this->built_distribute_first_ = rh.built_distribute_first_;
    this->bins_epoch_ = rh.bins_epoch_;
    this->regular_specs_ = std::move(rh.regular_specs_);

    this->cvp_data->bins_data = rh.cvp_data->bins_data;
    this->cvp_data->illegal_bins_data = rh.cvp_data->illegal_bins_data;
    this->cvp_data->ignore_bins_data = rh.cvp_data->ignore_bins_data;
    this->cvp_data->default_bins_data = rh.cvp_data->default_bins_data;
    this->cvp_data->owner_ = this;
    rh.cvp_data->bins_data.clear();
    rh.cvp_data->illegal_bins_data.clear();
    rh.cvp_data->ignore_bins_data.clear();
    rh.cvp_data->default_bins_data.clear();
    rh.cvp_data->owner_ = nullptr;
    delete rh.cvp_data;
    rh.cvp_data = nullptr;
    return *this;
  }

  ~coverpoint() {
    if (valid_data.use_count() > 0 && cvp_data != nullptr && cvp_data->owner_ == this) {
      try {
        if (cvp_data->parent_cvg != nullptr) {
          cvp_data->sync_options_from_parent();
        }
        ensure_bins();
      } catch (...) {
      }
      cvp_data->owner_ = nullptr;
    }
  }

  /*!
   * \brief Initializer list constructor that receives a list of bin (of any types,
   * even mixed) and registers all the bins in this coverpoint.
   */
  coverpoint(std::initializer_list<const bin_wrapper<T>> list) {
    cvp_data->owner_ = this;
    for (const bin_wrapper<T> &bin_w : list) {
      bin_w.get_bin()->add_to_cvp(*this);
    }
  }

  template <typename... Args>
  coverpoint(cvg_base *parent_cvg, Args... args) :
    coverpoint(std::make_tuple(args...), typename reversed_indices<sizeof...(Args)>::type()) {
    static_assert(forbid_type<cvg_base *, Args...>::value, "Coverpoint constructor accepts only 1 parent covergroup pointer!");
    // The delegated constructors register the bins in reverse order, so they
    // are given the arguments reversed: the bins keep the order of declaration.
    parent_cvg->register_cvp(this);

    // set strings here
    auto strings = parent_cvg->get_strings(this);
    this->sample_point = static_cast<T *>(std::get<0>(strings));
    this->cvp_data->name = std::get<1>(strings);
    this->cvp_data->sample_expression_str = std::get<2>(strings);
    parent_cvg->get_cvg_data()->add_cvp_data(this->cvp_data);
  }

  uint64_t bins_epoch() const override {
    return bins_epoch_;
  }

  bool has_deferred_bins() const override {
    return !has_explicit_bins_ || has_with_bin_array_;
  }

  void ensure_bins() const override {
    if (valid_data.use_count() == 0 || cvp_data == nullptr) return;
    if (cvp_data->in_ensure_bins_ || has_sampled_) return;
    coverpoint<T>* self = const_cast<coverpoint<T>*>(this);
    struct Guard {
      bool& flag;
      explicit Guard(bool& f) : flag(f) { flag = true; }
      ~Guard() { flag = false; }
    } guard(cvp_data->in_ensure_bins_);

    if (!has_explicit_bins_) {
      const unsigned int max_bins = cvp_data->option.auto_bin_max;
      if (auto_bins_built_ && built_auto_bin_max_ == max_bins) return;
      typedef fc4sc_type_traits<T> traits;
      if (!traits::supports_auto_bins)
        throw std::invalid_argument("Coverpoint \"" + cvp_data->name +
                                    "\" requires explicit bins for non-discrete type");
      if (max_bins == 0)
        throw std::invalid_argument("Coverpoint \"" + cvp_data->name +
                                    "\" option.auto_bin_max must be at least 1");

      self->clear_regular_bins();

      std::vector<interval_t<T>> raw_excl;
      for (const auto& ib : ignore_bins)
        raw_excl.insert(raw_excl.end(), ib.bin_data->intervals.begin(), ib.bin_data->intervals.end());
      for (const auto& ib : illegal_bins)
        raw_excl.insert(raw_excl.end(), ib.bin_data->intervals.begin(), ib.bin_data->intervals.end());
      std::vector<interval_t<T>> excl;
      if (!raw_excl.empty())
        excl = fc4sc::reunion(raw_excl, std::vector<interval_t<T>>());

      auto subtract_and_add = [&](const std::string& bname, const T& a, const T& b) {
        std::vector<interval_t<T>> kept;
        if (excl.empty()) {
          kept.push_back(interval(a, b));
        } else {
          T cur = a;
          bool done = false;
          for (const auto& ex : excl) {
            if (traits::less(ex.second, cur)) continue;
            if (traits::less(b, ex.first)) break;
            if (traits::less(cur, ex.first)) {
              kept.push_back(interval(cur, traits::prev(ex.first)));
            }
            if (!traits::less(ex.second, b)) {
              done = true;
              break;
            }
            cur = traits::next(ex.second);
          }
          if (!done && !traits::less(b, cur)) {
            kept.push_back(interval(cur, b));
          }
        }
        if (!kept.empty()) {
          bin<T>(bname, kept).add_to_cvp(*self);
        }
      };

      if (traits::has_enumerated_values) {
        auto evals = traits::enumerated_values();
        std::vector<T> seen;
        for (const auto& ev : evals) {
          bool dup = false;
          for (const auto& s : seen) {
            if (traits::equal(s, ev.first)) { dup = true; break; }
          }
          if (dup) continue;
          seen.push_back(ev.first);
          std::string label = ev.second.empty() ? traits::to_string(ev.first) : ev.second;
          subtract_and_add("auto[" + label + "]", ev.first, ev.first);
        }
      } else {
        T lo{}, hi{};
        traits::auto_bin_bounds(lo, hi);
        auto parts = traits::split_interval(lo, hi, static_cast<uint64_t>(max_bins), true);
        for (const auto& p : parts) {
          std::string bname = traits::equal(p.first, p.second)
              ? ("auto[" + traits::to_string(p.first) + "]")
              : ("auto[" + traits::to_string(p.first) + ":" + traits::to_string(p.second) + "]");
          subtract_and_add(bname, p.first, p.second);
        }
      }

      self->update_default_bins();
      auto_bins_built_ = true;
      built_auto_bin_max_ = max_bins;
      ++bins_epoch_;
    } else if (has_with_bin_array_) {
      const bool df = cvp_data->type_option().distribute_first;
      if (bins_built_ && built_distribute_first_ == df) return;
      if (!bins_built_ && !df) {
        bins_built_ = true;
        built_distribute_first_ = false;
        return;
      }
      self->clear_regular_bins();
      for (const auto& spec : regular_specs_) {
        spec(*self, df);
      }
      self->update_default_bins();
      bins_built_ = true;
      built_distribute_first_ = df;
      ++bins_epoch_;
    }
  }

  /*!
   *  Retrieves the number of regular bins
   */
  uint64_t size() const override {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    if (cvp_data->parent_cvg != nullptr && !cvp_data->in_ensure_bins_)
      cvp_data->sync_options_from_parent();
    ensure_bins();
    return bins.size();
  }

  void sample() override
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    if (!is_collecting()) {
      on_stopped_sample();
      return;
    }
    if (has_sample_expression) {
      bool cond;
      try {
        cond = sample_condition();
      }  catch(const std::exception& e) {
        std::cerr << e.what() << "\n";
	std::cerr << "sample_condition is not binded for coverpoint " << this->cvp_data->name << "\n";
	throw;
      }
      if (cond) {
        try {
          this->sample(sample_expression());
	} catch(const std::exception& e) {
	  std::cerr << e.what() << "\n";
	  std::cerr << "sample_expression is not binded for coverpoint " << this->cvp_data->name << "\n";
	  throw;
	}
      }
      else {
        // This is a fix so that crosses are not sampled if any of the coverpoints
        // used for crossing has a sample condition which is not met.
        this->last_sample_success = false;
        this->last_bin_index_hit = 0;
        this->last_bins_hit.clear();
        this->has_last_sampled_value_ = false;
      }
    }
    else {
      this->sample(*sample_point);
    }
  }

  void on_stopped_sample() override
  {
    if (valid_data.use_count() == 0 || cvp_data == nullptr) return;
    cvp_data->sync_options_from_parent();
    ensure_bins();
    if (!overlap_checked && cvp_data->option.detect_overlap) {
      overlap_checked = true;
      report_overlapping_bins();
    }
    this->last_sample_success = false;
    this->last_bin_index_hit = 0;
    this->last_bins_hit.clear();
    this->has_last_sampled_value_ = false;
  }

  bool is_collecting() const override
  {
    if (valid_data.use_count() == 0 || cvp_data == nullptr) return false;
    return collect && (cvp_data->parent_cvg == nullptr || cvp_data->parent_cvg->collect);
  }

  bool get_last_sampled_value(T& out) const
  {
    if (!has_last_sampled_value_) return false;
    out = last_sampled_value_;
    return true;
  }

  std::vector<size_t> matching_bin_indices(const T& val) const
  {
    typedef fc4sc_type_traits<T> traits;
    std::vector<size_t> out;
    if (traits::is_nan(val)) return out;
    ensure_bins();
    const T cval = traits::canonicalize(val);

    auto ign_it = get_interval_bs(this->ignore_interval_map, cval);
    if (ign_it != this->ignore_interval_map.end() &&
        traits::in_interval(cval, ign_it->first.first, ign_it->first.second)) {
      return out;
    }

    auto ill_it = get_interval_bs(this->illegal_interval_map, cval);
    if (ill_it != this->illegal_interval_map.end() &&
        traits::in_interval(cval, ill_it->first.first, ill_it->first.second)) {
      return out;
    }

    auto reg_it = get_interval_bs(this->regular_interval_map, cval);
    if (reg_it != this->regular_interval_map.end() &&
        traits::in_interval(cval, reg_it->first.first, reg_it->first.second)) {
      for (const auto& br : reg_it->second) {
        out.push_back(static_cast<size_t>(br.first));
      }
      std::sort(out.begin(), out.end());
      out.erase(std::unique(out.begin(), out.end()), out.end());
    }
    return out;
  }

  bool try_enumerate_bin_values(size_t bin_idx, uint64_t budget, std::vector<T>& out) const
  {
    typedef fc4sc_type_traits<T> traits;
    out.clear();
    ensure_bins();
    if (bin_idx >= bins.size() || budget == 0) return false;
    if (dynamic_cast<const transition_bin_data_model<T>*>(bins[bin_idx].bin_data) != nullptr)
      return false;

    for (const auto& intv : bins[bin_idx].bin_data->intervals) {
      if (traits::equal(intv.first, intv.second)) {
        if (out.size() >= budget) {
          out.clear();
          return false;
        }
        out.push_back(intv.first);
      } else {
        uint64_t dist = 0;
        if (!traits::try_distance_u64(intv.first, intv.second, dist)) {
          out.clear();
          return false;
        }
        if (dist >= budget || out.size() > budget - 1 - dist) {
          out.clear();
          return false;
        }
        traits::enumerate_interval(intv.first, intv.second, out);
      }
    }

    std::sort(out.begin(), out.end(), [](const T& a, const T& b) {
      return traits::less(a, b);
    });
    out.erase(std::unique(out.begin(), out.end(), [](const T& a, const T& b) {
      return traits::equal(a, b);
    }), out.end());
    return true;
  }

  /*!
   *  \brief Computes coverage for this instance. With the strict_sv_options
   *  and merge_instances type options, it is the type coverage unless the
   *  get_inst_coverage option of the covergroup instance is set.
   *  \returns Coverage value as a double between 0 and 100
   */
  double get_inst_coverage() const override
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }

    cvp_data->sync_options_from_parent();
    ensure_bins();
    if (inst_coverage_is_type_coverage(cvp_data->parent_cvg))
      return this->get_coverage();

    if (bins.empty()) // no bins defined
      return (cvp_data->option.weight == 0) ? 100 : 0;

    double res = 0;
    for (auto &bin : bins)
      res += (bin.get_hitcount() >= cvp_data->option.at_least);

    double real = res * 100 / bins.size();

    return (real >= this->cvp_data->option.goal) ? 100 : real;
  }

  /*!
   *  \brief Computes coverage for this instance, or the type coverage as
   *  get_inst_coverage()
   *  \param covered Number of covered bins
   *  \param total Total number of bins in this coverpoint
   *  \returns Coverage value as a double between 0 and 100
   */
  double get_inst_coverage(int &covered, int &total) const override
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }

    cvp_data->sync_options_from_parent();
    ensure_bins();
    if (inst_coverage_is_type_coverage(cvp_data->parent_cvg))
      return this->get_coverage(covered, total);

    double res = 0;
    covered = 0;
    total = bins.size();

    if (!bins.size())
    {
      total = 0;
      return (cvp_data->option.weight == 0) ? 100 : 0;
    }

    for (auto &bin : bins)
      res += (bin.get_hitcount() >= cvp_data->option.at_least);

    covered = res;
    double real = res * 100 / total;
    return (real >= this->cvp_data->option.goal) ? 100 : real;
  }

  /*!
   *  \brief Computes the type coverage of this coverpoint: its coverage
   *  across all the instances of the covergroup type it is in
   *  \returns Coverage value as a double between 0 and 100
   */
  double get_coverage() const override
  {
    int covered = 0;
    int total = 0;
    return this->get_coverage(covered, total);
  }

  /*!
   *  \brief Computes the type coverage of this coverpoint
   *  \param covered Number of covered bins, summed over the instances
   *  \param total Total number of bins, summed over the instances
   *  \returns Coverage value as a double between 0 and 100
   */
  double get_coverage(int &covered, int &total) const override
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    return fc4sc::global::get_cvp_coverage(cvp_data, covered, total);
  }

  /*!
   *  \brief Retrieves the number of hits of the bin with a particular index.
   *  The index order is the same as in the declaration of the coverpoint.
   *  This function will throw an error if the bin count is out of bounds!
   *  \returns the hit counter of a specified bin (by bin index).
   */
  uint64_t get_bin_hit_count(uint32_t bin_index) const
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    if (cvp_data->parent_cvg != nullptr && !cvp_data->in_ensure_bins_)
      cvp_data->sync_options_from_parent();
    ensure_bins();

    if (bin_index >= bins.size()) {// bin index out of bounds
      std::cerr << "FC4SC " << __FUNCTION__ << ": Error! bin_index argument "
          "is out of bounds. Passed value: [" << bin_index << "]" << std::endl
          << "Coverpoint [" << this->cvp_data->name << "] has [" << bins.size()
          << "] bins!" << std::endl;
      return 0;
    }

    return (this->bins[bin_index].get_hitcount());
  }

  /*!
   * \brief Enables sampling
   */
  void start() override
  {
    collect = true;
  }

  /*!
   * \brief Enables sampling
   */
  void stop() override
  {
    collect = false;
  }

  cvp_base_data_model* get_data() override
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }

    return cvp_data;
  }

  const cvp_base_data_model* get_data() const override
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }

    return cvp_data;
  }

  /*!
   * \brief Returns instance name
   */
  std::string& name() override
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }

    return cvp_data->name;
  }

  const std::string& name() const override
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }

    return cvp_data->name;
  }

  /*!
   * \brief Returns instance options
   */
  cvp_option& option() override
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }

    cvp_data->sync_options_from_parent();
    return cvp_data->option;
  }

  const cvp_option& option() const override
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }

    cvp_data->sync_options_from_parent();
    return cvp_data->option;
  }

 /*!
   * \brief Returns coverpoint bin misses
   */
  uint64_t get_misses() override
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }

    return cvp_data->misses;
  }

  /*!
   * \brief Returns coverpoint coverage weight
   */
  uint64_t get_weight() override
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }

    return cvp_data->option.weight;
  }

  /*!
   * \brief Returns a vector of pointers to base_bin for introspection
   */
  std::vector<bin_base*> get_bins_base() override
  {
    if (valid_data.use_count() > 0 && cvp_data != nullptr) {
      if (cvp_data->parent_cvg != nullptr && !cvp_data->in_ensure_bins_)
        cvp_data->sync_options_from_parent();
      ensure_bins();
    }
    std::vector<bin_base*> bin_base_vec;
    for (auto &bin_it : bins)
    {
      bin_base_vec.push_back(&bin_it);
    }
    return bin_base_vec;
  }

  std::vector<const bin_base*> get_bins_base() const override
  {
    if (valid_data.use_count() > 0 && cvp_data != nullptr) {
      if (cvp_data->parent_cvg != nullptr && !cvp_data->in_ensure_bins_)
        cvp_data->sync_options_from_parent();
      ensure_bins();
    }
    std::vector<const bin_base*> bin_base_vec;
    for (const auto& bin_it : bins)
    {
      bin_base_vec.push_back(&bin_it);
    }
    return bin_base_vec;
  }

  /*!
   * \brief Returns a vector of pointers to base_bin for introspection
   */
  std::vector<bin_base*> get_illegal_bins_base() override
  {
    std::vector<bin_base*> bin_base_vec;
    for (auto &bin_it : illegal_bins)
    {
      bin_base_vec.push_back(&bin_it);
    }
    return bin_base_vec;
  }

  std::vector<const bin_base*> get_illegal_bins_base() const override
  {
    std::vector<const bin_base*> bin_base_vec;
    for (const auto& bin_it : illegal_bins)
    {
      bin_base_vec.push_back(&bin_it);
    }
    return bin_base_vec;
  }

  /*!
   * \brief Returns a vector of pointers to base_bin for introspection
   */
  std::vector<bin_base*> get_ignore_bins_base() override
  {
    std::vector<bin_base*> bin_base_vec;
    for (auto &bin_it : ignore_bins)
    {
      bin_base_vec.push_back(&bin_it);
    }
    return bin_base_vec;
  }

  std::vector<const bin_base*> get_ignore_bins_base() const override
  {
    std::vector<const bin_base*> bin_base_vec;
    for (const auto& bin_it : ignore_bins)
    {
      bin_base_vec.push_back(&bin_it);
    }
    return bin_base_vec;
  }

  /*!
   * \brief Returns a vector of pointers to base_bin for introspection
   */
  std::vector<bin_base*> get_default_bins_base() override
  {
    std::vector<bin_base*> bin_base_vec;
    for (auto &bin_it : default_bins)
    {
      bin_base_vec.push_back(&bin_it);
    }
    return bin_base_vec;
  }

  std::vector<const bin_base*> get_default_bins_base() const override
  {
    std::vector<const bin_base*> bin_base_vec;
    for (const auto& bin_it : default_bins)
    {
      bin_base_vec.push_back(&bin_it);
    }
    return bin_base_vec;
  }

  std::vector<interval_t<T>> get_uncovered_intervals() const
  {
    if (valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    if (cvp_data->parent_cvg != nullptr && !cvp_data->in_ensure_bins_)
      cvp_data->sync_options_from_parent();
    ensure_bins();
    const unsigned int at_least = cvp_data->option.at_least;
    if (at_least == 0)
      return std::vector<interval_t<T>>();

    std::vector<interval_t<T>> raw_excl;
    for (const auto& ib : ignore_bins) {
      if (ib.bin_data != nullptr && !ib.bin_data->is_transition_bin())
        raw_excl.insert(raw_excl.end(), ib.bin_data->intervals.begin(), ib.bin_data->intervals.end());
    }
    for (const auto& ib : illegal_bins) {
      if (ib.bin_data != nullptr && !ib.bin_data->is_transition_bin())
        raw_excl.insert(raw_excl.end(), ib.bin_data->intervals.begin(), ib.bin_data->intervals.end());
    }
    auto excl_norm = detail::normalize_uncovered_intervals<T>(raw_excl, false);

    std::vector<interval_t<T>> raw_uncovered;
    for (const auto& b : bins) {
      if (b.bin_data == nullptr || b.bin_data->bin_type != bin_t::default_) continue;
      if (b.bin_data->is_transition_bin()) continue;
      if (b.get_hitcount() >= at_least) continue;
      raw_uncovered.insert(raw_uncovered.end(), b.bin_data->intervals.begin(), b.bin_data->intervals.end());
    }
    return detail::subtract_excluded_intervals<T>(raw_uncovered, excl_norm, true);
  }

  std::vector<interval_t<T>> get_uncovered_intervals(size_t bin_index) const
  {
    if (valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    if (cvp_data->parent_cvg != nullptr && !cvp_data->in_ensure_bins_)
      cvp_data->sync_options_from_parent();
    ensure_bins();
    if (bin_index >= bins.size())
      throw std::out_of_range("coverpoint::get_uncovered_intervals: bin_index out of range");

    const unsigned int at_least = cvp_data->option.at_least;
    if (at_least == 0)
      return std::vector<interval_t<T>>();
    const auto& b = bins[bin_index];
    if (b.bin_data == nullptr || b.bin_data->bin_type != bin_t::default_ || b.bin_data->is_transition_bin())
      return std::vector<interval_t<T>>();
    if (b.get_hitcount() >= at_least)
      return std::vector<interval_t<T>>();

    std::vector<interval_t<T>> raw_excl;
    for (const auto& ib : ignore_bins) {
      if (ib.bin_data != nullptr && !ib.bin_data->is_transition_bin())
        raw_excl.insert(raw_excl.end(), ib.bin_data->intervals.begin(), ib.bin_data->intervals.end());
    }
    for (const auto& ib : illegal_bins) {
      if (ib.bin_data != nullptr && !ib.bin_data->is_transition_bin())
        raw_excl.insert(raw_excl.end(), ib.bin_data->intervals.begin(), ib.bin_data->intervals.end());
    }
    auto excl_norm = detail::normalize_uncovered_intervals<T>(raw_excl, false);
    return detail::subtract_excluded_intervals<T>(b.bin_data->intervals, excl_norm, true);
  }

  std::vector<interval_t<std::string>> get_uncovered_intervals_str() const override
  {
    typedef fc4sc_type_traits<T> traits;
    auto ivs = get_uncovered_intervals();
    std::vector<interval_t<std::string>> out;
    out.reserve(ivs.size());
    for (const auto& iv : ivs) {
      out.push_back(interval_t<std::string>(traits::to_string(iv.first), traits::to_string(iv.second)));
    }
    return out;
  }

  std::vector<interval_t<int>> get_uncovered_intervals_to_int() const override
  {
    typedef fc4sc_type_traits<T> traits;
    auto ivs = get_uncovered_intervals();
    std::vector<interval_t<int>> out;
    out.reserve(ivs.size());
    for (const auto& iv : ivs) {
      out.push_back(interval_t<int>(traits::to_int(iv.first), traits::to_int(iv.second)));
    }
    return out;
  }

  std::vector<uncovered_bin_info> get_uncovered_bin_infos() const override
  {
    if (valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    if (cvp_data->parent_cvg != nullptr && !cvp_data->in_ensure_bins_)
      cvp_data->sync_options_from_parent();
    ensure_bins();

    std::vector<uncovered_bin_info> out;
    const unsigned int at_least = cvp_data->option.at_least;
    if (at_least == 0)
      return out;

    for (size_t i = 0; i < bins.size(); ++i) {
      const auto& b = bins[i];
      if (b.bin_data == nullptr || b.bin_data->bin_type != bin_t::default_) continue;
      const uint64_t hits = b.get_hitcount();
      if (hits >= at_least) continue;
      uncovered_bin_info info;
      info.name = b.bin_data->name;
      info.index = i;
      info.hits = hits;
      info.at_least = at_least;
      info.is_cross = false;
      info.is_auto = !has_explicit_bins_;
      info.is_transition = b.bin_data->is_transition_bin();
      out.push_back(std::move(info));
    }
    return out;
  }

  std::vector<std::string> get_uncovered_bins() const override
  {
    auto infos = get_uncovered_bin_infos();
    std::vector<std::string> names;
    names.reserve(infos.size());
    for (const auto& info : infos) {
      names.push_back(info.name);
    }
    return names;
  }

  std::string& get_sample_expression_str() override
  {
    return cvp_data->sample_expression_str;
  }

  std::string& get_sample_condition_str()
  {
    return cvp_data->sample_condition_str;
  }

};

/*!
 * \brief Defines a class to manage coverpoints in
 * dynamically defined covergroup
 * \tparam T Type of sample expression
 * \tparam U Type of sample condition expression
 */
template<typename T, typename U>
class dynamic_coverpoint_factory {

typedef typename std::function<T>::result_type ret_type;
typedef typename std::function<U>::result_type cond_ret_type;

friend class dynamic_covergroup_factory;

static_assert(std::is_same<bool,cond_ret_type>::value,"sample condition expression must return bool type");
  std::function<T> sample_expression;
  std::function<U> sample_condition;

  coverpoint<ret_type>* cvp_fact;

public:

  coverpoint<ret_type>* get_cvp_fact() const
  {
    return cvp_fact;
  }

  /*!
   * \brief binds variables to sample expression
   * \param Args must be same type as args for sample
   * expression
   */
  template<typename ... Args>
  void bind_sample(cvg_base& cvg, Args&... args)
  {
    static_assert(std::is_same<T,ret_type(Args...)>::value,"Argument variables must match coverpoint sample expression");
    cvp_base& cp_base = cvg.get_coverpoint(cvp_fact->cvp_data->name);
    coverpoint<ret_type>* cvp = dynamic_cast< coverpoint<ret_type>* >(&cp_base);
    if( cvp->sample_expression ) 
      throw ("Attempted to bind sample expression that is already binded in coverpoint " + cvp->cvp_data->name + " in covergroup " + cvg.name());
    cvp->sample_expression = std::bind(sample_expression,std::ref(args)...);
    if(!cvg.is_enabled()) std::cerr << "Warning: called bind_sample on disabled covergroup " << cvg.name() << "\n";
  }

  /*!
   * \brief binds variables to sample condition expression
   * \param Args must be same type as args for sample
   * condition expression
   */
  template<typename ... Args>
  void bind_condition(cvg_base& cvg, Args&... args)
  {
    static_assert(std::is_same<U,bool(Args...)>::value,"Argument variables must match coverpoint sample condition expression");
    cvp_base& cp_base = cvg.get_coverpoint(cvp_fact->cvp_data->name);
    coverpoint<ret_type>* cvp = dynamic_cast< coverpoint<ret_type>* >(&cp_base);
    if( cvp->sample_condition ) 
      throw ("Attempted to bind sample condition that is already binded in coverpoint " + cvp->name() + " in covergroup " + cvg.name());
    cvp->sample_condition = std::bind(sample_condition,std::ref(args)...);
    if(!cvg.is_enabled()) std::cerr << "Warning: called bind_condition on disabled covergroup " << cvg.name() << "\n";

  }

  /*!
   *  \brief Takes the bin name. Uses bin constructor
   *  \param bin_name Name of the bin
   *  \param args Rest of arguments
   */
  template<typename ... Args>
   void create_bin(const std::string &bin_name, Args... args)
  {
    bin<ret_type>(bin_name,args...).add_to_cvp(*cvp_fact);
  }

 /*!
   *  \brief Takes the bin name. Uses bin constructor
   *  \param bin_name Name of the bin
   *  \param args Rest of arguments
   */
  template<typename ... Args>
   void create_illegal_bin(const std::string &bin_name, Args... args)
  {
    illegal_bin<ret_type>(bin_name,args...).add_to_cvp(*cvp_fact);
  }

  /*!
   *  \brief Takes the bin name. Uses bin constructor
   *  \param bin_name Name of the bin
   *  \param args Rest of arguments
   */
  template<typename ... Args>
   void create_ignore_bin(const std::string &bin_name, Args... args)
  {
    ignore_bin<ret_type>(bin_name,args...).add_to_cvp(*cvp_fact);
  }

  /*!
   * \brief Constructs an bin_array which will split an interval into multiple
   * equal parts. The number of sub-intervals is specified via the count argument
   */
  void create_bin_array(const std::string &name, uint64_t count, interval_t<ret_type> interval)
  {
    bin_array<ret_type>(name,count,interval).add_to_cvp(*cvp_fact);
  }

  /*!
   * \brief Constructs an bin_array from a vector of intervals where each
   * interval will be nested by a bin
   */
  void  create_bin_array(const std::string &name, std::vector<interval_t<ret_type>>&& intvs)
  {
    bin_array<ret_type>(name,intvs).add_to_cvp(*cvp_fact);
  }

  /*!
   * \brief Constructs an bin_array from a vector of values where each
   * values will be nested by a bin
   */
  void create_bin_array(const std::string &name, const std::vector<ret_type>& intvs)
  {
    bin_array<ret_type>(name,intvs).add_to_cvp(*cvp_fact);
  }

  /*!
   *  \brief Creates a transition bin, see \link fc4sc::transition_bin \endlink
   *  \param bin_name Name of the bin
   *  \param args Transitions of the bin, see \link fc4sc::trans \endlink
   *
   *    The SystemVerilog "bins b = (1 => 2), (3 => 4);" is written
   *  create_transition_bin("b", trans<int>(1, 2), trans<int>(3, 4)). Each
   *  instance of the covergroup has its own history of sampled values.
   */
  template<typename ... Args>
  void create_transition_bin(const std::string &bin_name, Args... args)
  {
    transition_bin<ret_type>(bin_name,args...).add_to_cvp(*cvp_fact);
  }

  /*!
   *  \brief Creates a transition bin array, with a bin for each value
   *  sequence, see \link fc4sc::transition_bin_array \endlink
   */
  template<typename ... Args>
  void create_transition_bin_array(const std::string &bin_name, Args... args)
  {
    transition_bin_array<ret_type>(bin_name,args...).add_to_cvp(*cvp_fact);
  }

  /*!
   *  \brief Creates an illegal transition bin, see
   *  \link fc4sc::illegal_transition_bin \endlink
   */
  template<typename ... Args>
  void create_illegal_transition_bin(const std::string &bin_name, Args... args)
  {
    illegal_transition_bin<ret_type>(bin_name,args...).add_to_cvp(*cvp_fact);
  }

  /*!
   *  \brief Creates an ignore transition bin, see
   *  \link fc4sc::ignore_transition_bin \endlink
   */
  template<typename ... Args>
  void create_ignore_transition_bin(const std::string &bin_name, Args... args)
  {
    ignore_transition_bin<ret_type>(bin_name,args...).add_to_cvp(*cvp_fact);
  }

};

} // namespace fc4sc

#endif /* FC4SC_COVERPOINT_HPP */
