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

  uint64_t size() const {
    return bins_data.size();
  }

  std::string& get_sample_expression_str()
  {
    return sample_expression_str;
  }

  std::string& get_sample_condition_str()
  {
    return sample_condition_str;
  }

  void accept_visitor(covVisitorBase& visitor)
  {
    visitor.visit(*this);
  }

};

template <class T>
class coverpoint final : public coverpoint_base
{
private:
  // static check to make sure that the coverpoint is templated by an integral type
  static_assert(std::is_integral<T>::value, "Type must be integral!");
  /*
   * The covergroup class needs access to private members of the coverpoint
   * in order to set the sample condition, expression and name.
   */
  friend class covergroup;
  // binsof selects bins of the coverpoint for the bins of a cross
  friend class binsof<T>;
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
      return a.second < b.second;
    }
  };

  typedef std::map<fc4sc::interval_t<T>,std::vector<bin_range_t>,IntervalComp> interval_map_t;
  typedef typename std::map<fc4sc::interval_t<T>,std::vector<bin_range_t>,IntervalComp>::iterator interval_map_iterator_t;

  bool has_sample_expression = false;

  /*! Pointer to this object's data */
  coverpoint_data_model* cvp_data = new coverpoint_data_model;

  /*! For ensuring the object's data has not been deallocated */
  std::weak_ptr<bool> valid_data = cvp_data->valid;

  /*! Expression used to sample the data */
  std::function<T()> sample_expression;

  /*!
   *  \brief Create a dynamic copy of this coverpoint with matching bin types
   */
  cvp_base* create_instance(cvg_base* cvg_inst)
  {
    coverpoint<T>* cvp = new coverpoint<T>;
    cvp->cvp_data->name = this->name();
    cvp->cvp_data->option = this->option();
    cvp->cvp_data->sample_expression_str = this->get_sample_expression_str();
    cvp->cvp_data->sample_condition_str = this->get_sample_condition_str();
    for(auto& bin_it : this->bins)
    {
      auto new_bin_data = new bin_data_model<T>;
      cvp->bins.push_back(bin_it);
      cvp->bins.back().bin_data = new_bin_data;
      cvp->cvp_data->bins_data.push_back(new_bin_data);
      *new_bin_data = *(bin_it.bin_data);
      std::fill(new_bin_data->interval_hits.begin(),new_bin_data->interval_hits.end(),0);
    }
    for(auto& bin_it : this->illegal_bins)
    {
      auto new_bin_data = new bin_data_model<T>;
      cvp->illegal_bins.push_back(bin_it);
      cvp->illegal_bins.back().bin_data = new_bin_data;
      *new_bin_data = *(bin_it.bin_data);
      cvp->cvp_data->illegal_bins_data.push_back(new_bin_data);
      std::fill(new_bin_data->interval_hits.begin(),new_bin_data->interval_hits.end(),0);
    }
    for(auto& bin_it : this->ignore_bins)
    {
      auto new_bin_data = new bin_data_model<T>;
      cvp->ignore_bins.push_back(bin_it);
      cvp->ignore_bins.back().bin_data = new_bin_data;
      *new_bin_data = *(bin_it.bin_data);
      cvp->cvp_data->ignore_bins_data.push_back(new_bin_data);
      std::fill(new_bin_data->interval_hits.begin(),new_bin_data->interval_hits.end(),0);
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
    
    cvp->regular_interval_map = this->regular_interval_map;
    cvp->illegal_interval_map = this->illegal_interval_map;
    cvp->ignore_interval_map = this->ignore_interval_map;
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
    if (default_bins.empty()) return;

    std::vector<fc4sc::interval_t<T>> used;
    for (auto &bin_it : bins)
      used.insert(used.end(), bin_it.bin_data->intervals.begin(), bin_it.bin_data->intervals.end());
    for (auto &bin_it : illegal_bins)
      used.insert(used.end(), bin_it.bin_data->intervals.begin(), bin_it.bin_data->intervals.end());
    for (auto &bin_it : ignore_bins)
      used.insert(used.end(), bin_it.bin_data->intervals.begin(), bin_it.bin_data->intervals.end());
    std::sort(used.begin(), used.end());

    // the values between the used intervals
    std::vector<fc4sc::interval_t<T>> values;
    T next = std::numeric_limits<T>::min();
    bool past_max = false;
    for (auto &intv : used)
    {
      if (intv.second < next) continue;
      if (intv.first > next)
        values.push_back(fc4sc::interval<T>(next, static_cast<T>(intv.first - 1)));
      if (intv.second == std::numeric_limits<T>::max()) {
        past_max = true;
        break;
      }
      next = static_cast<T>(intv.second + 1);
    }
    if (!past_max)
      values.push_back(fc4sc::interval<T>(next, std::numeric_limits<T>::max()));

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
  interval_map_iterator_t slice_intervals_map(interval_map_iterator_t hint_it, interval_map_t& interval_map, fc4sc::interval_t<T> range)
  {
    T start_cut = range.first;
    T end_cut = range.second;
    auto map_it = get_interval_bs(interval_map,end_cut);
    if(map_it != interval_map.end()) {
      auto it = map_it->first;
      if(end_cut > it.first && end_cut < it.second){
        map_it = slice_helper(interval_map,map_it,fc4sc::interval<T>(end_cut+1,it.second),fc4sc::interval<T>(it.first,end_cut));
      }
      else if(end_cut == it.first && it.first != it.second) {
        map_it = slice_helper(interval_map,map_it,fc4sc::interval<T>(it.first+1,it.second),fc4sc::interval<T>(end_cut,end_cut));
      }
    }
    map_it = get_interval_bs(interval_map,start_cut);
    if(map_it != interval_map.end()) {
      auto it = map_it->first;
      if(start_cut > it.first && start_cut < it.second ) {
        map_it = slice_helper(interval_map,map_it,fc4sc::interval<T>(it.first,start_cut-1),fc4sc::interval<T>(start_cut,it.second));
      }
      else if(start_cut == it.second && it.first != it.second) {
        map_it = slice_helper(interval_map,map_it,fc4sc::interval<T>(it.first,start_cut-1),fc4sc::interval<T>(start_cut,start_cut));
      }
    }
    return map_it;
  }

  /*!
   *  \brief Helper for adding new ranges to the flat map representation of the coverpoint
   */
  interval_map_iterator_t add_range_map(interval_map_iterator_t hint_it,interval_map_t& interval_map,bin_range_t bin_range_key,fc4sc::interval_t<T> range)
  {
    // hint_it is end() when the range is after all the intervals: nothing
    // is to be read from it, and the loop below appends the range
    if(hint_it != interval_map.end() && range.second < hint_it->first.first) {
      std::vector<bin_range_t> bin_range_vec{bin_range_key};
      return std::next( interval_map.insert(hint_it,{range, bin_range_vec }) );
    }
    for(auto map_it = hint_it; map_it != interval_map.end(); map_it++)
    {
      auto it = map_it->first;
      if(range.first > range.second) {
        return map_it;
      }
      if(range.first <= it.first && range.second >= it.second) { 
	map_it->second.push_back(bin_range_key);
      }
      if(range.first < it.first) {
        std::vector<bin_range_t> bin_range_vec{bin_range_key};
        map_it = std::next(interval_map.insert(map_it, {fc4sc::interval<T>(range.first,it.first-1), bin_range_vec } ));
      }
      if(it.second + 1 > range.first)
        range.first = it.second + 1;
    }
    std::vector<bin_range_t> bin_range_vec{bin_range_key};
    interval_map.insert(interval_map.end(), {range, bin_range_vec } );
    return interval_map.end();
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
    // sample, and no transition goes on past an ignored value
    auto step_patterns = [&](bin_data_model<T>* bin_data) {
      auto data = static_cast<transition_bin_data_model<T>*>(bin_data);
      if (data->patterns.empty()) return;
      if (ignored)
        data->reset_patterns();
      else
        data->step_patterns(cvp_val, trans_samples);
    };
    for (auto index : this->trans_ignore_bins)
      step_patterns(this->ignore_bins[index].bin_data);
    for (auto index : this->trans_illegal_bins)
      step_patterns(this->illegal_bins[index].bin_data);
    for (auto index : this->trans_bins)
      step_patterns(this->bins[index].bin_data);
    if (ignored) return;

    // the lengths of the last values which are an ignored sequence
    std::vector<size_t> ignored_lengths;
    for (auto index : this->trans_ignore_bins)
    {
      auto data = static_cast<transition_bin_data_model<T>*>(this->ignore_bins[index].bin_data);
      data->sample(trans_history, trans_clean, nullptr, &ignored_lengths);
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

    bool trans_hit = !ignored_lengths.empty();
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
    // The options are plain fields which are set after the bins are
    // defined, so option.detect_overlap is looked at when sampling: the
    // check is made once, on the first sample after the option is set,
    // even if sampling is stopped.
    if (!overlap_checked && cvp_data->option.detect_overlap) {
      overlap_checked = true;
      report_overlapping_bins();
    }
    if (!collect) return;
    this->last_sample_success = false;
    this->last_bins_hit.clear();

    // 1) Search if the value is in the ignore bins
    bool ignored = false;
    auto range_it  = get_interval_bs(this->ignore_interval_map,cvp_val);
    if(range_it != this->ignore_interval_map.end()) {
      for(auto bin_range_it : range_it->second)
      {
        if (this->ignore_bins[bin_range_it.first].sample(cvp_val,bin_range_it.second)) {
          ignored = true;
          break;
        }
      }
    }

    // 2) Search if the value is in the illegal bins
    if (!ignored) {
      range_it  = get_interval_bs(this->illegal_interval_map,cvp_val);
      if(range_it != this->illegal_interval_map.end()) {
        for(auto bin_range_it : range_it->second)
        {
          try { this->illegal_bins[bin_range_it.first].sample(cvp_val,bin_range_it.second); }
          catch (illegal_bin_sample_exception &e) {
            // no transition with an illegal value in it is hit either
            sample_transitions(cvp_val, true);
            e.update_cvp_info(this->cvp_data->name);
            throw e;
          }
        }
      }
    }

    // 3) Sample the transition bins
    sample_transitions(cvp_val, ignored);

    if (ignored) {
      cvp_data->misses++;
      return;
    }

    // Sample the default bins, which hold the values in no other bin
    for (auto &bin_it : this->default_bins)
      bin_it.sample_default(cvp_val);

    // 4) Sample regular bins    
    range_it  = get_interval_bs(this->regular_interval_map,cvp_val);
    if(range_it != this->regular_interval_map.end()) {
      for(auto bin_range_it : range_it->second)
      {
        if (this->bins[bin_range_it.first].sample(cvp_val,bin_range_it.second)) {
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

  /*! Default constructor */
  coverpoint() { }

  /*!
   *  \brief Constructor that registers a new default bin
   */
  template <typename... Args>
  coverpoint(bin<T> n, Args... args) : coverpoint(args...)
  {
    if (!n.is_empty())
    {
      std::reverse(n.bin_data->intervals.begin(), n.bin_data->intervals.end());
      //bins.push_back(n);
      n.add_to_cvp(*this);
    }
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
    if (!n.is_empty())
    {
      std::reverse(n.bin_data->intervals.begin(), n.bin_data->intervals.end());
      //illegal_bins.push_back(n);
      n.add_to_cvp(*this);
    }
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
    if (!n.is_empty())
    {
      std::reverse(n.bin_data->intervals.begin(), n.bin_data->intervals.end());
      //ignore_bins.push_back(n);
      n.add_to_cvp(*this);
    }
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
  coverpoint(const coverpoint<T>& rh) = default;
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

    this->cvp_data->bins_data = rh.cvp_data->bins_data;
    this->cvp_data->illegal_bins_data = rh.cvp_data->illegal_bins_data;
    this->cvp_data->ignore_bins_data = rh.cvp_data->ignore_bins_data;
    this->cvp_data->default_bins_data = rh.cvp_data->default_bins_data;
    rh.cvp_data->bins_data.clear();
    rh.cvp_data->illegal_bins_data.clear();
    rh.cvp_data->ignore_bins_data.clear();
    rh.cvp_data->default_bins_data.clear();
    delete rh.cvp_data;
    return *this;
  }

  ~coverpoint() = default;

  /*!
   * \brief Initializer list constructor that receives a list of bin (of any types,
   * even mixed) and registers all the bins in this coverpoint.
   */
  coverpoint(std::initializer_list<const bin_wrapper<T>> list) {
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

  /*!
   *  Retrieves the number of regular bins
   */
  uint64_t size() const {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    return bins.size();
  }

  void sample() 
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
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
      }
    }
    else {
      this->sample(*sample_point);
    }
  }

  /*!
   *  \brief Computes coverage for this instance
   *  \returns Coverage value as a double between 0 and 100
   */
  double get_inst_coverage() const 
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }

    if (bins.empty()) // no bins defined
      return (cvp_data->option.weight == 0) ? 100 : 0;

    double res = 0;
    for (auto &bin : bins)
      res += (bin.get_hitcount() >= cvp_data->option.at_least);

    double real = res * 100 / bins.size();

    return (real >= this->cvp_data->option.goal) ? 100 : real;
  }

  /*!
   *  \brief Computes coverage for this instance
   *  \param covered Number of covered bins
   *  \param total Total number of bins in this coverpoint
   *  \returns Coverage value as a double between 0 and 100
   */
  double get_inst_coverage(int &covered, int &total) const 
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }

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
  double get_coverage() const
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
  double get_coverage(int &covered, int &total) const
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
  void start()
  {
    collect = true;
  }

  /*!
   * \brief Enables sampling
   */
  void stop()
  {
    collect = false;
  }

  cvp_base_data_model* get_data()
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
  std::string& name() 
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
  cvp_option& option() 
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }

    return cvp_data->option;
  }

 /*!
   * \brief Returns coverpoint bin misses
   */
  uint64_t get_misses()
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
  uint64_t get_weight()
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
  std::vector<bin_base*> get_bins_base()
  {
    std::vector<bin_base*> bin_base_vec;
    for (auto &bin_it : bins)
    {
      bin_base_vec.push_back(&bin_it);
    }
    return bin_base_vec;
  }

  /*!
   * \brief Returns a vector of pointers to base_bin for introspection
   */
  std::vector<bin_base*> get_illegal_bins_base()
  {
    std::vector<bin_base*> bin_base_vec;
    for (auto &bin_it : illegal_bins)
    {
      bin_base_vec.push_back(&bin_it);
    }
    return bin_base_vec;
  }

  /*!
   * \brief Returns a vector of pointers to base_bin for introspection
   */
  std::vector<bin_base*> get_ignore_bins_base()
  {
    std::vector<bin_base*> bin_base_vec;
    for (auto &bin_it : ignore_bins)
    {
      bin_base_vec.push_back(&bin_it);
    }
    return bin_base_vec;
  }

  /*!
   * \brief Returns a vector of pointers to base_bin for introspection
   */
  std::vector<bin_base*> get_default_bins_base()
  {
    std::vector<bin_base*> bin_base_vec;
    for (auto &bin_it : default_bins)
    {
      bin_base_vec.push_back(&bin_it);
    }
    return bin_base_vec;
  }

  std::string& get_sample_expression_str()
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

};

} // namespace fc4sc

#endif /* FC4SC_COVERPOINT_HPP */
