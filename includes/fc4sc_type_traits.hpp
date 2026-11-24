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

#ifndef FC4SC_TYPE_TRAITS_HPP
#define FC4SC_TYPE_TRAITS_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace sc_dt {
template <int W> class sc_int;
template <int W> class sc_uint;
template <int W> class sc_bigint;
template <int W> class sc_biguint;
template <int W> class sc_bv;
template <int W> class sc_lv;
} // namespace sc_dt

namespace fc4sc {

template <typename T>
using interval_t = std::pair<T, T>;

template <typename T>
struct fc4sc_interval_merge_traits {
  static bool can_merge_adjacent(const T&, const T&) { return true; }
};

template <int W>
struct fc4sc_interval_merge_traits<sc_dt::sc_lv<W>> {
  static bool can_merge_adjacent(const sc_dt::sc_lv<W>& lo, const sc_dt::sc_lv<W>& hi) {
    return lo.is_01() && hi.is_01();
  }
};

namespace detail {
template <typename T>
using decay_t = typename std::decay<T>::type;
}

/*!
 * \brief Extension point for registering enumerators of an enum or enum class
 * for automatic 1-bin-per-enumerator auto-bin generation.
 */
template <typename E>
struct fc4sc_enum_traits {
  static constexpr bool is_specialized = false;
  static std::vector<E> values() { return std::vector<E>(); }
  static std::vector<std::pair<E, std::string>> named_values() {
    std::vector<std::pair<E, std::string>> out;
    for (const auto& v : values()) {
      out.push_back(std::make_pair(v, std::string()));
    }
    return out;
  }
};

namespace detail {

template <typename E>
inline std::vector<std::pair<E, std::string>> parse_enum_names(
    const char* raw_names, const std::vector<E>& vals) {
  std::vector<std::pair<E, std::string>> out;
  std::vector<std::string> tokens;
  if (raw_names != nullptr) {
    std::string s(raw_names);
    size_t start = 0;
    int depth = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
      if (i == s.size() || (s[i] == ',' && depth == 0)) {
        std::string tok = s.substr(start, i - start);
        size_t first = tok.find_first_not_of(" \t\r\n");
        size_t last = tok.find_last_not_of(" \t\r\n");
        if (first != std::string::npos) {
          tok = tok.substr(first, last - first + 1);
          size_t scope = tok.rfind("::");
          if (scope != std::string::npos)
            tok = tok.substr(scope + 2);
          bool ident = !tok.empty();
          for (char c : tok) {
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_')) {
              ident = false;
              break;
            }
          }
          tokens.push_back(ident ? tok : std::string());
        } else {
          tokens.push_back(std::string());
        }
        start = i + 1;
      } else if (s[i] == '(' || s[i] == '<' || s[i] == '{') {
        ++depth;
      } else if ((s[i] == ')' || s[i] == '>' || s[i] == '}') && depth > 0) {
        --depth;
      }
    }
  }
  out.reserve(vals.size());
  for (size_t i = 0; i < vals.size(); ++i) {
    out.push_back(std::make_pair(vals[i], i < tokens.size() ? tokens[i] : std::string()));
  }
  return out;
}

/*!
 * \brief Arbitrary-precision unsigned integer (base 2^32, little-endian)
 * for overflow-free wide SystemC interval splitting and decimal formatting.
 */
struct big_uint {
  std::vector<uint32_t> limbs;

  big_uint() = default;
  explicit big_uint(uint64_t v) {
    if (v != 0) {
      limbs.push_back(static_cast<uint32_t>(v & 0xFFFFFFFFULL));
      uint32_t hi = static_cast<uint32_t>(v >> 32);
      if (hi != 0) limbs.push_back(hi);
    }
  }

  void trim() {
    while (!limbs.empty() && limbs.back() == 0) limbs.pop_back();
  }

  bool is_zero() const { return limbs.empty(); }

  bool fits_u64() const { return limbs.size() <= 2; }

  size_t bit_length() const {
    if (limbs.empty()) return 0;
    size_t bits = (limbs.size() - 1) * 32;
    uint32_t top = limbs.back();
    while (top != 0) {
      ++bits;
      top >>= 1;
    }
    return bits;
  }

  big_uint shl_bits(size_t shift) const {
    if (is_zero() || shift == 0) return *this;
    const size_t word_shift = shift / 32;
    const size_t bit_shift = shift % 32;
    big_uint res;
    res.limbs.assign(word_shift + limbs.size() + (bit_shift ? 1 : 0), 0);
    if (bit_shift == 0) {
      for (size_t i = 0; i < limbs.size(); ++i)
        res.limbs[word_shift + i] = limbs[i];
    } else {
      uint32_t carry = 0;
      for (size_t i = 0; i < limbs.size(); ++i) {
        uint64_t val = (static_cast<uint64_t>(limbs[i]) << bit_shift) | carry;
        res.limbs[word_shift + i] = static_cast<uint32_t>(val & 0xFFFFFFFFULL);
        carry = static_cast<uint32_t>(val >> 32);
      }
      res.limbs[word_shift + limbs.size()] = carry;
    }
    res.trim();
    return res;
  }

  big_uint shr_bits(size_t shift) const {
    if (is_zero() || shift == 0) return *this;
    const size_t word_shift = shift / 32;
    const size_t bit_shift = shift % 32;
    if (word_shift >= limbs.size()) return big_uint();
    big_uint res;
    const size_t n = limbs.size() - word_shift;
    res.limbs.assign(n, 0);
    if (bit_shift == 0) {
      for (size_t i = 0; i < n; ++i)
        res.limbs[i] = limbs[word_shift + i];
    } else {
      for (size_t i = 0; i < n; ++i) {
        uint32_t lo = limbs[word_shift + i] >> bit_shift;
        uint32_t hi = (word_shift + i + 1 < limbs.size())
            ? (limbs[word_shift + i + 1] << (32 - bit_shift))
            : 0U;
        res.limbs[i] = lo | hi;
      }
    }
    res.trim();
    return res;
  }

  static big_uint mul(const big_uint& a, const big_uint& b) {
    if (a.is_zero() || b.is_zero()) return big_uint();
    big_uint res;
    res.limbs.assign(a.limbs.size() + b.limbs.size(), 0);
    for (size_t i = 0; i < a.limbs.size(); ++i) {
      uint64_t carry = 0;
      for (size_t j = 0; j < b.limbs.size(); ++j) {
        uint64_t cur = static_cast<uint64_t>(res.limbs[i + j]) +
                       static_cast<uint64_t>(a.limbs[i]) * b.limbs[j] + carry;
        res.limbs[i + j] = static_cast<uint32_t>(cur & 0xFFFFFFFFULL);
        carry = cur >> 32;
      }
      res.limbs[i + b.limbs.size()] = static_cast<uint32_t>(carry);
    }
    res.trim();
    return res;
  }

  uint64_t to_u64() const {
    uint64_t lo = limbs.empty() ? 0ULL : static_cast<uint64_t>(limbs[0]);
    uint64_t hi = (limbs.size() > 1) ? static_cast<uint64_t>(limbs[1]) : 0ULL;
    return lo | (hi << 32);
  }

  int cmp(const big_uint& o) const {
    if (limbs.size() != o.limbs.size())
      return (limbs.size() < o.limbs.size()) ? -1 : 1;
    for (size_t i = limbs.size(); i > 0; --i) {
      if (limbs[i - 1] != o.limbs[i - 1])
        return (limbs[i - 1] < o.limbs[i - 1]) ? -1 : 1;
    }
    return 0;
  }

  void add_u64(uint64_t v) {
    if (v == 0) return;
    uint64_t carry = v;
    size_t i = 0;
    while (carry > 0) {
      if (i == limbs.size()) limbs.push_back(0);
      uint64_t sum = static_cast<uint64_t>(limbs[i]) + (carry & 0xFFFFFFFFULL);
      limbs[i] = static_cast<uint32_t>(sum & 0xFFFFFFFFULL);
      carry = (carry >> 32) + (sum >> 32);
      ++i;
    }
    trim();
  }

  static big_uint add(const big_uint& a, const big_uint& b) {
    big_uint res;
    size_t n = std::max(a.limbs.size(), b.limbs.size());
    res.limbs.resize(n, 0);
    uint64_t carry = 0;
    for (size_t i = 0; i < n; ++i) {
      uint64_t av = (i < a.limbs.size()) ? a.limbs[i] : 0ULL;
      uint64_t bv = (i < b.limbs.size()) ? b.limbs[i] : 0ULL;
      uint64_t s = av + bv + carry;
      res.limbs[i] = static_cast<uint32_t>(s & 0xFFFFFFFFULL);
      carry = s >> 32;
    }
    if (carry != 0) res.limbs.push_back(static_cast<uint32_t>(carry));
    res.trim();
    return res;
  }

  static big_uint sub(const big_uint& a, const big_uint& b) {
    big_uint res;
    res.limbs.resize(a.limbs.size(), 0);
    int64_t borrow = 0;
    for (size_t i = 0; i < a.limbs.size(); ++i) {
      int64_t av = static_cast<int64_t>(a.limbs[i]);
      int64_t bv = (i < b.limbs.size()) ? static_cast<int64_t>(b.limbs[i]) : 0;
      int64_t d = av - bv - borrow;
      if (d < 0) {
        d += (int64_t(1) << 32);
        borrow = 1;
      } else {
        borrow = 0;
      }
      res.limbs[i] = static_cast<uint32_t>(d);
    }
    res.trim();
    return res;
  }

  static big_uint mul_u32(const big_uint& a, uint32_t m) {
    if (m == 0 || a.is_zero()) return big_uint();
    big_uint res;
    res.limbs.resize(a.limbs.size(), 0);
    uint64_t carry = 0;
    for (size_t i = 0; i < a.limbs.size(); ++i) {
      uint64_t p = static_cast<uint64_t>(a.limbs[i]) * m + carry;
      res.limbs[i] = static_cast<uint32_t>(p & 0xFFFFFFFFULL);
      carry = p >> 32;
    }
    if (carry != 0) res.limbs.push_back(static_cast<uint32_t>(carry));
    res.trim();
    return res;
  }

  static big_uint mul_u64(const big_uint& a, uint64_t m) {
    if (m == 0 || a.is_zero()) return big_uint();
    uint32_t m_lo = static_cast<uint32_t>(m & 0xFFFFFFFFULL);
    uint32_t m_hi = static_cast<uint32_t>(m >> 32);
    big_uint lo = mul_u32(a, m_lo);
    if (m_hi == 0) return lo;
    big_uint hi = mul_u32(a, m_hi);
    if (!hi.is_zero()) hi.limbs.insert(hi.limbs.begin(), 0);
    return add(lo, hi);
  }

  void div_u64(uint64_t d, big_uint& q, uint64_t& rem) const {
    q.limbs.assign(limbs.size(), 0);
    rem = 0;
    if (d <= 0xFFFFFFFFULL) {
      for (size_t i = limbs.size(); i > 0; --i) {
        uint64_t cur = (rem << 32) | limbs[i - 1];
        q.limbs[i - 1] = static_cast<uint32_t>(cur / d);
        rem = cur % d;
      }
      q.trim();
      return;
    }
    for (size_t i = limbs.size(); i > 0; --i) {
      uint32_t w = limbs[i - 1];
      uint32_t qw = 0;
      for (int b = 31; b >= 0; --b) {
        bool carry = (rem >> 63) != 0;
        rem = (rem << 1) | ((w >> b) & 1U);
        if (carry || rem >= d) {
          rem -= d;
          qw |= (1U << b);
        }
      }
      q.limbs[i - 1] = qw;
    }
    q.trim();
  }

  static big_uint from_dec(const std::string& s) {
    big_uint res;
    for (char c : s) {
      if (c < '0' || c > '9') continue;
      res = mul_u32(res, 10U);
      res.add_u64(static_cast<uint64_t>(c - '0'));
    }
    return res;
  }

  std::string to_dec() const {
    if (is_zero()) return "0";
    big_uint cur = *this;
    std::vector<uint32_t> chunks;
    while (!cur.is_zero()) {
      big_uint q;
      uint64_t r = 0;
      cur.div_u64(1000000000ULL, q, r);
      chunks.push_back(static_cast<uint32_t>(r));
      cur = std::move(q);
    }
    std::string out = std::to_string(chunks.back());
    for (size_t i = chunks.size() - 1; i > 0; --i) {
      std::string part = std::to_string(chunks[i - 1]);
      out.append(9 - part.size(), '0');
      out += part;
    }
    return out;
  }

  static big_uint from_bits(const std::string& msb_bits) {
    big_uint res;
    size_t n = msb_bits.size();
    res.limbs.assign((n + 31) / 32, 0);
    for (size_t i = 0; i < n; ++i) {
      if (msb_bits[n - 1 - i] == '1') {
        res.limbs[i / 32] |= (uint32_t(1) << (i % 32));
      }
    }
    res.trim();
    return res;
  }

  std::string to_bits(size_t width) const {
    std::string s(width, '0');
    for (size_t i = 0; i < width; ++i) {
      size_t wi = i / 32;
      if (wi < limbs.size() && ((limbs[wi] >> (i % 32)) & 1U) != 0)
        s[width - 1 - i] = '1';
    }
    return s;
  }
};

inline bool parse_decimal_digits(const std::string& s, bool& neg, std::string& digits) {
  if (s.empty()) return false;
  size_t pos = 0;
  neg = false;
  if (s[pos] == '-') {
    neg = true;
    ++pos;
  }
  if (pos >= s.size()) return false;
  for (size_t i = pos; i < s.size(); ++i) {
    if (s[i] < '0' || s[i] > '9') return false;
  }
  digits = s.substr(pos);
  if (digits.size() > 1 && digits[0] == '0') return false;
  if (neg && digits == "0") return false;
  return true;
}

template <typename T>
inline T add_offset_integral(T lo, uint64_t offset, std::false_type) {
  return static_cast<T>(static_cast<uint64_t>(lo) + offset);
}

template <typename T>
inline T add_offset_integral(T lo, uint64_t offset, std::true_type) {
  const int64_t lo64 = static_cast<int64_t>(lo);
  if (lo64 >= 0) {
    return static_cast<T>(static_cast<int64_t>(static_cast<uint64_t>(lo64) + offset));
  }
  const uint64_t neg_mag = static_cast<uint64_t>(-(lo64 + 1)) + 1ULL;
  if (offset >= neg_mag) {
    return static_cast<T>(static_cast<int64_t>(offset - neg_mag));
  }
  const uint64_t rem = neg_mag - offset;
  return static_cast<T>(-static_cast<int64_t>(rem - 1ULL) - 1);
}

template <typename T>
inline T sub_offset_integral(T hi, uint64_t offset, std::false_type) {
  return static_cast<T>(static_cast<uint64_t>(hi) - offset);
}

template <typename T>
inline T sub_offset_integral(T hi, uint64_t offset, std::true_type) {
  const int64_t hi64 = static_cast<int64_t>(hi);
  if (hi64 >= 0) {
    const uint64_t pos_mag = static_cast<uint64_t>(hi64);
    if (offset <= pos_mag) {
      return static_cast<T>(static_cast<int64_t>(pos_mag - offset));
    }
    const uint64_t rem = offset - pos_mag;
    return static_cast<T>(-static_cast<int64_t>(rem - 1ULL) - 1);
  }
  const uint64_t neg_mag = static_cast<uint64_t>(-(hi64 + 1)) + 1ULL;
  const uint64_t total_neg = neg_mag + offset;
  return static_cast<T>(-static_cast<int64_t>(total_neg - 1ULL) - 1);
}

template <typename T, typename Traits>
inline std::vector<interval_t<T>> split_u64_interval(
    const T& lo, const T& hi, uint64_t dist, uint64_t count, bool one_per_value_if_fewer) {
  std::vector<interval_t<T>> out;
  if (count == 0)
    throw std::invalid_argument("Interval split count must be at least 1");
  if (count - 1 > dist) {
    if (!one_per_value_if_fewer) {
      out.push_back(interval_t<T>(lo, hi));
      return out;
    }
    for (uint64_t i = 0; i <= dist; ++i) {
      T v = Traits::add_offset(lo, i);
      out.push_back(interval_t<T>(v, v));
    }
    return out;
  }
  if (count == 1) {
    out.push_back(interval_t<T>(lo, hi));
    return out;
  }
  const uint64_t k = dist / count;
  const uint64_t m = dist % count;
  const uint64_t q = k + ((m == count - 1) ? 1ULL : 0ULL);
  uint64_t offset = 0;
  for (uint64_t i = 0; i < count; ++i) {
    const T start = Traits::add_offset(lo, offset);
    const T end = (i == count - 1) ? hi : Traits::add_offset(lo, offset + (q - 1));
    out.push_back(interval_t<T>(start, end));
    if (i != count - 1) offset += q;
  }
  return out;
}

template <typename T>
inline uint64_t abs_i64(int64_t v) {
  return (v >= 0) ? static_cast<uint64_t>(v) : (static_cast<uint64_t>(-(v + 1)) + 1ULL);
}

inline uint64_t sat_mul_u64(uint64_t a, uint64_t b) {
  if (a == 0 || b == 0) return 0;
  const uint64_t mx = std::numeric_limits<uint64_t>::max();
  if (a > mx / b) return mx;
  return a * b;
}

inline uint64_t sat_add_u64(uint64_t a, uint64_t b) {
  const uint64_t mx = std::numeric_limits<uint64_t>::max();
  return (a > mx - b) ? mx : (a + b);
}

inline uint64_t percent_delta_u64(uint64_t mag, uint64_t pct, bool& saturated) {
  saturated = false;
  if (mag == 0 || pct == 0) return 0;
  const uint64_t q1 = mag / 100ULL;
  const uint64_t r1 = mag % 100ULL;
  const uint64_t q2 = pct / 100ULL;
  const uint64_t r2 = pct % 100ULL;
  const uint64_t mx = std::numeric_limits<uint64_t>::max();

  auto checked_add = [&](uint64_t a, uint64_t b) -> uint64_t {
    if (a > mx - b) {
      saturated = true;
      return mx;
    }
    return a + b;
  };
  auto checked_mul = [&](uint64_t a, uint64_t b) -> uint64_t {
    if (a != 0 && b > mx / a) {
      saturated = true;
      return mx;
    }
    return a * b;
  };

  uint64_t t1 = checked_mul(checked_mul(q1, q2), 100ULL);
  uint64_t t2 = checked_mul(q1, r2);
  uint64_t t3 = checked_mul(r1, q2);
  uint64_t t4 = (r1 * r2) / 100ULL;
  return checked_add(checked_add(t1, t2), checked_add(t3, t4));
}

} // namespace detail

struct xml_user_attr {
  std::string key;
  std::string type;
  std::string value;
};

template <typename T, typename Enable = void>
struct fc4sc_type_traits {
  static constexpr bool is_supported = false;
  static constexpr bool is_discrete = false;
  static constexpr bool is_floating = false;
  static constexpr bool is_enum_type = false;
  static constexpr bool is_signed = false;
  static constexpr bool supports_wildcard = false;
  static constexpr bool supports_auto_bins = false;
  static constexpr bool supports_tolerance = false;
  static constexpr bool has_enumerated_values = false;
  static constexpr bool has_xml_display_attrs = false;
  static constexpr unsigned int bit_width = 0;
};

template <typename T>
struct fc4sc_type_traits<const T> : fc4sc_type_traits<T> {};

template <typename T>
struct fc4sc_type_traits<volatile T> : fc4sc_type_traits<T> {};

template <typename T>
struct fc4sc_type_traits<const volatile T> : fc4sc_type_traits<T> {};

// 1. Standard integral types (bool, char, int8_t..uint64_t, etc.)
template <typename T>
struct fc4sc_type_traits<T, typename std::enable_if<std::is_integral<T>::value>::type> {
  static constexpr bool is_supported = true;
  static constexpr bool is_discrete = true;
  static constexpr bool is_floating = false;
  static constexpr bool is_enum_type = false;
  static constexpr bool is_signed = std::is_signed<T>::value;
  static constexpr bool supports_wildcard = !std::is_same<T, bool>::value;
  static constexpr bool supports_auto_bins = true;
  static constexpr bool supports_tolerance = !std::is_same<T, bool>::value;
  static constexpr bool has_enumerated_values = false;
  static constexpr bool has_xml_display_attrs = false;
  static constexpr unsigned int bit_width =
      std::is_same<T, bool>::value ? 1U : static_cast<unsigned int>(sizeof(T) * 8U);

  struct less_comp {
    bool operator()(const T& a, const T& b) const { return a < b; }
  };
  typedef std::less<T> map_less_comp;

  static bool is_nan(const T&) { return false; }
  static T canonicalize(const T& v) { return v; }
  static bool less(const T& a, const T& b) { return a < b; }
  static bool equal(const T& a, const T& b) { return a == b; }
  static bool less_equal(const T& a, const T& b) { return a <= b; }
  static bool in_interval(const T& v, const T& lo, const T& hi) { return v >= lo && v <= hi; }
  static bool is_negative(const T& v) { return v < T(0); }

  static T validate_point(const T& v) { return v; }
  static interval_t<T> validate_interval(const T& lo, const T& hi) {
    return (lo <= hi) ? interval_t<T>(lo, hi) : interval_t<T>(hi, lo);
  }

  static T min_value() { return std::numeric_limits<T>::min(); }
  static T max_value() { return std::numeric_limits<T>::max(); }
  static T next(const T& v) { return static_cast<T>(v + 1); }
  static T prev(const T& v) { return static_cast<T>(v - 1); }

  static void auto_bin_bounds(T& lo, T& hi) {
    lo = min_value();
    hi = max_value();
  }
  static std::vector<std::pair<T, std::string>> enumerated_values() {
    return std::vector<std::pair<T, std::string>>();
  }

  static bool try_distance_u64(const T& lo, const T& hi, uint64_t& out) {
    if (hi < lo) return false;
    if (std::is_signed<T>::value) {
      out = static_cast<uint64_t>(static_cast<int64_t>(hi)) -
            static_cast<uint64_t>(static_cast<int64_t>(lo));
    } else {
      out = static_cast<uint64_t>(hi) - static_cast<uint64_t>(lo);
    }
    return true;
  }

  static T add_offset(const T& lo, uint64_t offset) {
    return detail::add_offset_integral(lo, offset, std::is_signed<T>());
  }

  static T sub_offset(const T& hi, uint64_t offset) {
    return detail::sub_offset_integral(hi, offset, std::is_signed<T>());
  }

  static std::vector<interval_t<T>> split_interval(
      const T& lo, const T& hi, uint64_t count, bool one_per_value_if_fewer = false) {
    uint64_t dist = 0;
    try_distance_u64(lo, hi, dist);
    return detail::split_u64_interval<T, fc4sc_type_traits<T>>(
        lo, hi, dist, count, one_per_value_if_fewer);
  }

  static uint64_t interval_count(const T& lo, const T& hi, uint64_t limit) {
    uint64_t dist = 0;
    try_distance_u64(lo, hi, dist);
    if (dist >= limit)
      throw std::invalid_argument("Transition expands to too many value sequences");
    return dist + 1;
  }

  static void enumerate_interval(const T& lo, const T& hi, std::vector<T>& out) {
    uint64_t dist = 0;
    try_distance_u64(lo, hi, dist);
    for (uint64_t k = 0; k <= dist; ++k)
      out.push_back(add_offset(lo, k));
  }

  static T from_bits_u64(uint64_t bits) {
    typedef typename std::make_unsigned<T>::type U;
    return static_cast<T>(static_cast<U>(bits));
  }

  static int to_int(const T& v) { return static_cast<int>(v); }

  static std::string to_string(const T& v) {
    typedef typename std::conditional<std::is_signed<T>::value, long long, unsigned long long>::type wide_t;
    return std::to_string(static_cast<wide_t>(v));
  }

  static std::string to_ucis_integer(const T& v) { return to_string(v); }

  static bool from_ucis_integer(const std::string& s, T& out) {
    bool neg = false;
    std::string digits;
    if (!detail::parse_decimal_digits(s, neg, digits)) return false;
    if (neg && !std::is_signed<T>::value) return false;
    typedef typename std::conditional<std::is_signed<T>::value, long long, unsigned long long>::type wide_t;
    std::istringstream in(s);
    wide_t w = 0;
    if (!(in >> w) || !in.eof()) return false;
    out = static_cast<T>(w);
    return to_ucis_integer(out) == s;
  }

  static const char* xml_attr_type() { return "int"; }
  static void append_range_user_attrs(const T&, const T&, size_t, std::vector<xml_user_attr>&) {}
  static void append_seq_user_attrs(const std::vector<T>&, size_t, std::vector<xml_user_attr>&) {}
};

// 2. Enum and enum class types
template <typename E>
struct fc4sc_type_traits<E, typename std::enable_if<std::is_enum<E>::value>::type> {
  typedef typename std::underlying_type<E>::type underlying_t;
  typedef fc4sc_type_traits<underlying_t> base_t;

  static constexpr bool is_supported = true;
  static constexpr bool is_discrete = true;
  static constexpr bool is_floating = false;
  static constexpr bool is_enum_type = true;
  static constexpr bool is_signed = std::is_signed<underlying_t>::value;
  static constexpr bool supports_wildcard = false;
  static constexpr bool supports_auto_bins = true;
  static constexpr bool supports_tolerance = false;
  static constexpr bool has_enumerated_values = fc4sc_enum_traits<E>::is_specialized;
  static constexpr bool has_xml_display_attrs = false;
  static constexpr unsigned int bit_width = base_t::bit_width;

  struct less_comp {
    bool operator()(const E& a, const E& b) const {
      return static_cast<underlying_t>(a) < static_cast<underlying_t>(b);
    }
  };
  typedef std::less<E> map_less_comp;

  static bool is_nan(const E&) { return false; }
  static E canonicalize(const E& v) { return v; }
  static bool less(const E& a, const E& b) {
    return static_cast<underlying_t>(a) < static_cast<underlying_t>(b);
  }
  static bool equal(const E& a, const E& b) {
    return static_cast<underlying_t>(a) == static_cast<underlying_t>(b);
  }
  static bool less_equal(const E& a, const E& b) {
    return static_cast<underlying_t>(a) <= static_cast<underlying_t>(b);
  }
  static bool in_interval(const E& v, const E& lo, const E& hi) {
    return !less(v, lo) && !less(hi, v);
  }
  static bool is_negative(const E& v) { return static_cast<underlying_t>(v) < underlying_t(0); }

  static E validate_point(const E& v) { return v; }
  static interval_t<E> validate_interval(const E& lo, const E& hi) {
    return less_equal(lo, hi) ? interval_t<E>(lo, hi) : interval_t<E>(hi, lo);
  }

  static E min_value() { return static_cast<E>(base_t::min_value()); }
  static E max_value() { return static_cast<E>(base_t::max_value()); }
  static E next(const E& v) { return static_cast<E>(base_t::next(static_cast<underlying_t>(v))); }
  static E prev(const E& v) { return static_cast<E>(base_t::prev(static_cast<underlying_t>(v))); }

  static void auto_bin_bounds(E& lo, E& hi) {
    lo = min_value();
    hi = max_value();
  }

  static std::vector<std::pair<E, std::string>> enumerated_values() {
    return fc4sc_enum_traits<E>::named_values();
  }

  static bool try_distance_u64(const E& lo, const E& hi, uint64_t& out) {
    return base_t::try_distance_u64(
        static_cast<underlying_t>(lo), static_cast<underlying_t>(hi), out);
  }

  static E add_offset(const E& lo, uint64_t offset) {
    return static_cast<E>(base_t::add_offset(static_cast<underlying_t>(lo), offset));
  }

  static std::vector<interval_t<E>> split_interval(
      const E& lo, const E& hi, uint64_t count, bool one_per_value_if_fewer = false) {
    uint64_t dist = 0;
    try_distance_u64(lo, hi, dist);
    return detail::split_u64_interval<E, fc4sc_type_traits<E>>(
        lo, hi, dist, count, one_per_value_if_fewer);
  }

  static uint64_t interval_count(const E& lo, const E& hi, uint64_t limit) {
    return base_t::interval_count(
        static_cast<underlying_t>(lo), static_cast<underlying_t>(hi), limit);
  }

  static void enumerate_interval(const E& lo, const E& hi, std::vector<E>& out) {
    uint64_t dist = 0;
    try_distance_u64(lo, hi, dist);
    for (uint64_t k = 0; k <= dist; ++k)
      out.push_back(add_offset(lo, k));
  }

  static int to_int(const E& v) { return static_cast<int>(static_cast<underlying_t>(v)); }
  static std::string to_string(const E& v) { return base_t::to_string(static_cast<underlying_t>(v)); }
  static std::string to_ucis_integer(const E& v) {
    return base_t::to_ucis_integer(static_cast<underlying_t>(v));
  }
  static bool from_ucis_integer(const std::string& s, E& out) {
    underlying_t u{};
    if (!base_t::from_ucis_integer(s, u)) return false;
    out = static_cast<E>(u);
    return true;
  }

  static const char* xml_attr_type() { return "int"; }
  static void append_range_user_attrs(const E&, const E&, size_t, std::vector<xml_user_attr>&) {}
  static void append_seq_user_attrs(const std::vector<E>&, size_t, std::vector<xml_user_attr>&) {}
};

// 3. Floating-point types (float, double, long double)
template <typename F>
struct fc4sc_type_traits<F, typename std::enable_if<std::is_floating_point<F>::value>::type> {
  static constexpr bool is_supported = true;
  static constexpr bool is_discrete = false;
  static constexpr bool is_floating = true;
  static constexpr bool is_enum_type = false;
  static constexpr bool is_signed = true;
  static constexpr bool supports_wildcard = false;
  static constexpr bool supports_auto_bins = false;
  static constexpr bool supports_tolerance = true;
  static constexpr bool has_enumerated_values = false;
  static constexpr bool has_xml_display_attrs = true;
  static constexpr unsigned int bit_width = 0;

  struct less_comp {
    bool operator()(const F& a, const F& b) const { return a < b; }
  };
  typedef std::less<F> map_less_comp;

  static bool is_nan(const F& v) { return std::isnan(v); }
  static F canonicalize(const F& v) { return (v == F(0)) ? F(0) : v; }
  static bool less(const F& a, const F& b) { return canonicalize(a) < canonicalize(b); }
  static bool equal(const F& a, const F& b) { return canonicalize(a) == canonicalize(b); }
  static bool less_equal(const F& a, const F& b) { return canonicalize(a) <= canonicalize(b); }
  static bool in_interval(const F& v, const F& lo, const F& hi) {
    if (std::isnan(v)) return false;
    F cv = canonicalize(v);
    return cv >= canonicalize(lo) && cv <= canonicalize(hi);
  }
  static bool is_negative(const F& v) { return v < F(0); }

  static F validate_point(const F& v) {
    if (std::isnan(v))
      throw std::invalid_argument("NaN value is not allowed in floating-point bin");
    return canonicalize(v);
  }

  static interval_t<F> validate_interval(const F& lo, const F& hi) {
    if (std::isnan(lo) || std::isnan(hi))
      throw std::invalid_argument("NaN bound is not allowed in floating-point interval");
    F clo = canonicalize(lo);
    F chi = canonicalize(hi);
    return (clo <= chi) ? interval_t<F>(clo, chi) : interval_t<F>(chi, clo);
  }

  static F min_value() { return -std::numeric_limits<F>::infinity(); }
  static F max_value() { return std::numeric_limits<F>::infinity(); }
  static F next(const F& v) { return canonicalize(std::nextafter(v, max_value())); }
  static F prev(const F& v) { return canonicalize(std::nextafter(v, min_value())); }

  static void auto_bin_bounds(F& lo, F& hi) {
    lo = min_value();
    hi = max_value();
  }
  static std::vector<std::pair<F, std::string>> enumerated_values() {
    return std::vector<std::pair<F, std::string>>();
  }

  static bool try_distance_u64(const F&, const F&, uint64_t&) { return false; }

  static std::vector<interval_t<F>> split_interval(
      const F& raw_lo, const F& raw_hi, uint64_t count, bool = false) {
    if (count == 0)
      throw std::invalid_argument("Interval split count must be at least 1");
    interval_t<F> clean = validate_interval(raw_lo, raw_hi);
    F lo = clean.first;
    F hi = clean.second;
    std::vector<interval_t<F>> out;
    if (lo == hi || count == 1 || !std::isfinite(lo) || !std::isfinite(hi)) {
      out.push_back(interval_t<F>(lo, hi));
      return out;
    }
    // Check if there are fewer than count representable floats in [lo, hi]
    F probe = lo;
    uint64_t steps = 0;
    while (probe < hi && steps < count) {
      probe = next(probe);
      ++steps;
    }
    if (steps < count - 1) {
      out.push_back(interval_t<F>(lo, hi));
      return out;
    }

    F cur_lo = lo;
    const long double ld_lo = static_cast<long double>(lo);
    const long double ld_hi = static_cast<long double>(hi);
    const long double ld_span = ld_hi - ld_lo;
    const bool span_inf = !std::isfinite(ld_span);
    for (uint64_t i = 0; i < count; ++i) {
      if (i == count - 1) {
        out.push_back(interval_t<F>(cur_lo, hi));
        break;
      }
      const long double frac = static_cast<long double>(i + 1) / static_cast<long double>(count);
      long double cut_ld = span_inf
          ? (ld_lo * (1.0L - frac) + ld_hi * frac)
          : (ld_lo + ld_span * frac);
      F cut = canonicalize(static_cast<F>(cut_ld));
      if (cut <= cur_lo) cut = next(cur_lo);
      if (cut > hi) cut = hi;
      F cur_hi = prev(cut);
      if (cur_hi < cur_lo) cur_hi = cur_lo;
      out.push_back(interval_t<F>(cur_lo, cur_hi));
      cur_lo = cut;
    }
    return out;
  }

  static uint64_t interval_count(const F& lo, const F& hi, uint64_t) {
    if (equal(lo, hi)) return 1;
    throw std::invalid_argument("Non-point floating-point interval cannot be enumerated in transition");
  }

  static void enumerate_interval(const F& lo, const F& hi, std::vector<F>& out) {
    if (!equal(lo, hi))
      throw std::invalid_argument("Non-point floating-point interval cannot be enumerated in transition");
    out.push_back(canonicalize(lo));
  }

  static int to_int(const F& v) {
    if (std::isnan(v)) return 0;
    const F lo = static_cast<F>(std::numeric_limits<int>::min());
    const F hi = static_cast<F>(std::numeric_limits<int>::max());
    if (v <= lo) return std::numeric_limits<int>::min();
    if (v >= hi) return std::numeric_limits<int>::max();
    return static_cast<int>(v);
  }

  static std::string to_string(const F& v) {
    F cv = canonicalize(v);
    std::ostringstream ss;
    ss << std::setprecision(std::numeric_limits<F>::max_digits10) << cv;
    return ss.str();
  }

  // Order-preserving IEEE-754 signed-magnitude integer encoding for valid xsd:integer
  static std::string encode_ieee_bits(float v) {
    float cv = canonicalize(v);
    uint32_t u = 0;
    std::memcpy(&u, &cv, sizeof(u));
    int32_t enc = ((u >> 31) == 0)
        ? static_cast<int32_t>(u)
        : -static_cast<int32_t>(u & 0x7FFFFFFFU);
    return std::to_string(enc);
  }

  static bool decode_ieee_bits(const std::string& s, float& out) {
    long long enc = 0;
    std::istringstream in(s);
    if (!(in >> enc) || !in.eof()) return false;
    if (enc < std::numeric_limits<int32_t>::min() || enc > std::numeric_limits<int32_t>::max())
      return false;
    uint32_t u = (enc >= 0)
        ? static_cast<uint32_t>(enc)
        : (0x80000000U | static_cast<uint32_t>(-enc));
    std::memcpy(&out, &u, sizeof(out));
    if (std::isnan(out)) return false;
    out = canonicalize(out);
    return true;
  }

  static std::string encode_ieee_bits(double v) {
    double cv = canonicalize(v);
    uint64_t u = 0;
    std::memcpy(&u, &cv, sizeof(u));
    int64_t enc = ((u >> 63) == 0)
        ? static_cast<int64_t>(u)
        : -static_cast<int64_t>(u & 0x7FFFFFFFFFFFFFFFULL);
    return std::to_string(enc);
  }

  static bool decode_ieee_bits(const std::string& s, double& out) {
    long long enc = 0;
    std::istringstream in(s);
    if (!(in >> enc) || !in.eof()) return false;
    uint64_t u = (enc >= 0)
        ? static_cast<uint64_t>(enc)
        : (0x8000000000000000ULL | static_cast<uint64_t>(-(enc + 1) + 1ULL));
    std::memcpy(&out, &u, sizeof(out));
    if (std::isnan(out)) return false;
    out = canonicalize(out);
    return true;
  }

  static std::string encode_ieee_bits(long double v) {
    return encode_ieee_bits(static_cast<double>(v));
  }

  static bool decode_ieee_bits(const std::string& s, long double& out) {
    double d = 0.0;
    if (!decode_ieee_bits(s, d)) return false;
    out = static_cast<long double>(d);
    return true;
  }

  static std::string to_ucis_integer(const F& v) { return encode_ieee_bits(v); }

  static bool from_ucis_integer(const std::string& s, F& out) {
    bool neg = false;
    std::string digits;
    if (!detail::parse_decimal_digits(s, neg, digits)) return false;
    return decode_ieee_bits(s, out);
  }

  static const char* xml_attr_type() {
    return std::is_same<F, float>::value ? "float" : "double";
  }

  static void append_range_user_attrs(
      const F& lo, const F& hi, size_t idx, std::vector<xml_user_attr>& out) {
    const std::string prefix = "fc4sc.range[" + std::to_string(idx) + "].";
    out.push_back(xml_user_attr{prefix + "from", xml_attr_type(), to_string(lo)});
    out.push_back(xml_user_attr{prefix + "to", xml_attr_type(), to_string(hi)});
  }

  static void append_seq_user_attrs(
      const std::vector<F>& seq, size_t idx, std::vector<xml_user_attr>& out) {
    std::string txt;
    for (size_t i = 0; i < seq.size(); ++i) {
      if (i > 0) txt += " => ";
      txt += to_string(seq[i]);
    }
    out.push_back(xml_user_attr{"fc4sc.seq[" + std::to_string(idx) + "]", "str", txt});
  }
};

// 4. SystemC fixed-width signed/unsigned (sc_int<W> / sc_uint<W>)
template <int W>
struct fc4sc_type_traits<sc_dt::sc_int<W>> {
  typedef sc_dt::sc_int<W> T;
  static constexpr bool is_supported = true;
  static constexpr bool is_discrete = true;
  static constexpr bool is_floating = false;
  static constexpr bool is_enum_type = false;
  static constexpr bool is_signed = true;
  static constexpr bool supports_wildcard = true;
  static constexpr bool supports_auto_bins = true;
  static constexpr bool supports_tolerance = true;
  static constexpr bool has_enumerated_values = false;
  static constexpr bool has_xml_display_attrs = false;
  static constexpr unsigned int bit_width = static_cast<unsigned int>(W);

  struct less_comp {
    bool operator()(const T& a, const T& b) const { return a.to_int64() < b.to_int64(); }
  };
  typedef std::less<T> map_less_comp;

  static bool is_nan(const T&) { return false; }
  static T canonicalize(const T& v) { return v; }
  static bool less(const T& a, const T& b) { return a.to_int64() < b.to_int64(); }
  static bool equal(const T& a, const T& b) { return a.to_int64() == b.to_int64(); }
  static bool less_equal(const T& a, const T& b) { return a.to_int64() <= b.to_int64(); }
  static bool in_interval(const T& v, const T& lo, const T& hi) {
    return v.to_int64() >= lo.to_int64() && v.to_int64() <= hi.to_int64();
  }
  static bool is_negative(const T& v) { return v.to_int64() < 0; }

  static T validate_point(const T& v) { return v; }
  static interval_t<T> validate_interval(const T& lo, const T& hi) {
    return less_equal(lo, hi) ? interval_t<T>(lo, hi) : interval_t<T>(hi, lo);
  }

  static int64_t min_i64() {
    return (W >= 64) ? std::numeric_limits<int64_t>::min()
                     : -static_cast<int64_t>(uint64_t(1) << (W - 1));
  }
  static int64_t max_i64() {
    return (W >= 64) ? std::numeric_limits<int64_t>::max()
                     : static_cast<int64_t>((uint64_t(1) << (W - 1)) - 1);
  }
  static T from_i64(int64_t x) { return T(x); }
  static T min_value() { return from_i64(min_i64()); }
  static T max_value() { return from_i64(max_i64()); }
  static T next(const T& v) { return from_i64(v.to_int64() + 1); }
  static T prev(const T& v) { return from_i64(v.to_int64() - 1); }

  static void auto_bin_bounds(T& lo, T& hi) {
    lo = min_value();
    hi = max_value();
  }
  static std::vector<std::pair<T, std::string>> enumerated_values() {
    return std::vector<std::pair<T, std::string>>();
  }

  static bool try_distance_u64(const T& lo, const T& hi, uint64_t& out) {
    int64_t l = lo.to_int64();
    int64_t h = hi.to_int64();
    if (h < l) return false;
    out = static_cast<uint64_t>(h) - static_cast<uint64_t>(l);
    return true;
  }

  static T add_offset(const T& lo, uint64_t offset) {
    return from_i64(detail::add_offset_integral(lo.to_int64(), offset, std::true_type()));
  }

  static T sub_offset(const T& hi, uint64_t offset) {
    return from_i64(detail::sub_offset_integral(hi.to_int64(), offset, std::true_type()));
  }

  static std::vector<interval_t<T>> split_interval(
      const T& lo, const T& hi, uint64_t count, bool one_per_value_if_fewer = false) {
    uint64_t dist = 0;
    try_distance_u64(lo, hi, dist);
    return detail::split_u64_interval<T, fc4sc_type_traits<T>>(
        lo, hi, dist, count, one_per_value_if_fewer);
  }

  static uint64_t interval_count(const T& lo, const T& hi, uint64_t limit) {
    uint64_t dist = 0;
    try_distance_u64(lo, hi, dist);
    if (dist >= limit)
      throw std::invalid_argument("Transition expands to too many value sequences");
    return dist + 1;
  }

  static void enumerate_interval(const T& lo, const T& hi, std::vector<T>& out) {
    uint64_t dist = 0;
    try_distance_u64(lo, hi, dist);
    for (uint64_t k = 0; k <= dist; ++k)
      out.push_back(add_offset(lo, k));
  }

  static T from_bits_u64(uint64_t bits) {
    if (W < 64 && ((bits >> (W - 1)) & 1ULL) != 0) {
      bits |= (~uint64_t(0)) << W;
    }
    return from_i64(static_cast<int64_t>(bits));
  }

  static int to_int(const T& v) { return v.to_int(); }
  static std::string to_string(const T& v) { return std::to_string(v.to_int64()); }
  static std::string to_ucis_integer(const T& v) { return to_string(v); }
  static bool from_ucis_integer(const std::string& s, T& out) {
    int64_t x = 0;
    if (!fc4sc_type_traits<int64_t>::from_ucis_integer(s, x)) return false;
    if (x < min_i64() || x > max_i64()) return false;
    out = from_i64(x);
    return true;
  }

  static const char* xml_attr_type() { return "int"; }
  static void append_range_user_attrs(const T&, const T&, size_t, std::vector<xml_user_attr>&) {}
  static void append_seq_user_attrs(const std::vector<T>&, size_t, std::vector<xml_user_attr>&) {}
};

template <int W>
struct fc4sc_type_traits<sc_dt::sc_uint<W>> {
  typedef sc_dt::sc_uint<W> T;
  static constexpr bool is_supported = true;
  static constexpr bool is_discrete = true;
  static constexpr bool is_floating = false;
  static constexpr bool is_enum_type = false;
  static constexpr bool is_signed = false;
  static constexpr bool supports_wildcard = true;
  static constexpr bool supports_auto_bins = true;
  static constexpr bool supports_tolerance = true;
  static constexpr bool has_enumerated_values = false;
  static constexpr bool has_xml_display_attrs = false;
  static constexpr unsigned int bit_width = static_cast<unsigned int>(W);

  struct less_comp {
    bool operator()(const T& a, const T& b) const { return a.to_uint64() < b.to_uint64(); }
  };
  typedef std::less<T> map_less_comp;

  static bool is_nan(const T&) { return false; }
  static T canonicalize(const T& v) { return v; }
  static bool less(const T& a, const T& b) { return a.to_uint64() < b.to_uint64(); }
  static bool equal(const T& a, const T& b) { return a.to_uint64() == b.to_uint64(); }
  static bool less_equal(const T& a, const T& b) { return a.to_uint64() <= b.to_uint64(); }
  static bool in_interval(const T& v, const T& lo, const T& hi) {
    return v.to_uint64() >= lo.to_uint64() && v.to_uint64() <= hi.to_uint64();
  }
  static bool is_negative(const T&) { return false; }

  static T validate_point(const T& v) { return v; }
  static interval_t<T> validate_interval(const T& lo, const T& hi) {
    return less_equal(lo, hi) ? interval_t<T>(lo, hi) : interval_t<T>(hi, lo);
  }

  static uint64_t max_u64() {
    return (W >= 64) ? std::numeric_limits<uint64_t>::max() : ((uint64_t(1) << W) - 1);
  }
  static T from_u64(uint64_t x) { return T(x); }
  static T min_value() { return from_u64(0); }
  static T max_value() { return from_u64(max_u64()); }
  static T next(const T& v) { return from_u64(v.to_uint64() + 1); }
  static T prev(const T& v) { return from_u64(v.to_uint64() - 1); }

  static void auto_bin_bounds(T& lo, T& hi) {
    lo = min_value();
    hi = max_value();
  }
  static std::vector<std::pair<T, std::string>> enumerated_values() {
    return std::vector<std::pair<T, std::string>>();
  }

  static bool try_distance_u64(const T& lo, const T& hi, uint64_t& out) {
    uint64_t l = lo.to_uint64();
    uint64_t h = hi.to_uint64();
    if (h < l) return false;
    out = h - l;
    return true;
  }

  static T add_offset(const T& lo, uint64_t offset) {
    return from_u64(lo.to_uint64() + offset);
  }

  static T sub_offset(const T& hi, uint64_t offset) {
    return from_u64(hi.to_uint64() - offset);
  }

  static std::vector<interval_t<T>> split_interval(
      const T& lo, const T& hi, uint64_t count, bool one_per_value_if_fewer = false) {
    uint64_t dist = 0;
    try_distance_u64(lo, hi, dist);
    return detail::split_u64_interval<T, fc4sc_type_traits<T>>(
        lo, hi, dist, count, one_per_value_if_fewer);
  }

  static uint64_t interval_count(const T& lo, const T& hi, uint64_t limit) {
    uint64_t dist = 0;
    try_distance_u64(lo, hi, dist);
    if (dist >= limit)
      throw std::invalid_argument("Transition expands to too many value sequences");
    return dist + 1;
  }

  static void enumerate_interval(const T& lo, const T& hi, std::vector<T>& out) {
    uint64_t dist = 0;
    try_distance_u64(lo, hi, dist);
    for (uint64_t k = 0; k <= dist; ++k)
      out.push_back(add_offset(lo, k));
  }

  static T from_bits_u64(uint64_t bits) {
    return from_u64(bits & max_u64());
  }

  static int to_int(const T& v) { return v.to_int(); }
  static std::string to_string(const T& v) { return std::to_string(v.to_uint64()); }
  static std::string to_ucis_integer(const T& v) { return to_string(v); }
  static bool from_ucis_integer(const std::string& s, T& out) {
    uint64_t x = 0;
    if (!fc4sc_type_traits<uint64_t>::from_ucis_integer(s, x)) return false;
    if (x > max_u64()) return false;
    out = from_u64(x);
    return true;
  }

  static const char* xml_attr_type() { return "int"; }
  static void append_range_user_attrs(const T&, const T&, size_t, std::vector<xml_user_attr>&) {}
  static void append_seq_user_attrs(const std::vector<T>&, size_t, std::vector<xml_user_attr>&) {}
};

// 5. SystemC arbitrary-width signed/unsigned (sc_bigint<W> / sc_biguint<W>)
template <int W>
struct fc4sc_type_traits<sc_dt::sc_bigint<W>> {
  typedef sc_dt::sc_bigint<W> T;
  static constexpr bool is_supported = true;
  static constexpr bool is_discrete = true;
  static constexpr bool is_floating = false;
  static constexpr bool is_enum_type = false;
  static constexpr bool is_signed = true;
  static constexpr bool supports_wildcard = (W <= 64);
  static constexpr bool supports_auto_bins = true;
  static constexpr bool supports_tolerance = true;
  static constexpr bool has_enumerated_values = false;
  static constexpr bool has_xml_display_attrs = false;
  static constexpr unsigned int bit_width = static_cast<unsigned int>(W);

  struct less_comp {
    bool operator()(const T& a, const T& b) const { return a < b; }
  };
  typedef std::less<T> map_less_comp;

  static bool is_nan(const T&) { return false; }
  static T canonicalize(const T& v) { return v; }
  static bool less(const T& a, const T& b) { return a < b; }
  static bool equal(const T& a, const T& b) { return a == b; }
  static bool less_equal(const T& a, const T& b) { return a <= b; }
  static bool in_interval(const T& v, const T& lo, const T& hi) { return v >= lo && v <= hi; }
  static bool is_negative(const T& v) { return v < 0; }

  static T validate_point(const T& v) { return v; }
  static interval_t<T> validate_interval(const T& lo, const T& hi) {
    return (lo <= hi) ? interval_t<T>(lo, hi) : interval_t<T>(hi, lo);
  }

  static T min_value() {
    T v(0);
    v.set(W - 1);
    return v;
  }
  static T max_value() {
    T v = min_value();
    return ~v;
  }
  static T next(const T& v) { return T(v + 1); }
  static T prev(const T& v) { return T(v - 1); }

  static void auto_bin_bounds(T& lo, T& hi) {
    lo = min_value();
    hi = max_value();
  }
  static std::vector<std::pair<T, std::string>> enumerated_values() {
    return std::vector<std::pair<T, std::string>>();
  }

  static detail::big_uint to_offset_from_min(const T& v) {
    detail::big_uint half;
    half.limbs.assign((W + 31) / 32, 0);
    half.limbs[(W - 1) / 32] |= (uint32_t(1) << ((W - 1) % 32));
    std::string s = v.to_string();
    if (!s.empty() && s[0] == '-') {
      return detail::big_uint::sub(half, detail::big_uint::from_dec(s.substr(1)));
    }
    return detail::big_uint::add(half, detail::big_uint::from_dec(s));
  }

  static T from_offset_from_min(const detail::big_uint& off) {
    detail::big_uint half;
    half.limbs.assign((W + 31) / 32, 0);
    half.limbs[(W - 1) / 32] |= (uint32_t(1) << ((W - 1) % 32));
    T out(0);
    if (off.cmp(half) < 0) {
      std::string dec = "-" + detail::big_uint::sub(half, off).to_dec();
      from_ucis_integer(dec, out);
    } else {
      std::string dec = detail::big_uint::sub(off, half).to_dec();
      from_ucis_integer(dec, out);
    }
    return out;
  }

  static detail::big_uint distance_big(const T& lo, const T& hi) {
    return detail::big_uint::sub(to_offset_from_min(hi), to_offset_from_min(lo));
  }

  static bool try_distance_u64(const T& lo, const T& hi, uint64_t& out) {
    if (hi < lo) return false;
    detail::big_uint d = distance_big(lo, hi);
    if (!d.fits_u64()) return false;
    out = d.to_u64();
    return true;
  }

  static T add_offset(const T& lo, uint64_t offset) {
    detail::big_uint base = to_offset_from_min(lo);
    base.add_u64(offset);
    return from_offset_from_min(base);
  }

  static T add_big_offset(const T& lo, const detail::big_uint& offset) {
    return from_offset_from_min(detail::big_uint::add(to_offset_from_min(lo), offset));
  }

  static T sub_big_offset(const T& hi, const detail::big_uint& offset) {
    return from_offset_from_min(detail::big_uint::sub(to_offset_from_min(hi), offset));
  }

  static std::vector<interval_t<T>> split_interval(
      const T& lo, const T& hi, uint64_t count, bool one_per_value_if_fewer = false) {
    if (count == 0)
      throw std::invalid_argument("Interval split count must be at least 1");
    detail::big_uint dist = distance_big(lo, hi);
    if (dist.fits_u64()) {
      return detail::split_u64_interval<T, fc4sc_type_traits<T>>(
          lo, hi, dist.to_u64(), count, one_per_value_if_fewer);
    }
    std::vector<interval_t<T>> out;
    if (count == 1) {
      out.push_back(interval_t<T>(lo, hi));
      return out;
    }
    detail::big_uint k;
    uint64_t m = 0;
    dist.div_u64(count, k, m);
    detail::big_uint q = k;
    if (m == count - 1) q.add_u64(1);
    detail::big_uint q_minus_1 = detail::big_uint::sub(q, detail::big_uint(1));
    detail::big_uint lo_off = to_offset_from_min(lo);
    detail::big_uint offset;
    for (uint64_t i = 0; i < count; ++i) {
      T start = from_offset_from_min(detail::big_uint::add(lo_off, offset));
      T end = (i == count - 1)
          ? hi
          : from_offset_from_min(detail::big_uint::add(detail::big_uint::add(lo_off, offset), q_minus_1));
      out.push_back(interval_t<T>(start, end));
      if (i != count - 1) offset = detail::big_uint::add(offset, q);
    }
    return out;
  }

  static uint64_t interval_count(const T& lo, const T& hi, uint64_t limit) {
    uint64_t dist = 0;
    if (!try_distance_u64(lo, hi, dist) || dist >= limit)
      throw std::invalid_argument("Transition expands to too many value sequences");
    return dist + 1;
  }

  static void enumerate_interval(const T& lo, const T& hi, std::vector<T>& out) {
    uint64_t dist = 0;
    if (!try_distance_u64(lo, hi, dist))
      throw std::invalid_argument("Transition interval is too large to enumerate");
    for (uint64_t k = 0; k <= dist; ++k)
      out.push_back(add_offset(lo, k));
  }

  static T from_bits_u64(uint64_t bits) {
    if (W < 64 && ((bits >> (W - 1)) & 1ULL) != 0) {
      bits |= (~uint64_t(0)) << W;
    }
    return T(static_cast<long long>(bits));
  }

  static int to_int(const T& v) { return v.to_int(); }
  static std::string to_string(const T& v) { return v.to_string(); }
  static std::string to_ucis_integer(const T& v) { return v.to_string(); }

  static bool from_ucis_integer(const std::string& s, T& out) {
    bool neg = false;
    std::string digits;
    if (!detail::parse_decimal_digits(s, neg, digits)) return false;
    T acc(0);
    for (char c : digits) {
      int d = c - '0';
      acc = neg ? T(acc * 10 - d) : T(acc * 10 + d);
    }
    if (acc.to_string() != s) return false;
    out = acc;
    return true;
  }

  static const char* xml_attr_type() { return "int"; }
  static void append_range_user_attrs(const T&, const T&, size_t, std::vector<xml_user_attr>&) {}
  static void append_seq_user_attrs(const std::vector<T>&, size_t, std::vector<xml_user_attr>&) {}
};

template <int W>
struct fc4sc_type_traits<sc_dt::sc_biguint<W>> {
  typedef sc_dt::sc_biguint<W> T;
  static constexpr bool is_supported = true;
  static constexpr bool is_discrete = true;
  static constexpr bool is_floating = false;
  static constexpr bool is_enum_type = false;
  static constexpr bool is_signed = false;
  static constexpr bool supports_wildcard = (W <= 64);
  static constexpr bool supports_auto_bins = true;
  static constexpr bool supports_tolerance = true;
  static constexpr bool has_enumerated_values = false;
  static constexpr bool has_xml_display_attrs = false;
  static constexpr unsigned int bit_width = static_cast<unsigned int>(W);

  struct less_comp {
    bool operator()(const T& a, const T& b) const { return a < b; }
  };
  typedef std::less<T> map_less_comp;

  static bool is_nan(const T&) { return false; }
  static T canonicalize(const T& v) { return v; }
  static bool less(const T& a, const T& b) { return a < b; }
  static bool equal(const T& a, const T& b) { return a == b; }
  static bool less_equal(const T& a, const T& b) { return a <= b; }
  static bool in_interval(const T& v, const T& lo, const T& hi) { return v >= lo && v <= hi; }
  static bool is_negative(const T&) { return false; }

  static T validate_point(const T& v) { return v; }
  static interval_t<T> validate_interval(const T& lo, const T& hi) {
    return (lo <= hi) ? interval_t<T>(lo, hi) : interval_t<T>(hi, lo);
  }

  static T min_value() { return T(0); }
  static T max_value() {
    T v(0);
    return ~v;
  }
  static T next(const T& v) { return T(v + 1); }
  static T prev(const T& v) { return T(v - 1); }

  static void auto_bin_bounds(T& lo, T& hi) {
    lo = min_value();
    hi = max_value();
  }
  static std::vector<std::pair<T, std::string>> enumerated_values() {
    return std::vector<std::pair<T, std::string>>();
  }

  static detail::big_uint to_big_uint(const T& v) {
    return detail::big_uint::from_dec(v.to_string());
  }

  static T from_big_uint(const detail::big_uint& b) {
    T out(0);
    from_ucis_integer(b.to_dec(), out);
    return out;
  }

  static detail::big_uint distance_big(const T& lo, const T& hi) {
    return detail::big_uint::sub(to_big_uint(hi), to_big_uint(lo));
  }

  static bool try_distance_u64(const T& lo, const T& hi, uint64_t& out) {
    if (hi < lo) return false;
    detail::big_uint d = distance_big(lo, hi);
    if (!d.fits_u64()) return false;
    out = d.to_u64();
    return true;
  }

  static T add_offset(const T& lo, uint64_t offset) {
    detail::big_uint b = to_big_uint(lo);
    b.add_u64(offset);
    return from_big_uint(b);
  }

  static T add_big_offset(const T& lo, const detail::big_uint& offset) {
    return from_big_uint(detail::big_uint::add(to_big_uint(lo), offset));
  }

  static T sub_big_offset(const T& hi, const detail::big_uint& offset) {
    return from_big_uint(detail::big_uint::sub(to_big_uint(hi), offset));
  }

  static std::vector<interval_t<T>> split_interval(
      const T& lo, const T& hi, uint64_t count, bool one_per_value_if_fewer = false) {
    if (count == 0)
      throw std::invalid_argument("Interval split count must be at least 1");
    detail::big_uint dist = distance_big(lo, hi);
    if (dist.fits_u64()) {
      return detail::split_u64_interval<T, fc4sc_type_traits<T>>(
          lo, hi, dist.to_u64(), count, one_per_value_if_fewer);
    }
    std::vector<interval_t<T>> out;
    if (count == 1) {
      out.push_back(interval_t<T>(lo, hi));
      return out;
    }
    detail::big_uint k;
    uint64_t m = 0;
    dist.div_u64(count, k, m);
    detail::big_uint q = k;
    if (m == count - 1) q.add_u64(1);
    detail::big_uint q_minus_1 = detail::big_uint::sub(q, detail::big_uint(1));
    detail::big_uint lo_b = to_big_uint(lo);
    detail::big_uint offset;
    for (uint64_t i = 0; i < count; ++i) {
      T start = from_big_uint(detail::big_uint::add(lo_b, offset));
      T end = (i == count - 1)
          ? hi
          : from_big_uint(detail::big_uint::add(detail::big_uint::add(lo_b, offset), q_minus_1));
      out.push_back(interval_t<T>(start, end));
      if (i != count - 1) offset = detail::big_uint::add(offset, q);
    }
    return out;
  }

  static uint64_t interval_count(const T& lo, const T& hi, uint64_t limit) {
    uint64_t dist = 0;
    if (!try_distance_u64(lo, hi, dist) || dist >= limit)
      throw std::invalid_argument("Transition expands to too many value sequences");
    return dist + 1;
  }

  static void enumerate_interval(const T& lo, const T& hi, std::vector<T>& out) {
    uint64_t dist = 0;
    if (!try_distance_u64(lo, hi, dist))
      throw std::invalid_argument("Transition interval is too large to enumerate");
    for (uint64_t k = 0; k <= dist; ++k)
      out.push_back(add_offset(lo, k));
  }

  static T from_bits_u64(uint64_t bits) {
    uint64_t mask = (W >= 64) ? ~uint64_t(0) : ((uint64_t(1) << W) - 1);
    return T(static_cast<unsigned long long>(bits & mask));
  }

  static int to_int(const T& v) { return v.to_int(); }
  static std::string to_string(const T& v) { return v.to_string(); }
  static std::string to_ucis_integer(const T& v) { return v.to_string(); }

  static bool from_ucis_integer(const std::string& s, T& out) {
    bool neg = false;
    std::string digits;
    if (!detail::parse_decimal_digits(s, neg, digits) || neg) return false;
    T acc(0);
    for (char c : digits) {
      acc = T(acc * 10 + (c - '0'));
    }
    if (acc.to_string() != s) return false;
    out = acc;
    return true;
  }

  static const char* xml_attr_type() { return "int"; }
  static void append_range_user_attrs(const T&, const T&, size_t, std::vector<xml_user_attr>&) {}
  static void append_seq_user_attrs(const std::vector<T>&, size_t, std::vector<xml_user_attr>&) {}
};

// 6. SystemC 2-state bit vector (sc_bv<W>)
template <int W>
struct fc4sc_type_traits<sc_dt::sc_bv<W>> {
  typedef sc_dt::sc_bv<W> T;
  static constexpr bool is_supported = true;
  static constexpr bool is_discrete = true;
  static constexpr bool is_floating = false;
  static constexpr bool is_enum_type = false;
  static constexpr bool is_signed = false;
  static constexpr bool supports_wildcard = (W <= 64);
  static constexpr bool supports_auto_bins = true;
  static constexpr bool supports_tolerance = false;
  static constexpr bool has_enumerated_values = false;
  static constexpr bool has_xml_display_attrs = false;
  static constexpr unsigned int bit_width = static_cast<unsigned int>(W);

  static bool less(const T& a, const T& b) {
    for (int i = a.size() - 1; i >= 0; --i) {
      auto wa = a.get_word(i);
      auto wb = b.get_word(i);
      if (wa != wb) return wa < wb;
    }
    return false;
  }

  struct less_comp {
    bool operator()(const T& a, const T& b) const { return less(a, b); }
  };
  typedef less_comp map_less_comp;

  static bool is_nan(const T&) { return false; }
  static T canonicalize(const T& v) { return v; }
  static bool equal(const T& a, const T& b) { return a == b; }
  static bool less_equal(const T& a, const T& b) { return !less(b, a); }
  static bool in_interval(const T& v, const T& lo, const T& hi) {
    return !less(v, lo) && !less(hi, v);
  }
  static bool is_negative(const T&) { return false; }

  static T validate_point(const T& v) { return v; }
  static interval_t<T> validate_interval(const T& lo, const T& hi) {
    return less_equal(lo, hi) ? interval_t<T>(lo, hi) : interval_t<T>(hi, lo);
  }

  static T min_value() { return T(std::string(static_cast<size_t>(W), '0').c_str()); }
  static T max_value() { return T(std::string(static_cast<size_t>(W), '1').c_str()); }

  static T next(const T& v) {
    std::string s = v.to_string();
    for (size_t i = s.size(); i > 0; --i) {
      if (s[i - 1] == '0') {
        s[i - 1] = '1';
        break;
      }
      s[i - 1] = '0';
    }
    return T(s.c_str());
  }

  static T prev(const T& v) {
    std::string s = v.to_string();
    for (size_t i = s.size(); i > 0; --i) {
      if (s[i - 1] == '1') {
        s[i - 1] = '0';
        break;
      }
      s[i - 1] = '1';
    }
    return T(s.c_str());
  }

  static void auto_bin_bounds(T& lo, T& hi) {
    lo = min_value();
    hi = max_value();
  }
  static std::vector<std::pair<T, std::string>> enumerated_values() {
    return std::vector<std::pair<T, std::string>>();
  }

  static detail::big_uint to_big_uint(const T& v) {
    return detail::big_uint::from_bits(v.to_string());
  }
  static T from_big_uint(const detail::big_uint& b) {
    return T(b.to_bits(static_cast<size_t>(W)).c_str());
  }

  static bool try_distance_u64(const T& lo, const T& hi, uint64_t& out) {
    if (less(hi, lo)) return false;
    detail::big_uint d = detail::big_uint::sub(to_big_uint(hi), to_big_uint(lo));
    if (!d.fits_u64()) return false;
    out = d.to_u64();
    return true;
  }

  static T add_offset(const T& lo, uint64_t offset) {
    detail::big_uint b = to_big_uint(lo);
    b.add_u64(offset);
    return from_big_uint(b);
  }

  static std::vector<interval_t<T>> split_interval(
      const T& lo, const T& hi, uint64_t count, bool one_per_value_if_fewer = false) {
    if (count == 0)
      throw std::invalid_argument("Interval split count must be at least 1");
    detail::big_uint lo_b = to_big_uint(lo);
    detail::big_uint hi_b = to_big_uint(hi);
    detail::big_uint dist = detail::big_uint::sub(hi_b, lo_b);
    if (dist.fits_u64()) {
      return detail::split_u64_interval<T, fc4sc_type_traits<T>>(
          lo, hi, dist.to_u64(), count, one_per_value_if_fewer);
    }
    std::vector<interval_t<T>> out;
    if (count == 1) {
      out.push_back(interval_t<T>(lo, hi));
      return out;
    }
    detail::big_uint k;
    uint64_t m = 0;
    dist.div_u64(count, k, m);
    detail::big_uint q = k;
    if (m == count - 1) q.add_u64(1);
    detail::big_uint q_minus_1 = detail::big_uint::sub(q, detail::big_uint(1));
    detail::big_uint offset;
    for (uint64_t i = 0; i < count; ++i) {
      T start = from_big_uint(detail::big_uint::add(lo_b, offset));
      T end = (i == count - 1)
          ? hi
          : from_big_uint(detail::big_uint::add(detail::big_uint::add(lo_b, offset), q_minus_1));
      out.push_back(interval_t<T>(start, end));
      if (i != count - 1) offset = detail::big_uint::add(offset, q);
    }
    return out;
  }

  static uint64_t interval_count(const T& lo, const T& hi, uint64_t limit) {
    uint64_t dist = 0;
    if (!try_distance_u64(lo, hi, dist) || dist >= limit)
      throw std::invalid_argument("Transition expands to too many value sequences");
    return dist + 1;
  }

  static void enumerate_interval(const T& lo, const T& hi, std::vector<T>& out) {
    uint64_t dist = 0;
    if (!try_distance_u64(lo, hi, dist))
      throw std::invalid_argument("Transition interval is too large to enumerate");
    for (uint64_t k = 0; k <= dist; ++k)
      out.push_back(add_offset(lo, k));
  }

  static T from_bits_u64(uint64_t bits) {
    std::string s(static_cast<size_t>(W), '0');
    for (int i = 0; i < W && i < 64; ++i) {
      if (((bits >> i) & 1ULL) != 0)
        s[static_cast<size_t>(W - 1 - i)] = '1';
    }
    return T(s.c_str());
  }

  static int to_int(const T& v) { return v.to_int(); }
  static std::string to_string(const T& v) { return v.to_string(); }
  static std::string to_ucis_integer(const T& v) { return to_big_uint(v).to_dec(); }

  static bool from_ucis_integer(const std::string& s, T& out) {
    bool neg = false;
    std::string digits;
    if (!detail::parse_decimal_digits(s, neg, digits) || neg) return false;
    detail::big_uint b = detail::big_uint::from_dec(digits);
    detail::big_uint max_b = to_big_uint(max_value());
    if (b.cmp(max_b) > 0) return false;
    out = from_big_uint(b);
    return true;
  }

  static const char* xml_attr_type() { return "bits"; }
  static void append_range_user_attrs(const T&, const T&, size_t, std::vector<xml_user_attr>&) {}
  static void append_seq_user_attrs(const std::vector<T>&, size_t, std::vector<xml_user_attr>&) {}
};

// 7. SystemC 4-state logic vector (sc_lv<W>)
// Total order (!is_01(), base-4 digits): 2-state [0..2^W-1] first, then 4-state X/Z values.
template <int W>
struct fc4sc_type_traits<sc_dt::sc_lv<W>> {
  typedef sc_dt::sc_lv<W> T;
  static constexpr bool is_supported = true;
  static constexpr bool is_discrete = true;
  static constexpr bool is_floating = false;
  static constexpr bool is_enum_type = false;
  static constexpr bool is_signed = false;
  static constexpr bool supports_wildcard = false;
  static constexpr bool supports_auto_bins = true;
  static constexpr bool supports_tolerance = false;
  static constexpr bool has_enumerated_values = false;
  static constexpr bool has_xml_display_attrs = true;
  static constexpr unsigned int bit_width = static_cast<unsigned int>(W);

  static unsigned int trit_at(const T& v, int bit_idx) {
    int wi = bit_idx / 32;
    int bi = bit_idx % 32;
    unsigned int cw = static_cast<unsigned int>((v.get_cword(wi) >> bi) & 1U);
    unsigned int dw = static_cast<unsigned int>((v.get_word(wi) >> bi) & 1U);
    return (cw << 1) | dw; // 0='0', 1='1', 2='Z', 3='X'
  }

  static bool less(const T& a, const T& b) {
    const bool a01 = a.is_01();
    const bool b01 = b.is_01();
    if (a01 != b01) return a01; // 2-state values come before all 4-state values
    if (a01) {
      for (int i = a.size() - 1; i >= 0; --i) {
        auto wa = a.get_word(i);
        auto wb = b.get_word(i);
        if (wa != wb) return wa < wb;
      }
      return false;
    }
    // Both have X/Z: compare MSB-to-LSB bit-by-bit; fast skip identical words
    for (int wi = a.size() - 1; wi >= 0; --wi) {
      if (a.get_word(wi) == b.get_word(wi) && a.get_cword(wi) == b.get_cword(wi))
        continue;
      int hi_bit = std::min(W - 1, (wi + 1) * 32 - 1);
      int lo_bit = wi * 32;
      for (int bit = hi_bit; bit >= lo_bit; --bit) {
        unsigned int ta = trit_at(a, bit);
        unsigned int tb = trit_at(b, bit);
        if (ta != tb) return ta < tb;
      }
    }
    return false;
  }

  struct less_comp {
    bool operator()(const T& a, const T& b) const { return less(a, b); }
  };
  typedef less_comp map_less_comp;

  static bool is_nan(const T&) { return false; }
  static T canonicalize(const T& v) { return v; }
  static bool equal(const T& a, const T& b) { return a == b; }
  static bool less_equal(const T& a, const T& b) { return !less(b, a); }
  static bool in_interval(const T& v, const T& lo, const T& hi) {
    return !less(v, lo) && !less(hi, v);
  }
  static bool is_negative(const T&) { return false; }

  static T validate_point(const T& v) { return v; }
  static interval_t<T> validate_interval(const T& lo, const T& hi) {
    if ((!lo.is_01() || !hi.is_01()) && !equal(lo, hi))
      throw std::invalid_argument("Non-point sc_lv interval cannot contain X or Z");
    return less_equal(lo, hi) ? interval_t<T>(lo, hi) : interval_t<T>(hi, lo);
  }

  static T min_value() { return T(("0b" + std::string(static_cast<size_t>(W), '0')).c_str()); }
  static T max_value() { return T(("0b" + std::string(static_cast<size_t>(W), 'X')).c_str()); }
  static T max_01_value() { return T(("0b" + std::string(static_cast<size_t>(W), '1')).c_str()); }

  static bool str_is_01(const std::string& s) {
    for (char c : s) {
      if (c != '0' && c != '1') return false;
    }
    return true;
  }

  static T next(const T& v) {
    std::string s = v.to_string();
    if (v.is_01()) {
      bool wrapped = true;
      for (size_t i = s.size(); i > 0; --i) {
        if (s[i - 1] == '0') {
          s[i - 1] = '1';
          wrapped = false;
          break;
        }
        s[i - 1] = '0';
      }
      if (!wrapped) return T(("0b" + s).c_str());
      std::string first_xz(static_cast<size_t>(W), '0');
      first_xz.back() = 'Z';
      return T(("0b" + first_xz).c_str());
    }
    // Increment base-4 (0 < 1 < Z < X), skipping any pure-01 value
    auto inc_base4 = [&]() {
      for (size_t i = s.size(); i > 0; --i) {
        char c = s[i - 1];
        if (c == '0') { s[i - 1] = '1'; return; }
        if (c == '1') { s[i - 1] = 'Z'; return; }
        if (c == 'Z' || c == 'z') { s[i - 1] = 'X'; return; }
        s[i - 1] = '0';
      }
    };
    inc_base4();
    if (str_is_01(s)) s.back() = 'Z';
    return T(("0b" + s).c_str());
  }

  static T prev(const T& v) {
    std::string s = v.to_string();
    if (v.is_01()) {
      for (size_t i = s.size(); i > 0; --i) {
        if (s[i - 1] == '1') {
          s[i - 1] = '0';
          break;
        }
        s[i - 1] = '1';
      }
      return T(("0b" + s).c_str());
    }
    std::string first_xz(static_cast<size_t>(W), '0');
    first_xz.back() = 'Z';
    if (s == first_xz) {
      return max_01_value();
    }
    auto dec_base4 = [&]() {
      for (size_t i = s.size(); i > 0; --i) {
        char c = s[i - 1];
        if (c == 'X' || c == 'x') { s[i - 1] = 'Z'; return; }
        if (c == 'Z' || c == 'z') { s[i - 1] = '1'; return; }
        if (c == '1') { s[i - 1] = '0'; return; }
        s[i - 1] = 'X';
      }
    };
    dec_base4();
    while (str_is_01(s)) dec_base4();
    return T(("0b" + s).c_str());
  }

  static void auto_bin_bounds(T& lo, T& hi) {
    lo = min_value();
    hi = max_01_value();
  }
  static std::vector<std::pair<T, std::string>> enumerated_values() {
    return std::vector<std::pair<T, std::string>>();
  }

  static detail::big_uint to_01_big_uint(const T& v) {
    return detail::big_uint::from_bits(v.to_string());
  }
  static T from_01_big_uint(const detail::big_uint& b) {
    return T(("0b" + b.to_bits(static_cast<size_t>(W))).c_str());
  }

  static bool try_distance_u64(const T& lo, const T& hi, uint64_t& out) {
    if (less(hi, lo)) return false;
    if (equal(lo, hi)) {
      out = 0;
      return true;
    }
    if (!lo.is_01() || !hi.is_01()) return false;
    detail::big_uint d = detail::big_uint::sub(to_01_big_uint(hi), to_01_big_uint(lo));
    if (!d.fits_u64()) return false;
    out = d.to_u64();
    return true;
  }

  static T add_offset(const T& lo, uint64_t offset) {
    if (offset == 0) return lo;
    detail::big_uint b = to_01_big_uint(lo);
    b.add_u64(offset);
    return from_01_big_uint(b);
  }

  static std::vector<interval_t<T>> split_interval(
      const T& lo, const T& hi, uint64_t count, bool one_per_value_if_fewer = false) {
    if (count == 0)
      throw std::invalid_argument("Interval split count must be at least 1");
    if (!lo.is_01() || !hi.is_01()) {
      if (!equal(lo, hi))
        throw std::invalid_argument("Non-point sc_lv interval cannot contain X or Z");
      return std::vector<interval_t<T>>(1, interval_t<T>(lo, hi));
    }
    detail::big_uint lo_b = to_01_big_uint(lo);
    detail::big_uint hi_b = to_01_big_uint(hi);
    detail::big_uint dist = detail::big_uint::sub(hi_b, lo_b);
    if (dist.fits_u64()) {
      return detail::split_u64_interval<T, fc4sc_type_traits<T>>(
          lo, hi, dist.to_u64(), count, one_per_value_if_fewer);
    }
    std::vector<interval_t<T>> out;
    if (count == 1) {
      out.push_back(interval_t<T>(lo, hi));
      return out;
    }
    detail::big_uint k;
    uint64_t m = 0;
    dist.div_u64(count, k, m);
    detail::big_uint q = k;
    if (m == count - 1) q.add_u64(1);
    detail::big_uint q_minus_1 = detail::big_uint::sub(q, detail::big_uint(1));
    detail::big_uint offset;
    for (uint64_t i = 0; i < count; ++i) {
      T start = from_01_big_uint(detail::big_uint::add(lo_b, offset));
      T end = (i == count - 1)
          ? hi
          : from_01_big_uint(detail::big_uint::add(detail::big_uint::add(lo_b, offset), q_minus_1));
      out.push_back(interval_t<T>(start, end));
      if (i != count - 1) offset = detail::big_uint::add(offset, q);
    }
    return out;
  }

  static uint64_t interval_count(const T& lo, const T& hi, uint64_t limit) {
    if (equal(lo, hi)) return 1;
    uint64_t dist = 0;
    if (!try_distance_u64(lo, hi, dist) || dist >= limit)
      throw std::invalid_argument("Transition expands to too many value sequences");
    return dist + 1;
  }

  static void enumerate_interval(const T& lo, const T& hi, std::vector<T>& out) {
    if (equal(lo, hi)) {
      out.push_back(lo);
      return;
    }
    uint64_t dist = 0;
    if (!try_distance_u64(lo, hi, dist))
      throw std::invalid_argument("Transition interval is too large to enumerate");
    for (uint64_t k = 0; k <= dist; ++k)
      out.push_back(add_offset(lo, k));
  }

  static int to_int(const T& v) {
    // Avoid SystemC SC_ID_VECTOR_CONTAINS_LOGIC_VALUE_ warning on X/Z
    if (!v.is_01()) return static_cast<int>(v.get_word(0));
    return v.to_int();
  }

  static std::string to_string(const T& v) { return v.to_string(); }

  static std::string to_ucis_integer(const T& v) {
    if (v.is_01()) return to_01_big_uint(v).to_dec();
    std::string s = v.to_string();
    detail::big_uint b4;
    for (char c : s) {
      b4 = detail::big_uint::mul_u32(b4, 4U);
      uint64_t d = (c == '0') ? 0ULL : (c == '1') ? 1ULL : (c == 'Z' || c == 'z') ? 2ULL : 3ULL;
      b4.add_u64(d);
    }
    b4.add_u64(1ULL);
    return "-" + b4.to_dec();
  }

  static bool from_ucis_integer(const std::string& s, T& out) {
    bool neg = false;
    std::string digits;
    if (!detail::parse_decimal_digits(s, neg, digits)) return false;
    if (!neg) {
      detail::big_uint b = detail::big_uint::from_dec(digits);
      detail::big_uint max_b = to_01_big_uint(max_01_value());
      if (b.cmp(max_b) > 0) return false;
      out = from_01_big_uint(b);
      return true;
    }
    detail::big_uint b = detail::big_uint::from_dec(digits);
    if (b.is_zero()) return false;
    b = detail::big_uint::sub(b, detail::big_uint(1ULL));
    std::string str(static_cast<size_t>(W), '0');
    for (size_t i = static_cast<size_t>(W); i > 0; --i) {
      detail::big_uint q;
      uint64_t r = 0;
      b.div_u64(4ULL, q, r);
      str[i - 1] = (r == 0) ? '0' : (r == 1) ? '1' : (r == 2) ? 'Z' : 'X';
      b = std::move(q);
    }
    if (!b.is_zero() || str_is_01(str)) return false;
    out = T(("0b" + str).c_str());
    return true;
  }

  static const char* xml_attr_type() { return "str"; }
  static void append_range_user_attrs(
      const T& lo, const T& hi, size_t idx, std::vector<xml_user_attr>& out) {
    if (lo.is_01() && hi.is_01()) return;
    const std::string prefix = "fc4sc.range[" + std::to_string(idx) + "].";
    out.push_back(xml_user_attr{prefix + "from", "str", to_string(lo)});
    out.push_back(xml_user_attr{prefix + "to", "str", to_string(hi)});
  }

  static void append_seq_user_attrs(
      const std::vector<T>& seq, size_t idx, std::vector<xml_user_attr>& out) {
    bool any_xz = false;
    std::string txt;
    for (size_t i = 0; i < seq.size(); ++i) {
      if (!seq[i].is_01()) any_xz = true;
      if (i > 0) txt += " => ";
      txt += to_string(seq[i]);
    }
    if (!any_xz) return;
    out.push_back(xml_user_attr{"fc4sc.seq[" + std::to_string(idx) + "]", "str", txt});
  }
};

// Helper: half_open_interval(lo, hi) -> [lo, prev(hi)]
template <typename T>
inline interval_t<T> half_open_interval(T lo, T hi) {
  typedef fc4sc_type_traits<T> traits;
  static_assert(traits::is_supported, "Unsupported type for half_open_interval");
  lo = traits::validate_point(lo);
  hi = traits::validate_point(hi);
  if (!traits::less(lo, hi))
    throw std::invalid_argument("half_open_interval requires lo < hi");
  if (!traits::is_discrete) {
    return interval_t<T>(lo, traits::prev(hi));
  }
  return interval_t<T>(lo, traits::prev(hi));
}

namespace detail {

template <typename Num, typename Enable = void>
struct num_conv;

template <typename Num>
struct num_conv<Num, typename std::enable_if<std::is_integral<Num>::value>::type> {
  static bool is_nan(const Num&) { return false; }
  static bool is_negative(const Num& v) { return v < Num(0); }
  static bool is_floating() { return false; }
  static bool to_u64_exact(const Num& v, uint64_t& out) {
    if (v < Num(0)) return false;
    out = static_cast<uint64_t>(v);
    return true;
  }
  static bool to_big_uint_exact(const Num& v, big_uint& out) {
    if (v < Num(0)) return false;
    out = big_uint(static_cast<uint64_t>(v));
    return true;
  }
  static long double to_long_double(const Num& v) { return static_cast<long double>(v); }
};

template <typename Num>
struct num_conv<Num, typename std::enable_if<std::is_floating_point<Num>::value>::type> {
  static bool is_nan(const Num& v) { return std::isnan(v); }
  static bool is_negative(const Num& v) { return v < Num(0); }
  static bool is_floating() { return true; }
  static bool to_u64_exact(const Num& v, uint64_t& out) {
    if (v < Num(0) || !std::isfinite(v)) return false;
    long double ld = static_cast<long double>(v);
    if (std::floor(ld) != ld) return false;
    if (ld >= 18446744073709551616.0L) return false;
    out = static_cast<uint64_t>(ld);
    return true;
  }
  static bool to_big_uint_exact(const Num& v, big_uint& out) {
    uint64_t u = 0;
    if (!to_u64_exact(v, u)) return false;
    out = big_uint(u);
    return true;
  }
  static long double to_long_double(const Num& v) { return static_cast<long double>(v); }
};

template <int W>
struct num_conv<sc_dt::sc_int<W>> {
  typedef sc_dt::sc_int<W> Num;
  static bool is_nan(const Num&) { return false; }
  static bool is_negative(const Num& v) { return v.to_int64() < 0; }
  static bool is_floating() { return false; }
  static bool to_u64_exact(const Num& v, uint64_t& out) {
    if (v.to_int64() < 0) return false;
    out = static_cast<uint64_t>(v.to_int64());
    return true;
  }
  static bool to_big_uint_exact(const Num& v, big_uint& out) {
    uint64_t u = 0;
    if (!to_u64_exact(v, u)) return false;
    out = big_uint(u);
    return true;
  }
  static long double to_long_double(const Num& v) { return static_cast<long double>(v.to_int64()); }
};

template <int W>
struct num_conv<sc_dt::sc_uint<W>> {
  typedef sc_dt::sc_uint<W> Num;
  static bool is_nan(const Num&) { return false; }
  static bool is_negative(const Num&) { return false; }
  static bool is_floating() { return false; }
  static bool to_u64_exact(const Num& v, uint64_t& out) {
    out = v.to_uint64();
    return true;
  }
  static bool to_big_uint_exact(const Num& v, big_uint& out) {
    out = big_uint(v.to_uint64());
    return true;
  }
  static long double to_long_double(const Num& v) { return static_cast<long double>(v.to_uint64()); }
};

template <int W>
struct num_conv<sc_dt::sc_bigint<W>> {
  typedef sc_dt::sc_bigint<W> Num;
  static bool is_nan(const Num&) { return false; }
  static bool is_negative(const Num& v) { return v < 0; }
  static bool is_floating() { return false; }
  static bool to_big_uint_exact(const Num& v, big_uint& out) {
    if (v < 0) return false;
    out = big_uint::from_dec(v.to_string());
    return true;
  }
  static bool to_u64_exact(const Num& v, uint64_t& out) {
    big_uint b;
    if (!to_big_uint_exact(v, b) || !b.fits_u64()) return false;
    out = b.to_u64();
    return true;
  }
  static long double to_long_double(const Num& v) { return static_cast<long double>(v.to_double()); }
};

template <int W>
struct num_conv<sc_dt::sc_biguint<W>> {
  typedef sc_dt::sc_biguint<W> Num;
  static bool is_nan(const Num&) { return false; }
  static bool is_negative(const Num&) { return false; }
  static bool is_floating() { return false; }
  static bool to_big_uint_exact(const Num& v, big_uint& out) {
    out = big_uint::from_dec(v.to_string());
    return true;
  }
  static bool to_u64_exact(const Num& v, uint64_t& out) {
    big_uint b;
    if (!to_big_uint_exact(v, b) || !b.fits_u64()) return false;
    out = b.to_u64();
    return true;
  }
  static long double to_long_double(const Num& v) { return static_cast<long double>(v.to_double()); }
};

inline void decompose_nonneg_ld(long double x, big_uint& mant, int& shift) {
  mant = big_uint();
  shift = 0;
  if (x <= 0.0L || !std::isfinite(x)) return;
  int exp = 0;
  long double frac = std::frexp(x, &exp);
  std::vector<uint32_t> msb_limbs;
  for (int iter = 0; iter < 16 && frac > 0.0L; ++iter) {
    frac = std::ldexp(frac, 32);
    long double ip = std::floor(frac);
    uint32_t limb = static_cast<uint32_t>(ip);
    frac -= ip;
    msb_limbs.push_back(limb);
    exp -= 32;
  }
  mant.limbs.resize(msb_limbs.size());
  for (size_t i = 0; i < msb_limbs.size(); ++i) {
    mant.limbs[i] = msb_limbs[msb_limbs.size() - 1 - i];
  }
  mant.trim();
  shift = exp;
}

template <typename T, typename Tol>
inline interval_t<T> compute_tolerance_impl(const T& val, const Tol& tol, std::true_type /*is_floating*/) {
  typedef fc4sc_type_traits<T> traits;
  if (traits::is_nan(val) || num_conv<Tol>::is_nan(tol) || num_conv<Tol>::is_negative(tol))
    throw std::invalid_argument("tolerance requires non-NaN value and non-negative tolerance");
  const T cval = traits::canonicalize(val);
  const T dtol = static_cast<T>(num_conv<Tol>::to_long_double(tol));
  const T lo = traits::canonicalize(cval - dtol);
  const T hi = traits::canonicalize(cval + dtol);
  if (traits::is_nan(lo) || traits::is_nan(hi))
    throw std::invalid_argument("tolerance produced NaN interval");
  return interval_t<T>(lo, hi);
}

template <typename T, typename Tol>
inline interval_t<T> compute_tolerance_discrete_impl(
    const T& val, const Tol& tol, std::false_type /*width <= 64*/) {
  typedef fc4sc_type_traits<T> traits;
  const T min_v = traits::min_value();
  const T max_v = traits::max_value();
  uint64_t d_down = 0;
  uint64_t d_up = 0;
  traits::try_distance_u64(min_v, val, d_down);
  traits::try_distance_u64(val, max_v, d_up);
  bool sat = false;
  uint64_t delta = 0;
  if (num_conv<Tol>::is_floating()) {
    long double f = std::floor(num_conv<Tol>::to_long_double(tol));
    if (f >= 18446744073709551616.0L)
      sat = true;
    else
      delta = static_cast<uint64_t>(f);
  } else if (!num_conv<Tol>::to_u64_exact(tol, delta)) {
    sat = true;
  }
  T lo = (sat || delta >= d_down) ? min_v : traits::sub_offset(val, delta);
  T hi = (sat || delta >= d_up) ? max_v : traits::add_offset(val, delta);
  return interval_t<T>(lo, hi);
}

template <typename T, typename Tol>
inline interval_t<T> compute_tolerance_discrete_impl(
    const T& val, const Tol& tol, std::true_type /*width > 64*/) {
  typedef fc4sc_type_traits<T> traits;
  const T min_v = traits::min_value();
  const T max_v = traits::max_value();
  big_uint bd_down = traits::distance_big(min_v, val);
  big_uint bd_up = traits::distance_big(val, max_v);
  big_uint bdelta;
  bool sat = false;
  if (num_conv<Tol>::is_floating()) {
    long double f = std::floor(num_conv<Tol>::to_long_double(tol));
    if (!std::isfinite(f)) {
      sat = true;
    } else if (f > 0.0L) {
      big_uint max_span = (bd_down.cmp(bd_up) >= 0) ? bd_down : bd_up;
      big_uint mant;
      int shift = 0;
      decompose_nonneg_ld(f, mant, shift);
      const int64_t flen = static_cast<int64_t>(mant.bit_length()) + static_cast<int64_t>(shift);
      if (flen > static_cast<int64_t>(max_span.bit_length())) {
        sat = true;
      } else if (shift >= 0) {
        bdelta = mant.shl_bits(static_cast<size_t>(shift));
      } else {
        bdelta = mant.shr_bits(static_cast<size_t>(-shift));
      }
    }
  } else if (!num_conv<Tol>::to_big_uint_exact(tol, bdelta)) {
    sat = true;
  }
  T lo = (sat || bdelta.cmp(bd_down) >= 0) ? min_v : traits::sub_big_offset(val, bdelta);
  T hi = (sat || bdelta.cmp(bd_up) >= 0) ? max_v : traits::add_big_offset(val, bdelta);
  return interval_t<T>(lo, hi);
}

template <typename T, typename Tol>
inline interval_t<T> compute_tolerance_impl(const T& val, const Tol& tol, std::false_type /*is_floating*/) {
  if (num_conv<Tol>::is_nan(tol) || num_conv<Tol>::is_negative(tol))
    throw std::invalid_argument("tolerance requires non-negative non-NaN tolerance");
  return compute_tolerance_discrete_impl(
      val, tol, std::integral_constant<bool, (fc4sc_type_traits<T>::bit_width > 64)>());
}

template <typename T, typename Pct>
inline interval_t<T> compute_percent_tolerance_impl(
    const T& val, const Pct& pct, std::true_type /*is_floating*/) {
  typedef fc4sc_type_traits<T> traits;
  if (traits::is_nan(val) || num_conv<Pct>::is_nan(pct) || num_conv<Pct>::is_negative(pct))
    throw std::invalid_argument("percent_tolerance requires non-NaN value and non-negative percentage");
  const T cval = traits::canonicalize(val);
  const long double pct_ld = num_conv<Pct>::to_long_double(pct);
  const T delta = (pct_ld == 0.0L)
      ? T(0)
      : static_cast<T>(std::abs(static_cast<long double>(cval)) * (pct_ld / 100.0L));
  const T lo = traits::canonicalize(cval - delta);
  const T hi = traits::canonicalize(cval + delta);
  if (traits::is_nan(lo) || traits::is_nan(hi))
    throw std::invalid_argument("percent_tolerance produced NaN interval");
  return interval_t<T>(lo, hi);
}

template <typename T, typename Pct>
inline interval_t<T> compute_percent_tolerance_discrete_impl(
    const T& val, const Pct& pct, std::false_type /*width <= 64*/) {
  typedef fc4sc_type_traits<T> traits;
  const T min_v = traits::min_value();
  const T max_v = traits::max_value();
  uint64_t d_down = 0;
  uint64_t d_up = 0;
  traits::try_distance_u64(min_v, val, d_down);
  traits::try_distance_u64(val, max_v, d_up);

  const T zero = T(0);
  uint64_t mag = 0;
  if (traits::less(val, zero))
    traits::try_distance_u64(val, zero, mag);
  else
    traits::try_distance_u64(zero, val, mag);

  bool sat = false;
  uint64_t delta = 0;
  uint64_t exact_pct = 0;
  if (num_conv<Pct>::to_u64_exact(pct, exact_pct)) {
    delta = percent_delta_u64(mag, exact_pct, sat);
  } else if (num_conv<Pct>::is_floating()) {
    long double pct_ld = num_conv<Pct>::to_long_double(pct);
    if (mag == 0 || pct_ld == 0.0L) {
      delta = 0;
    } else if (!std::isfinite(pct_ld)) {
      sat = true;
    } else {
      big_uint pmant;
      int pshift = 0;
      decompose_nonneg_ld(pct_ld, pmant, pshift);
      big_uint prod = big_uint::mul_u64(pmant, mag);
      if (pshift >= 0) {
        const int64_t plen = static_cast<int64_t>(prod.bit_length()) + static_cast<int64_t>(pshift);
        if (plen > 71) {
          sat = true;
        } else {
          prod = prod.shl_bits(static_cast<size_t>(pshift));
          big_uint q;
          uint64_t rem = 0;
          prod.div_u64(100ULL, q, rem);
          if (!q.fits_u64())
            sat = true;
          else
            delta = q.to_u64();
        }
      } else {
        prod = prod.shr_bits(static_cast<size_t>(-pshift));
        big_uint q;
        uint64_t rem = 0;
        prod.div_u64(100ULL, q, rem);
        if (!q.fits_u64())
          sat = true;
        else
          delta = q.to_u64();
      }
    }
  } else {
    big_uint bpct;
    if (mag == 0) {
      delta = 0;
    } else if (num_conv<Pct>::to_big_uint_exact(pct, bpct)) {
      big_uint q;
      uint64_t rem = 0;
      big_uint::mul_u64(bpct, mag).div_u64(100ULL, q, rem);
      if (!q.fits_u64())
        sat = true;
      else
        delta = q.to_u64();
    } else {
      sat = true;
    }
  }
  T lo = (sat || delta >= d_down) ? min_v : traits::sub_offset(val, delta);
  T hi = (sat || delta >= d_up) ? max_v : traits::add_offset(val, delta);
  return interval_t<T>(lo, hi);
}

template <typename T, typename Pct>
inline interval_t<T> compute_percent_tolerance_discrete_impl(
    const T& val, const Pct& pct, std::true_type /*width > 64*/) {
  typedef fc4sc_type_traits<T> traits;
  const T min_v = traits::min_value();
  const T max_v = traits::max_value();
  const T zero = T(0);
  big_uint bd_down = traits::distance_big(min_v, val);
  big_uint bd_up = traits::distance_big(val, max_v);
  big_uint bmag = traits::less(val, zero)
      ? traits::distance_big(val, zero)
      : traits::distance_big(zero, val);

  big_uint bdelta;
  bool sat = false;
  uint64_t upct = 0;
  if (num_conv<Pct>::to_u64_exact(pct, upct)) {
    big_uint q;
    uint64_t r = 0;
    big_uint::mul_u64(bmag, upct).div_u64(100ULL, q, r);
    bdelta = std::move(q);
  } else if (num_conv<Pct>::is_floating()) {
    long double pct_ld = num_conv<Pct>::to_long_double(pct);
    if (bmag.is_zero() || pct_ld == 0.0L) {
      bdelta = big_uint();
    } else if (!std::isfinite(pct_ld)) {
      sat = true;
    } else {
      big_uint max_span = (bd_down.cmp(bd_up) >= 0) ? bd_down : bd_up;
      big_uint pmant;
      int pshift = 0;
      decompose_nonneg_ld(pct_ld, pmant, pshift);
      big_uint prod = big_uint::mul(bmag, pmant);
      if (pshift >= 0) {
        const int64_t plen = static_cast<int64_t>(prod.bit_length()) + static_cast<int64_t>(pshift);
        if (plen > static_cast<int64_t>(max_span.bit_length()) + 7) {
          sat = true;
        } else {
          prod = prod.shl_bits(static_cast<size_t>(pshift));
          uint64_t rem = 0;
          prod.div_u64(100ULL, bdelta, rem);
        }
      } else {
        prod = prod.shr_bits(static_cast<size_t>(-pshift));
        uint64_t rem = 0;
        prod.div_u64(100ULL, bdelta, rem);
      }
    }
  } else {
    big_uint bpct;
    if (bmag.is_zero()) {
      bdelta = big_uint();
    } else if (num_conv<Pct>::to_big_uint_exact(pct, bpct)) {
      uint64_t rem = 0;
      big_uint::mul(bmag, bpct).div_u64(100ULL, bdelta, rem);
    } else {
      sat = true;
    }
  }
  T lo = (sat || bdelta.cmp(bd_down) >= 0) ? min_v : traits::sub_big_offset(val, bdelta);
  T hi = (sat || bdelta.cmp(bd_up) >= 0) ? max_v : traits::add_big_offset(val, bdelta);
  return interval_t<T>(lo, hi);
}

template <typename T, typename Pct>
inline interval_t<T> compute_percent_tolerance_impl(
    const T& val, const Pct& pct, std::false_type /*is_floating*/) {
  if (num_conv<Pct>::is_nan(pct) || num_conv<Pct>::is_negative(pct))
    throw std::invalid_argument("percent_tolerance requires non-negative non-NaN percentage");
  return compute_percent_tolerance_discrete_impl(
      val, pct, std::integral_constant<bool, (fc4sc_type_traits<T>::bit_width > 64)>());
}

} // namespace detail

/*!
 * \brief Returns closed tolerance interval [val - tol, val + tol] clamped to [min, max].
 */
template <typename T, typename Tol = T>
inline interval_t<T> tolerance(T val, Tol tol) {
  static_assert(fc4sc_type_traits<T>::supports_tolerance,
                "tolerance requires a numeric coverpoint type");
  static_assert(fc4sc_type_traits<Tol>::supports_tolerance,
                "tolerance amount must be a numeric type");
  return detail::compute_tolerance_impl(
      val, tol, std::integral_constant<bool, fc4sc_type_traits<T>::is_floating>());
}

/*!
 * \brief Returns closed percent tolerance interval [val - |val|*pct/100, val + |val|*pct/100] clamped to [min, max].
 */
template <typename T, typename Pct = double>
inline interval_t<T> percent_tolerance(T val, Pct pct) {
  static_assert(fc4sc_type_traits<T>::supports_tolerance,
                "percent_tolerance requires a numeric coverpoint type");
  static_assert(fc4sc_type_traits<Pct>::supports_tolerance,
                "percent_tolerance percentage must be a numeric type");
  return detail::compute_percent_tolerance_impl(
      val, pct, std::integral_constant<bool, fc4sc_type_traits<T>::is_floating>());
}

} // namespace fc4sc

/*!
 * \brief Registers enumerators of an enum or enum class for automatic 1-bin-per-enumerator auto-bins.
 */
#define FC4SC_ENUM_VALUES(EnumType, ...)                                        \
  namespace fc4sc {                                                             \
  template <>                                                                   \
  struct fc4sc_enum_traits<EnumType> {                                          \
    static constexpr bool is_specialized = true;                                \
    static std::vector<EnumType> values() {                                     \
      return std::vector<EnumType>{__VA_ARGS__};                                \
    }                                                                           \
    static std::vector<std::pair<EnumType, std::string>> named_values() {       \
      return fc4sc::detail::parse_enum_names<EnumType>(#__VA_ARGS__, values()); \
    }                                                                           \
  };                                                                            \
  }

#endif /* FC4SC_TYPE_TRAITS_HPP */
