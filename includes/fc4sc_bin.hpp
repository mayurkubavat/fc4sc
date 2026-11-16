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
#include <algorithm>
#include <vector>
#include <sstream>
#include <memory>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

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
 * \brief Maximum number of intervals a wildcard pattern may expand to
 */
static const uint64_t wildcard_max_intervals = 1 << 16;

/*!
 * \brief Maximum size of a wildcard pattern written as a sized literal
 */
static const uint64_t wildcard_max_size = 1 << 16;

/*!
 *  \brief Parses a wildcard pattern into the bits to match and their values
 *  \param pattern The pattern, see \link fc4sc::wildcard \endlink
 *  \param width Number of bits of the coverpoint type
 *  \param type_signed Whether the coverpoint type is signed
 *  \param value Returns the values of the bits to match
 *  \param care Returns a mask of the bits to match
 */
inline void parse_wildcard(const std::string& pattern, unsigned int width,
                           bool type_signed, uint64_t& value, uint64_t& care)
{
  const std::string error = "Invalid wildcard pattern \"" + pattern + "\": ";
  std::string digits = pattern;
  unsigned int digit_bits = 1;
  size_t size = 0;
  bool literal = false;
  bool is_signed = false;

  size_t quote = pattern.find('\'');
  if (quote != std::string::npos) {
    literal = true;
    size_t base = quote + 1;
    if (base < pattern.size() && (pattern[base] == 's' || pattern[base] == 'S')) {
      is_signed = true;
      ++base;
    }
    if (base >= pattern.size())
      throw std::invalid_argument(error + "missing base");
    switch (pattern[base]) {
      case 'b': case 'B': digit_bits = 1; break;
      case 'o': case 'O': digit_bits = 3; break;
      case 'h': case 'H': digit_bits = 4; break;
      default: throw std::invalid_argument(error + "base must be 'b, 'o or 'h");
    }
    if (quote > 0) {
      if (pattern.find_first_not_of("0123456789") < quote)
        throw std::invalid_argument(error + "bad size");
      for (size_t i = 0; i < quote; ++i) {
        size = size * 10 + (pattern[i] - '0');
        if (size > wildcard_max_size)
          throw std::invalid_argument(error + "size is larger than 65536");
      }
      if (size == 0)
        throw std::invalid_argument(error + "bad size");
    }
    digits = pattern.substr(base + 1);
  }

  // bits of the pattern, LSB first: 0, 1 or 2 for a don't care bit
  std::vector<int> bits;
  for (auto it = digits.rbegin(); it != digits.rend(); ++it)
  {
    const char c = *it;
    if (c == '_') continue;
    int digit;
    if (c == 'x' || c == 'X' || c == 'z' || c == 'Z' || c == '?') {
      digit = -1;
    }
    else if (c >= '0' && c <= '9') {
      digit = c - '0';
    }
    else if (c >= 'a' && c <= 'f') {
      digit = c - 'a' + 10;
    }
    else if (c >= 'A' && c <= 'F') {
      digit = c - 'A' + 10;
    }
    else {
      throw std::invalid_argument(error + "bad digit '" + c + "'");
    }
    if (digit >= (1 << digit_bits))
      throw std::invalid_argument(error + "bad digit '" + c + "'");
    for (unsigned int i = 0; i < digit_bits; ++i)
      bits.push_back((digit < 0) ? 2 : ((digit >> i) & 1));
  }
  if (bits.empty())
    throw std::invalid_argument(error + "no digits");

  if (size > 0) {
    // as for SystemVerilog literals: a pattern shorter than its size is
    // extended with zeros, or with don't care bits if its leftmost bit is one
    int ext = (bits.back() == 2) ? 2 : 0;
    while (bits.size() < size) bits.push_back(ext);
    while (bits.size() > size) {
      if (bits.back() == 1)
        throw std::invalid_argument(error + "digits do not fit in the size");
      bits.pop_back();
    }
  }

  if (bits.size() < width) {
    if (literal && size == 0 && bits.back() == 2) {
      // an unsized literal whose leftmost bit is x is extended with x
      bits.resize(width, 2);
    }
    else if (is_signed && type_signed && size > 0) {
      // a sized signed literal is sign extended to a signed type; with an
      // unsigned type, the comparison is unsigned and it is zero extended
      bits.resize(width, bits.back());
    }
  }

  value = 0;
  // bits above the pattern are zero, so they must be matched
  care = (width >= 64) ? ~uint64_t(0) : ((uint64_t(1) << width) - 1);
  for (size_t i = 0; i < bits.size(); ++i)
  {
    if (i >= width) {
      // bits above the type must be zero or don't care
      if (bits[i] == 1)
        throw std::invalid_argument(error + "wider than the coverpoint type");
      continue;
    }
    if (bits[i] == 2)
      care &= ~(uint64_t(1) << i);
    else if (bits[i] == 1)
      value |= (uint64_t(1) << i);
  }
}

/*!
 *  \brief Returns the values which match a wildcard pattern, as intervals
 *  \param pattern Binary digits, MSB first, where x, X, z, Z and ? are
 *  don't care bits and _ is ignored, as in "1??0" or "10_xx". The pattern may
 *  also be written as a SystemVerilog literal with a 'b, 'o or 'h base and an
 *  optional size, as in "4'b1??0" or "'h3?", where an x digit is a don't care
 *  digit. As for SystemVerilog literals, an unsized literal whose leftmost
 *  digit is x is extended with don't care bits, and a sized signed literal
 *  ('sb, 'so or 'sh) is sign extended to a signed type. Other bits above the
 *  pattern are zero. Bits of the pattern above the type must be zero or
 *  don't care.
 *  \returns The matching values, as sorted and disjoint intervals
 *
 *    The SystemVerilog "wildcard bins b = { 4'b11?? };" is written
 *  bin<T>("b", wildcard<T>("4'b11??")), and the same works for illegal_bin
 *  and ignore_bin.
 */
template <typename T>
std::vector<interval_t<T>> wildcard(const std::string& pattern)
{
  static_assert(std::is_integral<T>::value && !std::is_same<T, bool>::value,
                "Type must be integral!");
  typedef typename std::make_unsigned<T>::type U;
  const unsigned int width = std::numeric_limits<U>::digits;

  uint64_t value, care;
  parse_wildcard(pattern, width, std::is_signed<T>::value, value, care);

  // the don't care bits at the bottom make one block of consecutive values,
  // all combinations of the other don't care bits give the blocks to match
  unsigned int low = 0;
  while (low < width && !((care >> low) & 1)) ++low;

  std::vector<unsigned int> free_bits;
  for (unsigned int i = low; i < width; ++i)
    if (!((care >> i) & 1)) free_bits.push_back(i);

  if (free_bits.size() >= 64 || (uint64_t(1) << free_bits.size()) > wildcard_max_intervals)
    throw std::invalid_argument("Wildcard pattern \"" + pattern + "\" matches too many intervals");

  const U block = (low >= width) ? std::numeric_limits<U>::max()
                                 : static_cast<U>((uint64_t(1) << low) - 1);
  const U sign = static_cast<U>(U(1) << (width - 1));

  std::vector<interval_t<T>> intervals;
  for (uint64_t comb = 0; comb < (uint64_t(1) << free_bits.size()); ++comb)
  {
    uint64_t start = value & care;
    for (size_t i = 0; i < free_bits.size(); ++i)
      if ((comb >> i) & 1) start |= (uint64_t(1) << free_bits[i]);

    const U lo = static_cast<U>(start);
    const U hi = static_cast<U>(lo + block);
    if (std::is_signed<T>::value && lo < sign && hi >= sign) {
      // the block crosses from positive to negative values
      intervals.push_back(interval(static_cast<T>(lo), static_cast<T>(sign - 1)));
      intervals.push_back(interval(static_cast<T>(sign), static_cast<T>(hi)));
    }
    else {
      intervals.push_back(interval(static_cast<T>(lo), static_cast<T>(hi)));
    }
  }

  std::sort(intervals.begin(), intervals.end());

  // merge the intervals which are next to each other
  std::vector<interval_t<T>> merged;
  for (auto &intv : intervals)
  {
    if (!merged.empty() && merged.back().second != std::numeric_limits<T>::max() &&
        merged.back().second + 1 == intv.first)
      merged.back().second = intv.second;
    else
      merged.push_back(intv);
  }
  return merged;
}

/*!
 *  \brief Returns the values which match a wildcard pattern
 *  \param pattern The pattern, see \link fc4sc::wildcard \endlink
 *  \returns The matching values, sorted
 *
 *    The SystemVerilog "wildcard bins b[] = { 4'b11?? };", which has a bin
 *  for each matching value, is written
 *  bin_array<T>("b", wildcard_values<T>("4'b11??")).
 */
template <typename T>
std::vector<T> wildcard_values(const std::string& pattern)
{
  std::vector<T> values;
  for (auto &intv : wildcard<T>(pattern))
  {
    for (T v = intv.first; ; ++v)
    {
      if (values.size() >= wildcard_max_intervals)
        throw std::invalid_argument("Wildcard pattern \"" + pattern + "\" matches too many values");
      values.push_back(v);
      if (v == intv.second) break;
    }
  }
  return values;
}

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
};

} // namespace fc4sc

#endif /* FC4SC_BIN_HPP */
