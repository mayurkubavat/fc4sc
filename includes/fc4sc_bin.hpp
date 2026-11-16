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
 \file bin.hpp
 \brief Template functions for bin implementations

 This file contains the template implementation of
 bins and illegal bins
 */

#ifndef FC4SC_BIN_HPP
#define FC4SC_BIN_HPP

#include <iostream>
#include <deque>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <algorithm>
#include <vector>
#include <sstream>
#include <memory>

#include "fc4sc_base.hpp"

namespace fc4sc
{
// forward declarations
template <typename T> class coverpoint;
template <typename T> class binsof;
template <typename T> class bin;

template <typename T>
static std::vector<interval_t<T>> reunion(const bin<T>& lhs, const bin<T>& rhs);

template <typename T>
static std::vector<interval_t<T>> reunion(const bin<T>& lhs, const std::vector<interval_t<T>>& rhs);

template <typename T>
static std::vector<interval_t<T>> intersection(const bin<T>& lhs, const bin<T>& rhs);

template <typename T>
static std::vector<interval_t<T>> intersection(const bin<T>& lhs, const std::vector<interval_t<T>>& rhs);

/*!
 * \brief Defines a class for bin data model
 * \tparam T Type of values in this bin
 */
template <class T>
class bin_data_model : public bin_base_data_model
{
public:

  // the type of bin (default/ignore/illegal)
  bin_t bin_type;

  /*! Storage for hit counts corresponding to intervals */
  std::vector<uint64_t> interval_hits;

  /*! Storage for the values. All are converted to intervals */
  std::vector<interval_t<T>> intervals;

  /*! Name of the bin */
  std::string name;

  // the type of bin (default/ignore/illegal)
  //bin_t bin_type;
  bin_t& get_bin_type()
  {
    return bin_type;
  }

  /*! Storage for hit counts corresponding to intervals */
  //std::vector<uint64_t> interval_hits;
  std::vector<uint64_t>& get_interval_hits()
  {
    return interval_hits;
  }

  /*! Name of the bin */
  //std::string name;
  std::string& get_name()
  {
    return name;
  }

  /* function to introspect the bin's intervals */
  std::vector<interval_t<int>> get_intervals_to_int() const
  {
    std::vector<interval_t<int>> intervals_int;
    for(auto inter_it : intervals)
    {
      intervals_int.push_back({static_cast<int>(inter_it.first),static_cast<int>(inter_it.second)});
    }
    return intervals_int;
  }

  void accept_visitor(covVisitorBase& visitor)
  {
    visitor.visit(*this);
  }

};

/*!
 * \brief Defines a class for default bins
 * \tparam T Type of values in this bin
 */
template <class T>
class bin : public bin_base
{
private:
  //all bin types required to be integral since floating point bins result in ambiguous intervals for bins
  static_assert(std::is_integral<T>::value, "Type must be integral!");
  friend binsof<T>;
  friend coverpoint<T>;

  // These constructors are private, making the only public constructor be
  // the one which receives a name as the first argument. This forces the user
  // to give a name for each instantiated bin.

  /*!
   *  \brief Takes a value and wraps it in a pair
   *  \param value Value associated with the bin
   *  \param args Rest of arguments
   */
  template <typename... Args>
  bin(T value, Args... args) noexcept : bin(args...) {
    bin_data->intervals.push_back(interval(value, value));
    bin_data->interval_hits.push_back(0);
  }

  /*!
   *  \brief Adds a new intervals to the bin
   *  \param intervals New vector of intervals associated with the bin
   *  \param args Rest of arguments
   */
  template <typename... Args>
  bin(std::vector<interval_t<T>> intervals_vec, Args... args) noexcept : bin(args...) {
    for(auto interval : intervals_vec)
    {
      bin_data->intervals.push_back(interval);
      bin_data->interval_hits.push_back(0);
      if (bin_data->intervals.back().first > bin_data->intervals.back().second) {
        std::swap(bin_data->intervals.back().first, bin_data->intervals.back().second);
      }
    }
  }

  /*!
   *  \brief Adds a new interval to the bin
   *  \param interval New interval associated with the bin
   *  \param args Rest of arguments
   */
  template <typename... Args>
  bin(interval_t<T> interval, Args... args) noexcept : bin(args...) {
    bin_data->intervals.push_back(interval);
    bin_data->interval_hits.push_back(0);
    if (bin_data->intervals.back().first > bin_data->intervals.back().second) {
      std::swap(bin_data->intervals.back().first, bin_data->intervals.back().second);
    }
  }

protected:
  /*! Default Constructor */
  bin() { }

  /*! Pointer to this object's data */
  bin_data_model<T>* bin_data = new bin_data_model<T>;

  /*! For ensuring the object's data has not been deallocated */
  std::weak_ptr<bool> valid_data = bin_data->valid;

  // the type of bin (default/ignore/illegal)
  //bin_t bin_type;
  bin_t& bin_type()
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    return bin_data->bin_type;
  }

  /*! Storage for hit counts corresponding to intervals */
  //std::vector<uint64_t> interval_hits;
  std::vector<uint64_t>& interval_hits()
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    return bin_data->interval_hits;
  }

  /*! Storage for the values. All are converted to intervals */
  //std::vector<interval_t<T>> intervals;
  std::vector<interval_t<T>>& intervals()
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    return bin_data->intervals;
  }

  /*! Name of the bin */
  //std::string name;
  std::string& name()
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    return bin_data->name;
  }

  /*!
   *  \brief Comparator class for sorting vector of intervals
   */
  struct IntervalComp {
    bool operator() (const fc4sc::interval_t<T>& a, const fc4sc::interval_t<T>& b) const {
      return a.first < b.first;
    }
  };

  /*!
   *  \brief Checks and merges overlapping intervals
   */
  void remove_interval_overlap()
  {
    sort(bin_data->intervals.begin(),bin_data->intervals.end(),IntervalComp());
    auto intrv_end = bin_data->intervals.begin();
    for (auto intrv = std::next(intrv_end); intrv != bin_data->intervals.end(); intrv++)
    {
      if(intrv_end->second >= intrv->first) {
        intrv_end->second = intrv->second;
	std::cerr << "Warning: Merging overlapping intervals "
	          << "("  << intrv_end->first << "," << intrv_end->second << ")"
		  << " and "
		  << "(" << intrv->first << "," << intrv->second << ")"
	          << " within bin \"" << this->bin_data->name << "\"\n";
      }
      else {
        *(++intrv_end) = *intrv;
      }
    }
    bin_data->intervals.erase(++intrv_end,bin_data->intervals.end());
  }

public:

  /* function to introspect the bin's intervals */
  std::vector<interval_t<int>> get_intervals_to_int() const
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    std::vector<interval_t<int>> intervals_int;
    for(auto inter_it : bin_data->intervals)
    {
      intervals_int.push_back({static_cast<int>(inter_it.first),static_cast<int>(inter_it.second)});
    }
    return intervals_int;
  }

  /* Virtual function used to register this bin inside a coverpoint */
  virtual void add_to_cvp(coverpoint<T> &cvp)
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    remove_interval_overlap();
    cvp.insert_intervals(cvp.regular_interval_map,*this,cvp.bins.size());
    cvp.bins.push_back(*this);
    cvp.cvp_data->bins_data.push_back(this->bin_data);
  }

  /*!
   *  \brief Takes the bin name
   *  \param bin_name Name of the bin
   *  \param args Rest of arguments
   */
  template<typename ...Args>
  explicit bin(const std::string &bin_name, Args... args) noexcept : bin(args...) {
    static_assert(forbid_type<std::string, Args...>::value, "Bin constructor accepts only 1 name argument!");
    this->bin_data->name = bin_name;
    this->bin_data->bin_type = bin_t::default_;
  }

  /*!
   *  \brief Takes the bin name
   *  \param bin_name Name of the bin
   *  \param args Rest of arguments
   */
  template<typename ...Args>
  explicit bin(const char *bin_name, Args... args) noexcept : bin(std::string(bin_name), args...) {}

  /*! Default Destructor */
  virtual ~bin() = default;

  uint64_t get_hitcount() const
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    uint64_t hitsum = 0;
    for (auto hitcount : bin_data->interval_hits)
      hitsum += hitcount;
    return hitsum;
  }

  /*!
   * \brief Samples the given value and interval index and increments hit counts
   * \param val Current sampled value
   * \param interval index
   */
  uint64_t sample(const T &val, unsigned int interval_index)
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    if(val >= this->bin_data->intervals[interval_index].first && val <= this->bin_data->intervals[interval_index].second) {
      this->bin_data->interval_hits[interval_index]++;
      return 1;
    }
    else {
      return 0;
    }
  }

  /*!
   * \brief Finds if the given value is contained in the bin
   * \param val Value to search for
   * \returns true if found, false otherwise
   */
  bool contains(const T &val) const
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    for (size_t i = 0; i < bin_data->intervals.size(); ++i)
      if (bin_data->intervals[i].first <= val && bin_data->intervals[i].second >= val)
        return true;

    return false;
  }

  bool is_empty() const
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    return bin_data->intervals.empty();
  }

  friend std::vector<interval_t<T>> reunion<T>(const bin<T>& lhs, const std::vector<interval_t<T>>& rhs);
  friend std::vector<interval_t<T>> intersection<T>(const bin<T>& lhs, const std::vector<interval_t<T>>& rhs);
  friend std::vector<interval_t<T>> reunion<T>(const bin<T>& lhs, const bin<T>& rhs);
  friend std::vector<interval_t<T>> intersection<T>(const bin<T>& lhs, const bin<T>& rhs);
};

/*!
 * \brief Defines a class for illegal bins
 * \tparam T Type of values in this bin
 */
template <class T>
class illegal_bin final : public bin<T>
{
  static_assert(std::is_integral<T>::value, "Type must be integral!");
  friend class coverpoint<T>;
protected:
  
  illegal_bin() = delete;

public:
  /*!
   *  \brief Forward to parent constructor
   */
  template <typename... Args>
  explicit illegal_bin(Args... args) : bin<T>::bin(args...) {
    this->bin_data->bin_type = bin_t::illegal_;
  }

  virtual ~illegal_bin() = default;

  /*!
   * \brief Samples the given value and interval index and increments hit counts
   * \param val Current sampled value
   * \param interval index
   */
  uint64_t sample(const T &val, unsigned int interval_index)
  {
    if(this->valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    if(val >= this->bin_data->intervals[interval_index].first && val <= this->bin_data->intervals[interval_index].second) {
      // construct exception to be thrown
      std::stringstream ss; ss << val;
      illegal_bin_sample_exception e;
      e.update_bin_info(this->bin_data->name, ss.str());
      this->bin_data->interval_hits[interval_index]++;
      throw e;
    }
    else {
      return 0;
    }
  }

  /* Virtual function used to register this bin inside a coverpoint */
  virtual void add_to_cvp(coverpoint<T> &cvp) override
  {
    if(this->valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    bin<T>::remove_interval_overlap();
    cvp.insert_intervals(cvp.illegal_interval_map,*this,cvp.illegal_bins.size());
    cvp.illegal_bins.push_back(*this);
    cvp.cvp_data->illegal_bins_data.push_back(this->bin_data);
  }

};

template <class T>
class ignore_bin final : public bin<T>
{
  static_assert(std::is_integral<T>::value, "Type must be integral!");
protected:

  ignore_bin() = delete;

public:
  /*!
   *  \brief Forward to parent constructor
   */
  template <typename... Args>
  explicit ignore_bin(Args... args) : bin<T>::bin(args...) {
    this->bin_data->bin_type = bin_t::ignore_;
  }

  virtual ~ignore_bin() = default;

  /* Virtual function used to register this bin inside a coverpoint */
  virtual void add_to_cvp(coverpoint<T> &cvp) override
  {
    if(this->valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    bin<T>::remove_interval_overlap();
    cvp.insert_intervals(cvp.ignore_interval_map,*this,cvp.ignore_bins.size());
    cvp.ignore_bins.push_back(*this);
    cvp.cvp_data->ignore_bins_data.push_back(this->bin_data);
  }

};


template <class T>
class bin_array final : public bin<T>
{
  static_assert(std::is_integral<T>::value, "Type must be integral!");
protected:

  uint64_t count;
  bool sparse = false;

  bin_array() = delete;

public:
  /*!
   * \brief Constructs an bin_array which will split an interval into multiple
   * equal parts. The number of sub-intervals is specified via the count argument
   */
  explicit bin_array(const std::string &name, uint64_t count, interval_t<T> interval) noexcept :
    bin<T>(name, interval), count(count), sparse(false) {}

  /*!
   * \brief Constructs an bin_array from a vector of intervals where each
   * interval will be nested by a bin
   */
  explicit bin_array(const std::string &name, std::vector<interval_t<T>>&& intvs) noexcept :
    count(intvs.size()), sparse(true)
  {
    this->bin_data->name = name;
    this->bin_data->intervals = std::move(intvs);
  }

  /*!
   * \brief Constructs an bin_array from a vector of intervals ref where each
   * interval will be nested by a bin
   */
  explicit bin_array(const std::string &name, std::vector<interval_t<T>>& intvs) noexcept :
    count(intvs.size()), sparse(true)
  {
    this->bin_data->name = name;
    this->bin_data->intervals = intvs;
  }

  /*!
   * \brief Constructs an bin_array from a vector of values where each
   * values will be nested by a bin
   */
  explicit bin_array(const std::string &name, const std::vector<T>& intvs) noexcept :
    count(intvs.size()), sparse(true)
  {
    this->bin_data->name = name;
    this->bin_data->intervals.clear();
    this->bin_data->intervals.reserve(this->count);
    // transform each value in the input vector to an interval
    std::transform(intvs.begin(), intvs.end(),
                   std::back_inserter(this->bin_data->intervals),
                   [](const T& v) { return fc4sc::interval(v,v); });
  }

  virtual ~bin_array() = default;

  /* Virtual function used to register this bin inside a coverpoint */
  virtual void add_to_cvp(coverpoint<T> &cvp) override
  {
    if (this->sparse) {
      // bin array was defined by using a vector of intervals or values
      // create a new bin for each value/interval and add it to the coverpoint
      std::stringstream ss;

      for (size_t i = 0; i < this->bin_data->intervals.size(); ++i) {
        ss << this->bin_data->name << "[" << i << "]";
        //cvp.bins.push_back(bin<T>(ss.str(), this->bin_data->intervals[i]));
	bin<T>(ss.str(), this->bin_data->intervals[i]).add_to_cvp(cvp);
        ss.str(std::string()); // clear the stringstream
      }
    }
    else {
      // bin array was defined by using an interval which needs to be split into
      // multiple pieces. The interval is found in the this->bin_data->intervals[0]

      uint64_t interval_length = (this->bin_data->intervals[0].second - this->bin_data->intervals[0].first) + 1;

      if (this->count > interval_length) {
        // This bin array interval cannot be split into pieces. Add a single
        // bin containing the whole interval to the coverpoint. We can simply
        // use this object since it already matches the bin that we need!
        //cvp.bins.push_back(*this);
	this->bin<T>::add_to_cvp(cvp);
      }
      else {
        std::stringstream ss;
        // This bin array interval must be split into pieces.
        T start = this->bin_data->intervals[0].first;
        T stop = this->bin_data->intervals[0].second;
        T interval_len = (interval_length + 1) / this->count;

        for (size_t i = 0; i < this->count; ++i) {
          ss << this->bin_data->name << "[" << i << "]";
          // the last interval, will contain all the extra elements
          T end = (i == (this->count - 1)) ? stop : start + (interval_len - 1);
          //cvp.bins.push_back(bin<T>(ss.str(), interval(start, end)));
          bin<T>(ss.str(), interval(start, end)).add_to_cvp(cvp);
          start = start + interval_len;
          ss.str(std::string()); // clear the stringstream
        }
      }
    }
  }



};

/*!
 * \brief Maximum number of value sequences a transition may expand to
 */
static const uint64_t trans_max_sequences = 1 << 16;

/*!
 * \brief Maximum number of values in all the value sequences of a transition
 */
static const uint64_t trans_max_values = 1 << 20;

/*!
 * \brief Unsigned type with the bits of T, also for bool
 */
template <typename T, bool = std::is_same<T, bool>::value>
struct trans_unsigned { typedef typename std::make_unsigned<T>::type type; };

template <typename T>
struct trans_unsigned<T, true> { typedef unsigned char type; };

/*!
 * \brief Consecutive repetition of a transition step, see \link fc4sc::repeat \endlink
 * \tparam S Type of the repeated step
 */
template <typename S>
struct trans_repeat_t
{
  S step;
  uint64_t min;
  uint64_t max;
};

/*!
 *  \brief Repeats a step of a transition, as the SystemVerilog step [*n]
 *  \param step Value, interval, vector of values or vector of intervals
 *  \param count Number of consecutive repetitions
 */
template <typename S>
trans_repeat_t<S> repeat(S step, uint64_t count)
{
  return trans_repeat_t<S>{step, count, count};
}

/*!
 *  \brief Repeats a step of a transition, as the SystemVerilog step [*min:max]
 *  \param step Value, interval, vector of values or vector of intervals
 *  \param min Minimum number of consecutive repetitions
 *  \param max Maximum number of consecutive repetitions
 */
template <typename S>
trans_repeat_t<S> repeat(S step, uint64_t min, uint64_t max)
{
  return trans_repeat_t<S>{step, min, max};
}

/*!
 * \brief A transition: a list of steps, where each step is a set of values,
 * which a coverpoint must take in consecutive samples
 * \tparam T Type of the values
 *
 *   Each argument is a step and can be a value, an interval, a vector of
 * values or a vector of intervals. A step given to \link fc4sc::repeat \endlink
 * is repeated. The SystemVerilog transition (1 => [3:4] => 5,7) is written
 * trans<int>(1, interval(3,4), std::vector<int>{5,7}), and (1 => 2 [*2:3])
 * is written trans<int>(1, fc4sc::repeat(2, 2, 3)).
 */
template <typename T>
class trans
{
  static_assert(std::is_integral<T>::value, "Type must be integral!");

  struct step_t
  {
    std::vector<interval_t<T>> values;
    uint64_t min;
    uint64_t max;
  };

  std::vector<step_t> steps;

  static std::vector<interval_t<T>> to_set(T value)
  {
    return std::vector<interval_t<T>>{interval(value, value)};
  }

  static std::vector<interval_t<T>> to_set(interval_t<T> intv)
  {
    if (intv.first > intv.second) std::swap(intv.first, intv.second);
    return std::vector<interval_t<T>>{intv};
  }

  static std::vector<interval_t<T>> to_set(const std::vector<interval_t<T>>& intvs)
  {
    std::vector<interval_t<T>> set;
    for (auto &intv : intvs)
      set.push_back(to_set(intv)[0]);
    return set;
  }

  static std::vector<interval_t<T>> to_set(const std::vector<T>& values)
  {
    std::vector<interval_t<T>> set;
    for (auto &value : values)
      set.push_back(interval(value, value));
    return set;
  }

  void add() {}

  template <typename S, typename... Args>
  void add(const trans_repeat_t<S>& rep, Args... args)
  {
    if (rep.min == 0 || rep.min > rep.max)
      throw std::invalid_argument("Transition repetition must be [*n] or [*min:max] with 0 < min <= max");
    steps.push_back(step_t{to_set(rep.step), rep.min, rep.max});
    add(args...);
  }

  template <typename S, typename... Args>
  void add(const S& step, Args... args)
  {
    steps.push_back(step_t{to_set(step), 1, 1});
    add(args...);
  }

  /*! Adds n to count and checks that count stays within the limit */
  static void add_count(uint64_t& count, uint64_t n)
  {
    if (n > trans_max_sequences || count + n > trans_max_sequences)
      throw std::invalid_argument("Transition expands to too many value sequences");
    count += n;
  }

  /*! Multiplies count by n and checks that count stays within the limit */
  static void mul_count(uint64_t& count, uint64_t n)
  {
    if (n > trans_max_sequences || (n != 0 && count > trans_max_sequences / n))
      throw std::invalid_argument("Transition expands to too many value sequences");
    count *= n;
  }

  /*! Adds n to a number of values and checks that it stays within the limit */
  static void add_values(uint64_t& count, uint64_t n)
  {
    if (n > trans_max_values || count + n > trans_max_values)
      throw std::invalid_argument("Transition expands to too many values");
    count += n;
  }

  /*! Multiplies a number of values by n and checks that it stays within the limit */
  static void mul_values(uint64_t& count, uint64_t n)
  {
    if (n != 0 && count > trans_max_values / n)
      throw std::invalid_argument("Transition expands to too many values");
    count *= n;
  }

  typedef typename trans_unsigned<T>::type U;

  /*! Number of values in a step */
  static uint64_t values_count(const std::vector<interval_t<T>>& set)
  {
    uint64_t count = 0;
    for (auto &intv : set)
    {
      // number of values in the interval, without overflow
      U length = static_cast<U>(static_cast<U>(intv.second) - static_cast<U>(intv.first));
      if (length >= trans_max_sequences)
        throw std::invalid_argument("Transition expands to too many value sequences");
      add_count(count, uint64_t(length) + 1);
    }
    return count;
  }

public:

  /*!
   *  \brief Takes the steps of the transition
   *  \param args Steps of the transition, see \link fc4sc::trans \endlink
   */
  template <typename... Args>
  explicit trans(Args... args)
  {
    add(args...);
    if (steps.empty())
      throw std::invalid_argument("Transition needs at least one step");
  }

  /*!
   *  \brief Returns all the value sequences which match this transition
   *
   *  Throws std::invalid_argument if there are more than trans_max_sequences
   *  sequences, or more than trans_max_values values in all of them.
   */
  std::vector<std::vector<T>> sequences() const
  {
    // count the sequences and their values before building them
    uint64_t count = 1;
    uint64_t length = 0;
    std::vector<std::vector<T>> step_values;
    for (auto &step : steps)
    {
      const uint64_t values = values_count(step.values);
      if (step.min > trans_max_values)
        throw std::invalid_argument("Transition expands to too many values");

      // sequences of the step: sum of values^n, and their values: sum of n*values^n
      uint64_t power = 1;
      for (uint64_t n = 1; n < step.min; ++n)
        mul_count(power, values);
      uint64_t step_count = 0;
      uint64_t step_length = 0;
      for (uint64_t n = step.min; n <= step.max; ++n)
      {
        mul_count(power, values);
        add_count(step_count, power);
        add_values(step_length, n * power);
      }

      uint64_t new_length = length;
      mul_values(new_length, step_count);
      uint64_t step_part = count;
      mul_values(step_part, step_length);
      add_values(new_length, step_part);
      mul_count(count, step_count);
      length = new_length;

      step_values.push_back(std::vector<T>());
      for (auto &intv : step.values)
      {
        const U last = static_cast<U>(static_cast<U>(intv.second) - static_cast<U>(intv.first));
        for (uint64_t k = 0; k <= last; ++k)
          step_values.back().push_back(static_cast<T>(static_cast<U>(static_cast<U>(intv.first) + k)));
      }
    }

    // the step lists of all the repetition counts
    std::vector<std::vector<const std::vector<T>*>> lists(1);
    for (size_t s = 0; s < steps.size(); ++s)
    {
      std::vector<std::vector<const std::vector<T>*>> new_lists;
      for (auto &list : lists)
      {
        for (uint64_t n = steps[s].min; n <= steps[s].max; ++n)
        {
          new_lists.push_back(list);
          new_lists.back().insert(new_lists.back().end(), n, &step_values[s]);
        }
      }
      lists.swap(new_lists);
    }

    // all the value combinations of each step list
    std::vector<std::vector<T>> result;
    result.reserve(count);
    for (auto &list : lists)
    {
      std::vector<size_t> index(list.size(), 0);
      std::vector<T> seq(list.size());
      for (size_t i = 0; i < list.size(); ++i)
        seq[i] = (*list[i])[0];
      bool more = true;
      while (more)
      {
        result.push_back(seq);
        // next combination, the last step changes first
        more = false;
        for (size_t i = list.size(); i > 0 && !more; --i)
        {
          const size_t p = i - 1;
          if (++index[p] == list[p]->size())
            index[p] = 0;
          else
            more = true;
          seq[p] = (*list[p])[index[p]];
        }
      }
    }
    return result;
  }
};

/*!
 * \brief Defines a class for the data model of transition bins
 * \tparam T Type of values in this bin
 */
template <class T>
class transition_bin_data_model : public bin_data_model<T>
{
public:

  /*! The value sequences of this bin. interval_hits holds their hit counts */
  std::vector<std::vector<T>> sequences;

  /* function to introspect the bin's sequences */
  std::vector<std::vector<int>> get_sequences_to_int() const
  {
    std::vector<std::vector<int>> sequences_int;
    for (auto &seq : sequences)
    {
      sequences_int.push_back(std::vector<int>());
      for (auto value : seq)
        sequences_int.back().push_back(static_cast<int>(value));
    }
    return sequences_int;
  }

  /*!
   * \brief Checks if the last sampled values end with one of the sequences
   * of this bin. Increments the hit count of the first such sequence.
   * \param history The last sampled values, the oldest first
   * \param max_length Length of the longest sequence which may be hit
   * \returns Index of the sequence which was hit, or -1
   */
  int sample(const std::deque<T>& history, size_t max_length)
  {
    for (size_t i = 0; i < sequences.size(); ++i)
    {
      const std::vector<T>& seq = sequences[i];
      if (seq.size() > history.size() || seq.size() > max_length) continue;
      if (std::equal(seq.rbegin(), seq.rend(), history.rbegin())) {
        this->interval_hits[i]++;
        return static_cast<int>(i);
      }
    }
    return -1;
  }
};

/*!
 * \brief Defines a class for transition bins
 * \tparam T Type of values in this bin
 *
 *   A transition bin is hit when the values of consecutive samples of the
 * coverpoint match one of its transitions. The SystemVerilog
 * "bins b = (1 => 2 => 3), (5 => 6);" is written
 * transition_bin<int>("b", trans<int>(1, 2, 3), trans<int>(5, 6)).
 * The bin is hit at most once per sample, and a transition which has a value
 * of an ignore bin in it is not hit.
 */
template <class T>
class transition_bin : public bin<T>
{
  static_assert(std::is_integral<T>::value, "Type must be integral!");
  friend class coverpoint<T>;

protected:

  /*! Creates a bin for each sequence instead of one bin for all */
  bool array = false;

  transition_bin() = delete;

  transition_bin_data_model<T>* trans_data()
  {
    return static_cast<transition_bin_data_model<T>*>(this->bin_data);
  }

  void add_trans() {}

  template <typename... Args>
  void add_trans(const trans<T>& tr, Args... args)
  {
    uint64_t values = 0;
    for (auto &seq : trans_data()->sequences)
      values += seq.size();
    for (auto &seq : tr.sequences())
    {
      if (trans_data()->sequences.size() >= trans_max_sequences)
        throw std::invalid_argument("Transition bin \"" + this->bin_data->name + "\" has too many value sequences");
      values += seq.size();
      if (values > trans_max_values)
        throw std::invalid_argument("Transition bin \"" + this->bin_data->name + "\" has too many values");
      trans_data()->sequences.push_back(seq);
      trans_data()->interval_hits.push_back(0);
    }
    add_trans(args...);
  }

  /* Registers a single transition bin inside a coverpoint */
  void add_single_to_cvp(coverpoint<T> &cvp)
  {
    switch (this->bin_data->bin_type)
    {
      case bin_t::illegal_:
        cvp.trans_illegal_bins.push_back(cvp.illegal_bins.size());
        cvp.illegal_bins.push_back(illegal_bin<T>(static_cast<bin<T>&>(*this)));
        cvp.cvp_data->illegal_bins_data.push_back(this->bin_data);
        break;
      case bin_t::ignore_:
        cvp.trans_ignore_bins.push_back(cvp.ignore_bins.size());
        cvp.ignore_bins.push_back(ignore_bin<T>(static_cast<bin<T>&>(*this)));
        cvp.cvp_data->ignore_bins_data.push_back(this->bin_data);
        break;
      default:
        cvp.trans_bins.push_back(cvp.bins.size());
        cvp.bins.push_back(*this);
        cvp.cvp_data->bins_data.push_back(this->bin_data);
        break;
    }
    for (auto &seq : trans_data()->sequences)
      cvp.trans_depth = std::max<size_t>(cvp.trans_depth, seq.size());
  }

public:

  /*!
   *  \brief Takes the bin name and its transitions
   *  \param bin_name Name of the bin
   *  \param args Transitions of the bin, see \link fc4sc::trans \endlink
   */
  template <typename... Args>
  explicit transition_bin(const std::string &bin_name, Args... args) : bin<T>(bin_name, std::vector<interval_t<T>>())
  {
    auto data = new transition_bin_data_model<T>;
    data->name = bin_name;
    data->bin_type = bin_t::default_;
    delete this->bin_data;
    this->bin_data = data;
    this->valid_data = data->valid;
    add_trans(args...);
    if (data->sequences.empty())
      throw std::invalid_argument("Transition bin \"" + bin_name + "\" needs at least one transition");
  }

  virtual ~transition_bin() = default;

  /* Virtual function used to register this bin inside a coverpoint */
  virtual void add_to_cvp(coverpoint<T> &cvp) override
  {
    if(this->valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    if (!array) {
      add_single_to_cvp(cvp);
      return;
    }
    // one bin for each sequence
    std::stringstream ss;
    auto& sequences = trans_data()->sequences;
    for (size_t i = 0; i < sequences.size(); ++i) {
      ss << this->bin_data->name << "[" << i << "]";
      transition_bin<T> single(ss.str());
      single.bin_data->bin_type = this->bin_data->bin_type;
      single.trans_data()->sequences.push_back(sequences[i]);
      single.trans_data()->interval_hits.push_back(0);
      single.add_single_to_cvp(cvp);
      ss.str(std::string()); // clear the stringstream
    }
  }

private:

  /*! Creates a bin without transitions, to be filled by add_to_cvp */
  explicit transition_bin(const std::string &bin_name) : bin<T>(bin_name, std::vector<interval_t<T>>())
  {
    auto data = new transition_bin_data_model<T>;
    data->name = bin_name;
    data->bin_type = bin_t::default_;
    delete this->bin_data;
    this->bin_data = data;
    this->valid_data = data->valid;
  }
};

/*!
 * \brief Defines a class for arrays of transition bins, with a bin for each
 * value sequence of the transitions, as the SystemVerilog "bins b[] = (...);"
 * \tparam T Type of values in this bin
 */
template <class T>
class transition_bin_array final : public transition_bin<T>
{
public:
  template <typename... Args>
  explicit transition_bin_array(const std::string &bin_name, Args... args) :
    transition_bin<T>(bin_name, args...)
  {
    this->array = true;
  }

  virtual ~transition_bin_array() = default;
};

/*!
 * \brief Defines a class for illegal transition bins: sampling one of their
 * transitions throws, as for \link fc4sc::illegal_bin \endlink
 * \tparam T Type of values in this bin
 */
template <class T>
class illegal_transition_bin final : public transition_bin<T>
{
public:
  template <typename... Args>
  explicit illegal_transition_bin(const std::string &bin_name, Args... args) :
    transition_bin<T>(bin_name, args...)
  {
    this->bin_data->bin_type = bin_t::illegal_;
  }

  virtual ~illegal_transition_bin() = default;
};

/*!
 * \brief Defines a class for ignore transition bins: in a sample which
 * completes one of their transitions, no transition bin is hit
 * \tparam T Type of values in this bin
 */
template <class T>
class ignore_transition_bin final : public transition_bin<T>
{
public:
  template <typename... Args>
  explicit ignore_transition_bin(const std::string &bin_name, Args... args) :
    transition_bin<T>(bin_name, args...)
  {
    this->bin_data->bin_type = bin_t::ignore_;
  }

  virtual ~ignore_transition_bin() = default;
};

/*
 * Bin wrapper class used when constructing coverpoint via the COVERPOINT macro.
 * Under the hood, the macro instantiates a coverpoint using std::initializer_list
 * as argument. Because the std::initializer_list is limited to one type only,
 * we cannot directly pass any type of bin we want to the coverpoint. In order to
 * do that, we use this class which offers implicit cast from any type of bin and
 * stores it internally as a dynamically allocated object.
 */
template <class T>
class bin_wrapper final {
private:
  friend class coverpoint<T>;

  std::unique_ptr<bin<T>> bin_h;
  bin<T> *get_bin() const { return bin_h.get(); }

public:
  bin_wrapper(bin_wrapper && r) { bin_h = std::move(r.bin_h); }
  bin_wrapper() = delete;
  bin_wrapper(bin_wrapper &) = delete;
  bin_wrapper& operator=(bin_wrapper &) = delete;
  bin_wrapper& operator=(bin_wrapper &&) = delete;
public:
  ~bin_wrapper() = default;
  // Implicit cast to other bin types.
  bin_wrapper(bin<T>        && r) noexcept : bin_h(new bin<T>        (std::move(r))) {}
  bin_wrapper(bin_array<T>  && r) noexcept : bin_h(new bin_array<T>  (std::move(r))) {}
  bin_wrapper(illegal_bin<T>&& r) noexcept : bin_h(new illegal_bin<T>(std::move(r))) {}
  bin_wrapper(ignore_bin<T> && r) noexcept : bin_h(new ignore_bin<T> (std::move(r))) {}
  bin_wrapper(transition_bin<T> && r) noexcept : bin_h(new transition_bin<T>(std::move(r))) {}
};

} // namespace fc4sc

#endif /* FC4SC_BIN_HPP */
