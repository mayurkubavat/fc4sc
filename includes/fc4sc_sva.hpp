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
 * \file fc4sc_sva.hpp
 * \brief Optional Preponed-sampled SystemC temporal assertion & cover engine.
 *
 * Implements FC4SC_ASSERT_PROPERTY / FC4SC_COVER_PROPERTY using SystemC 3.0.3
 * sc_stage_callback_if (SC_POST_START_OF_SIMULATION | SC_PRE_TIMESTEP |
 * SC_PRE_STOP | SC_PRE_PAUSE) with T_{k-1} Preponed snapshots and Thompson-NFA
 * temporal sequence evaluation, exporting UCIS <assertionCoverage>.
 * Requires C++17 / SystemC 3.0.3 and is excluded from includes/fc4sc.hpp.
 */

#ifndef FC4SC_SVA_HPP
#define FC4SC_SVA_HPP

#include <algorithm>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <systemc>

#include "fc4sc.hpp"

namespace fc4sc {

struct sva_options {
  fc4sc::scp_base* scope = nullptr;
  fc4sc::global* cntxt = nullptr;
  std::string file;
  uint32_t line = 0;
  sc_core::sc_severity severity = sc_core::SC_ERROR;
  bool no_throw = false;
  unsigned int weight = 1;
  std::string name_component;
  std::string type_component;
};

namespace sva {
namespace detail {

inline uint64_t next_sva_token() {
  static uint64_t seq = 1;
  return seq++;
}

inline std::vector<std::pair<std::string, std::string>> sva_scope_path(
    const sc_core::sc_object* obj)
{
  std::vector<std::pair<std::string, std::string>> rev;
  if (obj == nullptr) return rev;
  const sc_core::sc_object* cur = obj->get_parent_object();
  while (cur != nullptr) {
    const char* bname = cur->basename();
    const char* k = cur->kind();
    rev.emplace_back(bname ? std::string(bname) : std::string("module"),
                     k ? std::string(k) : std::string("sc_module"));
    cur = cur->get_parent_object();
  }
  std::reverse(rev.begin(), rev.end());
  return rev;
}

inline scp_base_data_model* resolve_sva_scope(
    const sc_core::sc_object* obj,
    const sva_options& opts,
    const std::string& fallback_file,
    uint32_t fallback_line)
{
  if (opts.scope != nullptr) {
    return opts.scope->get_scp_data();
  }
  fc4sc::global* cntxt = (opts.cntxt != nullptr) ? opts.cntxt : fc4sc::global::getter();
  const std::string f = opts.file.empty() ? fallback_file : opts.file;
  const uint32_t l = (opts.line > 0) ? opts.line : (fallback_line > 0 ? fallback_line : 1U);
  return fc4sc::global::resolve_scope_path(sva_scope_path(obj), f, l, cntxt);
}

struct sc_obj_guard {
  sc_core::sc_simcontext* simc = nullptr;
  sc_core::sc_object_manager* obj_mgr = nullptr;
  sc_core::sc_object* sc_obj = nullptr;
  std::string sc_name;
  std::string attr_name;
  std::unique_ptr<sc_core::sc_attribute<uint64_t>> token_attr;

  void bind(const sc_core::sc_object* obj) {
    if (obj == nullptr) return;
    sc_core::sc_simcontext* cur = sc_core::sc_curr_simcontext;
    if (cur == nullptr) cur = sc_core::sc_get_curr_simcontext();
    simc = cur;
    obj_mgr = (cur != nullptr) ? cur->get_object_manager() : nullptr;
    sc_obj = const_cast<sc_core::sc_object*>(obj);
    sc_name = obj->name() ? std::string(obj->name()) : std::string();
    const uint64_t tok = next_sva_token();
    attr_name = "__fc4sc_sva_tok_" + std::to_string(tok);
    token_attr.reset(new sc_core::sc_attribute<uint64_t>(attr_name));
    token_attr->value = tok;
    sc_obj->add_attribute(*token_attr);
  }

  bool is_alive() const {
    if (sc_obj == nullptr) return true;
    sc_core::sc_simcontext* cur = sc_core::sc_curr_simcontext;
    if (cur == nullptr || cur != simc) return false;
    if (cur->get_object_manager() != obj_mgr) return false;
    if (sc_core::sc_find_object(sc_name.c_str()) != sc_obj) return false;
    if (token_attr != nullptr && sc_obj->get_attribute(attr_name) != token_attr.get()) {
      return false;
    }
    return true;
  }

  void detach_if_alive() {
    if (sc_obj != nullptr && token_attr != nullptr) {
      sc_core::sc_simcontext* cur = sc_core::sc_curr_simcontext;
      if (cur != nullptr && cur == simc && cur->get_object_manager() == obj_mgr &&
          sc_core::sc_find_object(sc_name.c_str()) == sc_obj &&
          sc_obj->get_attribute(attr_name) == token_attr.get()) {
        sc_obj->remove_attribute(attr_name);
      }
    }
    token_attr.reset();
  }
};

struct sampler_slot_base {
  virtual ~sampler_slot_base() = default;
  virtual bool is_alive() const = 0;
  virtual void detach_if_alive() = 0;
  virtual void init_pending() = 0;
  virtual void shift_on_clock() = 0;
  virtual void refresh_pending() = 0;
  virtual void set_eval_pending(bool use_pending) = 0;
  virtual void ensure_history_depth(size_t depth) = 0;
};

template <typename T>
struct typed_sampler_slot final : public sampler_slot_base {
  std::function<bool(T&)> reader;
  sc_obj_guard guard;
  T pending_val{};
  bool has_pending = false;
  bool eval_pending = false;
  std::deque<T> history;
  size_t max_history = 16;

  bool is_alive() const override { return guard.is_alive(); }
  void detach_if_alive() override { guard.detach_if_alive(); }

  void init_pending() override {
    T tmp{};
    if (reader && reader(tmp)) {
      pending_val = std::move(tmp);
      has_pending = true;
    }
  }

  void shift_on_clock() override {
    if (!has_pending) init_pending();
    history.push_front(pending_val);
    if (history.size() > max_history) {
      history.pop_back();
    }
  }

  void refresh_pending() override {
    T tmp{};
    if (reader && reader(tmp)) {
      pending_val = std::move(tmp);
      has_pending = true;
    }
  }

  void set_eval_pending(bool use_pending) override {
    eval_pending = use_pending;
  }

  void ensure_history_depth(size_t depth) override {
    if (depth > max_history) max_history = depth;
  }

  T current() const {
    if (eval_pending) return pending_val;
    if (!history.empty()) return history[0];
    return pending_val;
  }

  T past(size_t n) const {
    if (eval_pending) {
      if (n == 0) return pending_val;
      return (n - 1 < history.size()) ? history[n - 1] : T{};
    }
    return (n < history.size()) ? history[n] : T{};
  }

  bool has_past(size_t n) const {
    if (eval_pending) {
      if (n == 0) return has_pending;
      return (n - 1) < history.size();
    }
    return n < history.size();
  }

  size_t history_size() const { return history.size(); }
};

struct pred_sampler_slot final : public sampler_slot_base {
  std::function<bool()> reader;
  bool pending_val = false;
  bool has_pending = false;
  bool eval_pending = false;
  std::deque<bool> history;
  size_t max_history = 16;

  bool is_alive() const override { return true; }
  void detach_if_alive() override {}

  void init_pending() override {
    if (reader) {
      pending_val = reader();
      has_pending = true;
    }
  }

  void shift_on_clock() override {
    if (!has_pending) init_pending();
    history.push_front(pending_val);
    if (history.size() > max_history) history.pop_back();
  }

  void refresh_pending() override {
    if (reader) {
      pending_val = reader();
      has_pending = true;
    }
  }

  void set_eval_pending(bool use_pending) override { eval_pending = use_pending; }
  void ensure_history_depth(size_t depth) override {
    if (depth > max_history) max_history = depth;
  }

  bool current() const {
    if (eval_pending) return pending_val;
    if (!history.empty()) return history[0];
    return pending_val;
  }
};

template <typename T>
inline char lsb_char(const T& v) {
  using U = typename std::decay<T>::type;
  if constexpr (std::is_same<U, bool>::value) {
    return v ? '1' : '0';
  } else if constexpr (std::is_same<U, sc_dt::sc_logic>::value) {
    char c = v.to_char();
    if (c == 'x') c = 'X';
    if (c == 'z') c = 'Z';
    return c;
  } else if constexpr (std::is_integral<U>::value || std::is_enum<U>::value) {
    return (static_cast<uint64_t>(v) & 1ULL) ? '1' : '0';
  } else if constexpr (std::is_base_of<sc_dt::sc_int_base, U>::value ||
                       std::is_base_of<sc_dt::sc_uint_base, U>::value ||
                       std::is_base_of<sc_dt::sc_signed, U>::value ||
                       std::is_base_of<sc_dt::sc_unsigned, U>::value) {
    return v.test(0) ? '1' : '0';
  } else if constexpr (std::is_base_of<sc_dt::sc_lv_base, U>::value) {
    static const char kMap[4] = {'0', '1', 'Z', 'X'};
    return kMap[static_cast<unsigned int>(v.get_bit(0)) & 3U];
  } else if constexpr (std::is_base_of<sc_dt::sc_bv_base, U>::value) {
    return v.get_bit(0) ? '1' : '0';
  } else {
    return static_cast<bool>(v[0]) ? '1' : '0';
  }
}

template <int W>
inline char lsb_char(const sc_dt::sc_lv<W>& v) {
  static const char kMap[4] = {'0', '1', 'Z', 'X'};
  return kMap[static_cast<unsigned int>(v.get_bit(0)) & 3U];
}

template <int W>
inline char lsb_char(const sc_dt::sc_bv<W>& v) {
  return v.get_bit(0) ? '1' : '0';
}

template <int W>
inline char lsb_char(const sc_dt::sc_int<W>& v) {
  return v.test(0) ? '1' : '0';
}

template <int W>
inline char lsb_char(const sc_dt::sc_uint<W>& v) {
  return v.test(0) ? '1' : '0';
}

template <int W>
inline char lsb_char(const sc_dt::sc_bigint<W>& v) {
  return v.test(0) ? '1' : '0';
}

template <int W>
inline char lsb_char(const sc_dt::sc_biguint<W>& v) {
  return v.test(0) ? '1' : '0';
}

template <typename T>
inline bool expr_to_bool(const T& v) {
  using U = typename std::decay<T>::type;
  if constexpr (std::is_same<U, bool>::value) {
    return v;
  } else if constexpr (std::is_same<U, sc_dt::sc_logic>::value) {
    return v == sc_dt::SC_LOGIC_1;
  } else if constexpr (std::is_base_of<sc_dt::sc_lv_base, U>::value) {
    return v.or_reduce() == sc_dt::Log_1;
  } else if constexpr (std::is_base_of<sc_dt::sc_bv_base, U>::value) {
    return v.or_reduce();
  } else if constexpr (std::is_base_of<sc_dt::sc_int_base, U>::value ||
                       std::is_base_of<sc_dt::sc_uint_base, U>::value ||
                       std::is_base_of<sc_dt::sc_signed, U>::value ||
                       std::is_base_of<sc_dt::sc_unsigned, U>::value) {
    return v != 0;
  } else {
    return static_cast<bool>(v);
  }
}

inline void merge_deps(std::vector<std::shared_ptr<sampler_slot_base>>& dst,
                       const std::vector<std::shared_ptr<sampler_slot_base>>& src) {
  for (size_t i = 0; i < src.size(); ++i) {
    if (std::find(dst.begin(), dst.end(), src[i]) == dst.end()) {
      dst.push_back(src[i]);
    }
  }
}

} // namespace detail

enum class edge_kind {
  posedge = 0,
  negedge = 1,
  anyedge = 2
};

class clock_spec {
public:
  edge_kind kind = edge_kind::posedge;
  const sc_core::sc_object* sc_obj = nullptr;
  std::function<bool(char&)> read_lsb;
  char prev_val = 'X';
  bool initialized = false;

  void init() {
    char c = 'X';
    if (read_lsb && read_lsb(c)) {
      prev_val = c;
      initialized = true;
    }
  }

  bool check_edge_and_update() {
    char curr = 'X';
    if (!read_lsb || !read_lsb(curr)) return false;
    if (!initialized) {
      prev_val = curr;
      initialized = true;
      return false;
    }
    bool hit = false;
    switch (kind) {
      case edge_kind::posedge:
        hit = (prev_val != '1' && curr == '1');
        break;
      case edge_kind::negedge:
        hit = (prev_val != '0' && curr == '0');
        break;
      case edge_kind::anyedge:
        hit = (prev_val != curr);
        break;
    }
    prev_val = curr;
    return hit;
  }

  void refresh_level() {
    char curr = 'X';
    if (read_lsb && read_lsb(curr)) {
      prev_val = curr;
      initialized = true;
    }
  }
};

template <typename T>
inline clock_spec make_clock(const sc_core::sc_signal_in_if<T>& sig, edge_kind ek) {
  clock_spec spec;
  spec.kind = ek;
  const sc_core::sc_signal_in_if<T>* sig_ptr = &sig;
  spec.sc_obj = dynamic_cast<const sc_core::sc_object*>(&sig);
  spec.read_lsb = [sig_ptr](char& out) -> bool {
    out = detail::lsb_char(sig_ptr->read());
    return true;
  };
  return spec;
}

template <typename T>
inline clock_spec make_clock(const sc_core::sc_port_b<sc_core::sc_signal_in_if<T>>& port, edge_kind ek) {
  clock_spec spec;
  spec.kind = ek;
  const sc_core::sc_port_b<sc_core::sc_signal_in_if<T>>* port_ptr = &port;
  spec.sc_obj = &port;
  spec.read_lsb = [port_ptr](char& out) -> bool {
    const auto* iface = dynamic_cast<const sc_core::sc_signal_in_if<T>*>(port_ptr->get_interface());
    if (iface == nullptr) return false;
    out = detail::lsb_char(iface->read());
    return true;
  };
  return spec;
}

template <typename T>
inline clock_spec make_clock(const sc_core::sc_port_b<sc_core::sc_signal_inout_if<T>>& port, edge_kind ek) {
  clock_spec spec;
  spec.kind = ek;
  const sc_core::sc_port_b<sc_core::sc_signal_inout_if<T>>* port_ptr = &port;
  spec.sc_obj = &port;
  spec.read_lsb = [port_ptr](char& out) -> bool {
    const auto* iface = dynamic_cast<const sc_core::sc_signal_in_if<T>*>(port_ptr->get_interface());
    if (iface == nullptr) return false;
    out = detail::lsb_char(iface->read());
    return true;
  };
  return spec;
}

template <typename T>
inline clock_spec make_clock(const sc_core::sc_export<sc_core::sc_signal_in_if<T>>& exp, edge_kind ek) {
  clock_spec spec;
  spec.kind = ek;
  const sc_core::sc_export<sc_core::sc_signal_in_if<T>>* exp_ptr = &exp;
  spec.sc_obj = &exp;
  spec.read_lsb = [exp_ptr](char& out) -> bool {
    const auto* iface = dynamic_cast<const sc_core::sc_signal_in_if<T>*>(exp_ptr->get_interface());
    if (iface == nullptr) return false;
    out = detail::lsb_char(iface->read());
    return true;
  };
  return spec;
}

template <typename T>
inline clock_spec make_clock(const sc_core::sc_export<sc_core::sc_signal_inout_if<T>>& exp, edge_kind ek) {
  clock_spec spec;
  spec.kind = ek;
  const sc_core::sc_export<sc_core::sc_signal_inout_if<T>>* exp_ptr = &exp;
  spec.sc_obj = &exp;
  spec.read_lsb = [exp_ptr](char& out) -> bool {
    const auto* iface = dynamic_cast<const sc_core::sc_signal_in_if<T>*>(exp_ptr->get_interface());
    if (iface == nullptr) return false;
    out = detail::lsb_char(iface->read());
    return true;
  };
  return spec;
}

template <typename S>
inline clock_spec posedge(const S& s) { return make_clock(s, edge_kind::posedge); }

template <typename S>
inline clock_spec negedge(const S& s) { return make_clock(s, edge_kind::negedge); }

template <typename S>
inline clock_spec edge(const S& s) { return make_clock(s, edge_kind::anyedge); }

class pred_expr;
class seq_expr;
class prop_expr;

template <typename T>
class expr {
public:
  std::function<T()> eval;
  std::vector<std::shared_ptr<detail::sampler_slot_base>> deps;
  std::shared_ptr<detail::typed_sampler_slot<T>> direct_slot;

  T operator()() const { return eval ? eval() : T{}; }

  operator pred_expr() const;
  operator seq_expr() const;
  operator prop_expr() const;

  seq_expr then(unsigned int n, const seq_expr& next) const;
  seq_expr then(unsigned int min_d, unsigned int max_d, const seq_expr& next) const;
  seq_expr then_unbounded(unsigned int min_d, const seq_expr& next) const;
  seq_expr repeat(unsigned int count) const;
  seq_expr repeat(unsigned int min_r, unsigned int max_r) const;
  seq_expr repeat_unbounded(unsigned int min_r) const;
  prop_expr implies(const seq_expr& con) const;
  prop_expr next_implies(const seq_expr& con) const;
  prop_expr disable_iff(const pred_expr& cond) const;
};

struct delay_tag {
  unsigned int min_d = 1;
  unsigned int max_d = 1;
  bool unbounded = false;
};

inline delay_tag d(unsigned int n) { return delay_tag{n, n, false}; }
inline delay_tag d(unsigned int min_d, unsigned int max_d) { return delay_tag{min_d, max_d, false}; }
inline delay_tag d_unbounded(unsigned int min_d) { return delay_tag{min_d, min_d, true}; }

class seq_expr {
public:
  enum class kind_t {
    atom = 0,
    concat = 1,
    repeat = 2,
    disj = 3
  };

  struct node {
    kind_t kind = kind_t::atom;
    std::function<bool()> pred;
    std::shared_ptr<node> lhs;
    std::shared_ptr<node> rhs;
    unsigned int min_cnt = 1;
    unsigned int max_cnt = 1;
    bool unbounded = false;
  };

  std::shared_ptr<node> root;
  std::vector<std::shared_ptr<detail::sampler_slot_base>> deps;

  seq_expr() = default;
  seq_expr(std::shared_ptr<node> r, std::vector<std::shared_ptr<detail::sampler_slot_base>> d)
      : root(std::move(r)), deps(std::move(d)) {}

  seq_expr then(unsigned int n, const seq_expr& next) const {
    return then(n, n, next);
  }

  seq_expr then(unsigned int min_d, unsigned int max_d, const seq_expr& next) const {
    auto n = std::make_shared<node>();
    n->kind = kind_t::concat;
    n->lhs = root;
    n->rhs = next.root;
    n->min_cnt = min_d;
    n->max_cnt = (max_d < min_d) ? min_d : max_d;
    n->unbounded = false;
    auto merged = deps;
    detail::merge_deps(merged, next.deps);
    return seq_expr(std::move(n), std::move(merged));
  }

  seq_expr then_unbounded(unsigned int min_d, const seq_expr& next) const {
    auto n = std::make_shared<node>();
    n->kind = kind_t::concat;
    n->lhs = root;
    n->rhs = next.root;
    n->min_cnt = min_d;
    n->max_cnt = min_d;
    n->unbounded = true;
    auto merged = deps;
    detail::merge_deps(merged, next.deps);
    return seq_expr(std::move(n), std::move(merged));
  }

  seq_expr repeat(unsigned int count) const {
    return repeat(count, count);
  }

  seq_expr repeat(unsigned int min_r, unsigned int max_r) const {
    auto n = std::make_shared<node>();
    n->kind = kind_t::repeat;
    n->lhs = root;
    n->min_cnt = min_r;
    n->max_cnt = (max_r < min_r) ? min_r : max_r;
    n->unbounded = false;
    return seq_expr(std::move(n), deps);
  }

  seq_expr repeat_unbounded(unsigned int min_r) const {
    auto n = std::make_shared<node>();
    n->kind = kind_t::repeat;
    n->lhs = root;
    n->min_cnt = min_r;
    n->max_cnt = min_r;
    n->unbounded = true;
    return seq_expr(std::move(n), deps);
  }

  prop_expr implies(const seq_expr& con) const;
  prop_expr next_implies(const seq_expr& con) const;
};

struct pending_seq_delay {
  seq_expr lhs;
  delay_tag tag;
};

inline pending_seq_delay operator>>(const seq_expr& lhs, const delay_tag& tag) {
  return pending_seq_delay{lhs, tag};
}

inline seq_expr operator>>(const pending_seq_delay& lhs, const seq_expr& rhs) {
  if (lhs.tag.unbounded) return lhs.lhs.then_unbounded(lhs.tag.min_d, rhs);
  return lhs.lhs.then(lhs.tag.min_d, lhs.tag.max_d, rhs);
}

inline seq_expr operator||(const seq_expr& a, const seq_expr& b) {
  auto n = std::make_shared<seq_expr::node>();
  n->kind = seq_expr::kind_t::disj;
  n->lhs = a.root;
  n->rhs = b.root;
  auto merged = a.deps;
  detail::merge_deps(merged, b.deps);
  return seq_expr(std::move(n), std::move(merged));
}

class pred_expr {
public:
  std::function<bool()> eval;
  std::vector<std::shared_ptr<detail::sampler_slot_base>> deps;

  operator seq_expr() const {
    auto n = std::make_shared<seq_expr::node>();
    n->kind = seq_expr::kind_t::atom;
    n->pred = eval;
    return seq_expr(std::move(n), deps);
  }

  operator prop_expr() const;

  seq_expr then(unsigned int n, const seq_expr& next) const {
    return seq_expr(*this).then(n, next);
  }
  seq_expr then(unsigned int min_d, unsigned int max_d, const seq_expr& next) const {
    return seq_expr(*this).then(min_d, max_d, next);
  }
  seq_expr then_unbounded(unsigned int min_d, const seq_expr& next) const {
    return seq_expr(*this).then_unbounded(min_d, next);
  }
  seq_expr repeat(unsigned int count) const {
    return seq_expr(*this).repeat(count);
  }
  seq_expr repeat(unsigned int min_r, unsigned int max_r) const {
    return seq_expr(*this).repeat(min_r, max_r);
  }
  seq_expr repeat_unbounded(unsigned int min_r) const {
    return seq_expr(*this).repeat_unbounded(min_r);
  }
  prop_expr implies(const seq_expr& con) const;
  prop_expr next_implies(const seq_expr& con) const;
};

class prop_expr {
public:
  bool is_implication = false;
  bool next_cycle = false;
  seq_expr antecedent;
  seq_expr consequent;
  bool has_disable = false;
  pred_expr disable_cond;
  std::vector<std::shared_ptr<detail::sampler_slot_base>> deps;

  prop_expr() = default;
  prop_expr(const seq_expr& seq)
      : is_implication(false), next_cycle(false), antecedent(seq), consequent(seq), deps(seq.deps) {}

  prop_expr disable_iff(const pred_expr& cond) const {
    prop_expr copy = *this;
    copy.has_disable = true;
    copy.disable_cond = cond;
    detail::merge_deps(copy.deps, cond.deps);
    return copy;
  }

  prop_expr disable_iff(const expr<bool>& cond) const {
    pred_expr p;
    p.eval = [cond]() -> bool { return cond(); };
    p.deps = cond.deps;
    return disable_iff(p);
  }
};

inline pred_expr::operator prop_expr() const {
  return prop_expr(seq_expr(*this));
}

template <typename T>
inline expr<T>::operator pred_expr() const {
  pred_expr p;
  auto fn = eval;
  p.eval = [fn]() -> bool { return fn ? detail::expr_to_bool(fn()) : false; };
  p.deps = deps;
  return p;
}

template <typename T>
inline expr<T>::operator seq_expr() const {
  return seq_expr(static_cast<pred_expr>(*this));
}

template <typename T>
inline expr<T>::operator prop_expr() const {
  return prop_expr(static_cast<seq_expr>(*this));
}

template <typename T>
inline seq_expr expr<T>::then(unsigned int n, const seq_expr& next) const {
  return seq_expr(*this).then(n, next);
}

template <typename T>
inline seq_expr expr<T>::then(unsigned int min_d, unsigned int max_d, const seq_expr& next) const {
  return seq_expr(*this).then(min_d, max_d, next);
}

template <typename T>
inline seq_expr expr<T>::then_unbounded(unsigned int min_d, const seq_expr& next) const {
  return seq_expr(*this).then_unbounded(min_d, next);
}

template <typename T>
inline seq_expr expr<T>::repeat(unsigned int count) const {
  return seq_expr(*this).repeat(count);
}

template <typename T>
inline seq_expr expr<T>::repeat(unsigned int min_r, unsigned int max_r) const {
  return seq_expr(*this).repeat(min_r, max_r);
}

template <typename T>
inline seq_expr expr<T>::repeat_unbounded(unsigned int min_r) const {
  return seq_expr(*this).repeat_unbounded(min_r);
}

template <typename T>
inline prop_expr expr<T>::implies(const seq_expr& con) const {
  return seq_expr(*this).implies(con);
}

template <typename T>
inline prop_expr expr<T>::next_implies(const seq_expr& con) const {
  return seq_expr(*this).next_implies(con);
}

template <typename T>
inline prop_expr expr<T>::disable_iff(const pred_expr& cond) const {
  return prop_expr(*this).disable_iff(cond);
}

inline prop_expr seq_expr::implies(const seq_expr& con) const {
  prop_expr p;
  p.is_implication = true;
  p.next_cycle = false;
  p.antecedent = *this;
  p.consequent = con;
  p.deps = deps;
  detail::merge_deps(p.deps, con.deps);
  return p;
}

inline prop_expr seq_expr::next_implies(const seq_expr& con) const {
  prop_expr p;
  p.is_implication = true;
  p.next_cycle = true;
  p.antecedent = *this;
  p.consequent = con;
  p.deps = deps;
  detail::merge_deps(p.deps, con.deps);
  return p;
}

inline prop_expr pred_expr::implies(const seq_expr& con) const {
  return seq_expr(*this).implies(con);
}

inline prop_expr pred_expr::next_implies(const seq_expr& con) const {
  return seq_expr(*this).next_implies(con);
}

inline prop_expr implies(const seq_expr& ant, const seq_expr& con) {
  return ant.implies(con);
}

inline prop_expr next_implies(const seq_expr& ant, const seq_expr& con) {
  return ant.next_implies(con);
}

inline prop_expr disable_iff(const pred_expr& cond, const prop_expr& prop) {
  return prop.disable_iff(cond);
}

inline prop_expr disable_iff(const expr<bool>& cond, const prop_expr& prop) {
  return prop.disable_iff(cond);
}

inline seq_expr true_seq() {
  auto n = std::make_shared<seq_expr::node>();
  n->kind = seq_expr::kind_t::atom;
  n->pred = []() { return true; };
  return seq_expr(std::move(n), {});
}

inline seq_expr delay(unsigned int n, const seq_expr& s) {
  return true_seq().then(n, s);
}

inline seq_expr delay(unsigned int min_d, unsigned int max_d, const seq_expr& s) {
  return true_seq().then(min_d, max_d, s);
}

inline seq_expr delay_unbounded(unsigned int min_d, const seq_expr& s) {
  return true_seq().then_unbounded(min_d, s);
}

inline seq_expr seq_delay(const seq_expr& a, unsigned int n, const seq_expr& b) {
  return a.then(n, b);
}

inline seq_expr seq_delay(const seq_expr& a, unsigned int min_d, unsigned int max_d, const seq_expr& b) {
  return a.then(min_d, max_d, b);
}

inline seq_expr seq_delay_unbounded(const seq_expr& a, unsigned int min_d, const seq_expr& b) {
  return a.then_unbounded(min_d, b);
}

inline seq_expr seq_or(const seq_expr& a, const seq_expr& b) {
  return a || b;
}

inline seq_expr repeat(const seq_expr& s, unsigned int n) { return s.repeat(n); }
inline seq_expr repeat(const seq_expr& s, unsigned int min_r, unsigned int max_r) { return s.repeat(min_r, max_r); }
inline seq_expr repeat_unbounded(const seq_expr& s, unsigned int min_r) { return s.repeat_unbounded(min_r); }

template <typename T>
inline expr<T> val(const sc_core::sc_signal_in_if<T>& sig) {
  auto slot = std::make_shared<detail::typed_sampler_slot<T>>();
  const sc_core::sc_signal_in_if<T>* sig_ptr = &sig;
  slot->guard.bind(dynamic_cast<const sc_core::sc_object*>(&sig));
  slot->reader = [sig_ptr](T& out) -> bool {
    out = sig_ptr->read();
    return true;
  };
  expr<T> e;
  e.direct_slot = slot;
  e.deps.push_back(slot);
  e.eval = [slot]() -> T { return slot->current(); };
  return e;
}

template <typename T>
inline expr<T> val(const sc_core::sc_port_b<sc_core::sc_signal_in_if<T>>& port) {
  auto slot = std::make_shared<detail::typed_sampler_slot<T>>();
  const sc_core::sc_port_b<sc_core::sc_signal_in_if<T>>* port_ptr = &port;
  slot->guard.bind(&port);
  slot->reader = [port_ptr](T& out) -> bool {
    const auto* iface = dynamic_cast<const sc_core::sc_signal_in_if<T>*>(port_ptr->get_interface());
    if (iface == nullptr) return false;
    out = iface->read();
    return true;
  };
  expr<T> e;
  e.direct_slot = slot;
  e.deps.push_back(slot);
  e.eval = [slot]() -> T { return slot->current(); };
  return e;
}

template <typename T>
inline expr<T> val(const sc_core::sc_port_b<sc_core::sc_signal_inout_if<T>>& port) {
  auto slot = std::make_shared<detail::typed_sampler_slot<T>>();
  const sc_core::sc_port_b<sc_core::sc_signal_inout_if<T>>* port_ptr = &port;
  slot->guard.bind(&port);
  slot->reader = [port_ptr](T& out) -> bool {
    const auto* iface = dynamic_cast<const sc_core::sc_signal_in_if<T>*>(port_ptr->get_interface());
    if (iface == nullptr) return false;
    out = iface->read();
    return true;
  };
  expr<T> e;
  e.direct_slot = slot;
  e.deps.push_back(slot);
  e.eval = [slot]() -> T { return slot->current(); };
  return e;
}

template <typename T>
inline expr<T> val(const sc_core::sc_export<sc_core::sc_signal_in_if<T>>& exp) {
  auto slot = std::make_shared<detail::typed_sampler_slot<T>>();
  const sc_core::sc_export<sc_core::sc_signal_in_if<T>>* exp_ptr = &exp;
  slot->guard.bind(&exp);
  slot->reader = [exp_ptr](T& out) -> bool {
    const auto* iface = dynamic_cast<const sc_core::sc_signal_in_if<T>*>(exp_ptr->get_interface());
    if (iface == nullptr) return false;
    out = iface->read();
    return true;
  };
  expr<T> e;
  e.direct_slot = slot;
  e.deps.push_back(slot);
  e.eval = [slot]() -> T { return slot->current(); };
  return e;
}

template <typename T>
inline expr<T> val(const sc_core::sc_export<sc_core::sc_signal_inout_if<T>>& exp) {
  auto slot = std::make_shared<detail::typed_sampler_slot<T>>();
  const sc_core::sc_export<sc_core::sc_signal_inout_if<T>>* exp_ptr = &exp;
  slot->guard.bind(&exp);
  slot->reader = [exp_ptr](T& out) -> bool {
    const auto* iface = dynamic_cast<const sc_core::sc_signal_in_if<T>*>(exp_ptr->get_interface());
    if (iface == nullptr) return false;
    out = iface->read();
    return true;
  };
  expr<T> e;
  e.direct_slot = slot;
  e.deps.push_back(slot);
  e.eval = [slot]() -> T { return slot->current(); };
  return e;
}

template <typename Fn, typename = typename std::enable_if<
    !std::is_base_of<sc_core::sc_interface, typename std::decay<Fn>::type>::value &&
    !std::is_base_of<sc_core::sc_object, typename std::decay<Fn>::type>::value>::type>
inline expr<decltype(std::declval<Fn>()())> val_fn(Fn fn) {
  using T = decltype(fn());
  auto slot = std::make_shared<detail::typed_sampler_slot<T>>();
  slot->reader = [fn](T& out) -> bool {
    out = fn();
    return true;
  };
  expr<T> e;
  e.direct_slot = slot;
  e.deps.push_back(slot);
  e.eval = [slot]() -> T { return slot->current(); };
  return e;
}

template <typename Fn>
inline pred_expr pred(Fn fn) {
  auto slot = std::make_shared<detail::pred_sampler_slot>();
  slot->reader = [fn]() -> bool { return detail::expr_to_bool(fn()); };
  pred_expr p;
  p.deps.push_back(slot);
  p.eval = [slot]() -> bool { return slot->current(); };
  return p;
}

inline pred_expr to_pred(const pred_expr& p) { return p; }
inline pred_expr to_pred(const expr<bool>& e) {
  pred_expr p;
  p.eval = [e]() -> bool { return e(); };
  p.deps = e.deps;
  return p;
}

inline pred_expr operator!(const pred_expr& p) {
  pred_expr r;
  r.deps = p.deps;
  r.eval = [p]() -> bool { return !p.eval(); };
  return r;
}

inline pred_expr operator!(const expr<bool>& e) {
  return !to_pred(e);
}

inline pred_expr operator&&(const pred_expr& a, const pred_expr& b) {
  pred_expr r;
  r.deps = a.deps;
  detail::merge_deps(r.deps, b.deps);
  r.eval = [a, b]() -> bool { return a.eval() && b.eval(); };
  return r;
}

inline pred_expr operator&&(const expr<bool>& a, const expr<bool>& b) {
  return to_pred(a) && to_pred(b);
}
inline pred_expr operator&&(const expr<bool>& a, const pred_expr& b) {
  return to_pred(a) && b;
}
inline pred_expr operator&&(const pred_expr& a, const expr<bool>& b) {
  return a && to_pred(b);
}

inline pred_expr operator||(const pred_expr& a, const pred_expr& b) {
  pred_expr r;
  r.deps = a.deps;
  detail::merge_deps(r.deps, b.deps);
  r.eval = [a, b]() -> bool { return a.eval() || b.eval(); };
  return r;
}
inline pred_expr operator||(const expr<bool>& a, const expr<bool>& b) {
  return to_pred(a) || to_pred(b);
}
inline pred_expr operator||(const expr<bool>& a, const pred_expr& b) {
  return to_pred(a) || b;
}
inline pred_expr operator||(const pred_expr& a, const expr<bool>& b) {
  return a || to_pred(b);
}

// Implicit conversion from expr<bool> -> pred_expr / seq_expr / prop_expr via helper operators
inline seq_expr to_seq(const seq_expr& s) { return s; }
inline seq_expr to_seq(const pred_expr& p) { return seq_expr(p); }
inline seq_expr to_seq(const expr<bool>& e) { return seq_expr(to_pred(e)); }

namespace detail {

template <typename T>
inline std::shared_ptr<typed_sampler_slot<T>> materialize_slot(
    const expr<T>& e,
    std::vector<std::shared_ptr<sampler_slot_base>>& deps_out)
{
  deps_out = e.deps;
  if (e.direct_slot != nullptr) {
    return e.direct_slot;
  }
  auto slot = std::make_shared<typed_sampler_slot<T>>();
  auto fn = e.eval;
  auto dep_slots = e.deps;
  slot->reader = [fn, dep_slots](T& out) -> bool {
    if (!fn) return false;
    for (size_t i = 0; i < dep_slots.size(); ++i) {
      if (dep_slots[i]) dep_slots[i]->set_eval_pending(true);
    }
    out = fn();
    for (size_t i = 0; i < dep_slots.size(); ++i) {
      if (dep_slots[i]) dep_slots[i]->set_eval_pending(false);
    }
    return true;
  };
  deps_out.push_back(slot);
  return slot;
}

} // namespace detail

template <typename T>
inline expr<T> past(const expr<T>& e, size_t n = 1) {
  std::vector<std::shared_ptr<detail::sampler_slot_base>> deps;
  auto slot = detail::materialize_slot(e, deps);
  slot->ensure_history_depth(n + 2);
  expr<T> res;
  res.deps = std::move(deps);
  res.eval = [slot, n]() -> T { return slot->past(n); };
  return res;
}

inline expr<bool> past(const pred_expr& p, size_t n = 1) {
  expr<bool> e;
  e.deps = p.deps;
  e.eval = p.eval;
  return past(e, n);
}

template <typename S>
inline auto past(const S& sig, size_t n = 1) -> decltype(past(val(sig), n)) {
  return past(val(sig), n);
}

template <typename T>
inline pred_expr rose(const expr<T>& e) {
  std::vector<std::shared_ptr<detail::sampler_slot_base>> deps;
  auto slot = detail::materialize_slot(e, deps);
  slot->ensure_history_depth(2);
  pred_expr p;
  p.deps = std::move(deps);
  p.eval = [slot]() -> bool {
    const char cur = detail::lsb_char(slot->current());
    const char prv = slot->has_past(1) ? detail::lsb_char(slot->past(1)) : detail::lsb_char(T{});
    return cur == '1' && prv != '1';
  };
  return p;
}

inline pred_expr rose(const pred_expr& p) {
  expr<bool> e;
  e.deps = p.deps;
  e.eval = p.eval;
  return rose(e);
}

template <typename S>
inline pred_expr rose(const S& sig) { return rose(val(sig)); }

template <typename T>
inline pred_expr fell(const expr<T>& e) {
  std::vector<std::shared_ptr<detail::sampler_slot_base>> deps;
  auto slot = detail::materialize_slot(e, deps);
  slot->ensure_history_depth(2);
  pred_expr p;
  p.deps = std::move(deps);
  p.eval = [slot]() -> bool {
    const char cur = detail::lsb_char(slot->current());
    const char prv = slot->has_past(1) ? detail::lsb_char(slot->past(1)) : detail::lsb_char(T{});
    return cur == '0' && prv != '0';
  };
  return p;
}

inline pred_expr fell(const pred_expr& p) {
  expr<bool> e;
  e.deps = p.deps;
  e.eval = p.eval;
  return fell(e);
}

template <typename S>
inline pred_expr fell(const S& sig) { return fell(val(sig)); }

template <typename T>
inline pred_expr stable(const expr<T>& e) {
  std::vector<std::shared_ptr<detail::sampler_slot_base>> deps;
  auto slot = detail::materialize_slot(e, deps);
  slot->ensure_history_depth(2);
  pred_expr p;
  p.deps = std::move(deps);
  p.eval = [slot]() -> bool {
    return slot->has_past(1) ? (slot->current() == slot->past(1)) : (slot->current() == T{});
  };
  return p;
}

inline pred_expr stable(const pred_expr& p) {
  expr<bool> e;
  e.deps = p.deps;
  e.eval = p.eval;
  return stable(e);
}

template <typename S>
inline pred_expr stable(const S& sig) { return stable(val(sig)); }

template <typename T>
inline pred_expr changed(const expr<T>& e) {
  return !stable(e);
}

inline pred_expr changed(const pred_expr& p) {
  return !stable(p);
}

template <typename S>
inline pred_expr changed(const S& sig) { return changed(val(sig)); }

#define FC4SC_SVA_CMP_OP(OP) \
template <typename T, typename U> \
inline pred_expr operator OP(const expr<T>& a, const expr<U>& b) { \
  pred_expr p; \
  p.deps = a.deps; \
  detail::merge_deps(p.deps, b.deps); \
  p.eval = [a, b]() -> bool { return a() OP b(); }; \
  return p; \
} \
template <typename T, typename U, typename = typename std::enable_if< \
    !std::is_same<typename std::decay<U>::type, expr<T>>::value>::type> \
inline pred_expr operator OP(const expr<T>& a, const U& b) { \
  pred_expr p; \
  p.deps = a.deps; \
  p.eval = [a, b]() -> bool { return a() OP b; }; \
  return p; \
} \
template <typename T, typename U, typename = typename std::enable_if< \
    !std::is_same<typename std::decay<T>::type, expr<U>>::value>::type> \
inline pred_expr operator OP(const T& a, const expr<U>& b) { \
  pred_expr p; \
  p.deps = b.deps; \
  p.eval = [a, b]() -> bool { return a OP b(); }; \
  return p; \
}

FC4SC_SVA_CMP_OP(==)
FC4SC_SVA_CMP_OP(!=)
FC4SC_SVA_CMP_OP(<)
FC4SC_SVA_CMP_OP(<=)
FC4SC_SVA_CMP_OP(>)
FC4SC_SVA_CMP_OP(>=)
#undef FC4SC_SVA_CMP_OP

#define FC4SC_SVA_BIN_OP(OP) \
template <typename T> \
inline expr<T> operator OP(const expr<T>& a, const expr<T>& b) { \
  expr<T> r; \
  r.deps = a.deps; \
  detail::merge_deps(r.deps, b.deps); \
  r.eval = [a, b]() -> T { return a() OP b(); }; \
  return r; \
}

FC4SC_SVA_BIN_OP(+)
FC4SC_SVA_BIN_OP(-)
FC4SC_SVA_BIN_OP(&)
FC4SC_SVA_BIN_OP(|)
FC4SC_SVA_BIN_OP(^)
#undef FC4SC_SVA_BIN_OP

inline seq_expr operator||(const expr<bool>& a, const seq_expr& b) { return to_seq(a) || b; }
inline seq_expr operator||(const seq_expr& a, const expr<bool>& b) { return a || to_seq(b); }
inline seq_expr operator||(const pred_expr& a, const seq_expr& b) { return to_seq(a) || b; }
inline seq_expr operator||(const seq_expr& a, const pred_expr& b) { return a || to_seq(b); }
inline pending_seq_delay operator>>(const expr<bool>& lhs, const delay_tag& tag) {
  return to_seq(lhs) >> tag;
}
inline pending_seq_delay operator>>(const pred_expr& lhs, const delay_tag& tag) {
  return to_seq(lhs) >> tag;
}

namespace detail {

struct nfa_edge {
  int to = 0;
  bool is_eps = false;
  bool is_tick = false;
  std::function<bool()> pred;
};

struct nfa_graph {
  int entry = 0;
  int exit = 0;
  std::vector<std::vector<nfa_edge>> adj;

  int new_node() {
    adj.emplace_back();
    return static_cast<int>(adj.size()) - 1;
  }

  void add_eps(int u, int v) {
    nfa_edge e;
    e.to = v;
    e.is_eps = true;
    adj[static_cast<size_t>(u)].push_back(e);
  }

  void add_tick(int u, int v) {
    nfa_edge e;
    e.to = v;
    e.is_tick = true;
    adj[static_cast<size_t>(u)].push_back(e);
  }

  void add_pred(int u, int v, std::function<bool()> p) {
    nfa_edge e;
    e.to = v;
    e.pred = std::move(p);
    adj[static_cast<size_t>(u)].push_back(std::move(e));
  }
};

struct nfa_fragment {
  int entry = 0;
  int exit = 0;
};

inline std::shared_ptr<seq_expr::node> make_atom_node(std::function<bool()> pred) {
  auto n = std::make_shared<seq_expr::node>();
  n->kind = seq_expr::kind_t::atom;
  n->pred = std::move(pred);
  return n;
}

inline std::shared_ptr<seq_expr::node> make_false_node() {
  return make_atom_node([]() { return false; });
}

inline std::shared_ptr<seq_expr::node> make_true_node() {
  return make_atom_node([]() { return true; });
}

inline std::shared_ptr<seq_expr::node> make_delay_node(
    unsigned int min_d,
    unsigned int max_d,
    bool unbounded,
    const std::shared_ptr<seq_expr::node>& rhs) {
  auto n = std::make_shared<seq_expr::node>();
  n->kind = seq_expr::kind_t::concat;
  n->lhs = make_true_node();
  n->rhs = rhs;
  n->min_cnt = min_d;
  n->max_cnt = max_d;
  n->unbounded = unbounded;
  return n;
}

inline bool has_top_zero_repeat(const std::shared_ptr<seq_expr::node>& n) {
  if (!n) return false;
  if (n->kind == seq_expr::kind_t::repeat) return n->min_cnt == 0;
  if (n->kind == seq_expr::kind_t::disj) {
    return has_top_zero_repeat(n->lhs) || has_top_zero_repeat(n->rhs);
  }
  return false;
}

inline std::shared_ptr<seq_expr::node> normalize_zero_repeat(const std::shared_ptr<seq_expr::node>& n) {
  if (!n) return n;
  if (n->kind == seq_expr::kind_t::concat) {
    auto l = normalize_zero_repeat(n->lhs);
    auto r = normalize_zero_repeat(n->rhs);

    if (r && r->kind == seq_expr::kind_t::disj && has_top_zero_repeat(r)) {
      auto c1 = std::make_shared<seq_expr::node>(*n);
      c1->lhs = l;
      c1->rhs = r->lhs;
      auto c2 = std::make_shared<seq_expr::node>(*n);
      c2->lhs = l;
      c2->rhs = r->rhs;
      auto dj = std::make_shared<seq_expr::node>();
      dj->kind = seq_expr::kind_t::disj;
      dj->lhs = normalize_zero_repeat(c1);
      dj->rhs = normalize_zero_repeat(c2);
      return dj;
    }

    if (l && l->kind == seq_expr::kind_t::disj && has_top_zero_repeat(l)) {
      auto c1 = std::make_shared<seq_expr::node>(*n);
      c1->lhs = l->lhs;
      c1->rhs = r;
      auto c2 = std::make_shared<seq_expr::node>(*n);
      c2->lhs = l->rhs;
      c2->rhs = r;
      auto dj = std::make_shared<seq_expr::node>();
      dj->kind = seq_expr::kind_t::disj;
      dj->lhs = normalize_zero_repeat(c1);
      dj->rhs = normalize_zero_repeat(c2);
      return dj;
    }

    if (r && r->kind == seq_expr::kind_t::repeat && r->min_cnt == 0) {
      std::shared_ptr<seq_expr::node> empty_branch;
      if (!n->unbounded && n->max_cnt == 0) {
        empty_branch = make_false_node();
      } else {
        const unsigned int eff_min = (n->min_cnt == 0) ? 1U : n->min_cnt;
        const unsigned int new_min = eff_min - 1U;
        if (n->unbounded) {
          auto cat = std::make_shared<seq_expr::node>();
          cat->kind = seq_expr::kind_t::concat;
          cat->lhs = l;
          cat->rhs = make_true_node();
          cat->min_cnt = new_min;
          cat->max_cnt = new_min;
          cat->unbounded = true;
          empty_branch = normalize_zero_repeat(cat);
        } else {
          const unsigned int new_max = n->max_cnt - 1U;
          if (new_min == 0 && new_max == 0) {
            empty_branch = l;
          } else {
            auto cat = std::make_shared<seq_expr::node>();
            cat->kind = seq_expr::kind_t::concat;
            cat->lhs = l;
            cat->rhs = make_true_node();
            cat->min_cnt = new_min;
            cat->max_cnt = new_max;
            cat->unbounded = false;
            empty_branch = normalize_zero_repeat(cat);
          }
        }
      }
      if (r->max_cnt == 0 && !r->unbounded) return empty_branch;
      auto r_pos = std::make_shared<seq_expr::node>(*r);
      r_pos->min_cnt = 1;
      auto cat_pos = std::make_shared<seq_expr::node>(*n);
      cat_pos->lhs = l;
      cat_pos->rhs = r_pos;
      cat_pos = normalize_zero_repeat(cat_pos);
      auto dj = std::make_shared<seq_expr::node>();
      dj->kind = seq_expr::kind_t::disj;
      dj->lhs = empty_branch;
      dj->rhs = cat_pos;
      return dj;
    }

    if (l && l->kind == seq_expr::kind_t::repeat && l->min_cnt == 0) {
      std::shared_ptr<seq_expr::node> empty_branch;
      if (!n->unbounded && n->max_cnt == 0) {
        empty_branch = make_false_node();
      } else {
        const unsigned int eff_min = (n->min_cnt == 0) ? 1U : n->min_cnt;
        const unsigned int new_min = eff_min - 1U;
        if (n->unbounded) {
          empty_branch = make_delay_node(new_min, new_min, true, r);
        } else {
          const unsigned int new_max = n->max_cnt - 1U;
          if (new_min == 0 && new_max == 0) {
            empty_branch = r;
          } else {
            empty_branch = make_delay_node(new_min, new_max, false, r);
          }
        }
      }
      if (l->max_cnt == 0 && !l->unbounded) return empty_branch;
      auto l_pos = std::make_shared<seq_expr::node>(*l);
      l_pos->min_cnt = 1;
      auto cat_pos = std::make_shared<seq_expr::node>(*n);
      cat_pos->lhs = l_pos;
      cat_pos->rhs = r;
      cat_pos = normalize_zero_repeat(cat_pos);
      auto dj = std::make_shared<seq_expr::node>();
      dj->kind = seq_expr::kind_t::disj;
      dj->lhs = empty_branch;
      dj->rhs = cat_pos;
      return dj;
    }

    auto res = std::make_shared<seq_expr::node>(*n);
    res->lhs = l;
    res->rhs = r;
    return res;
  }
  if (n->kind == seq_expr::kind_t::disj) {
    auto res = std::make_shared<seq_expr::node>(*n);
    res->lhs = normalize_zero_repeat(n->lhs);
    res->rhs = normalize_zero_repeat(n->rhs);
    return res;
  }
  if (n->kind == seq_expr::kind_t::repeat) {
    auto res = std::make_shared<seq_expr::node>(*n);
    res->lhs = normalize_zero_repeat(n->lhs);
    return res;
  }
  return n;
}

inline nfa_fragment compile_node(nfa_graph& g, const std::shared_ptr<seq_expr::node>& n) {
  if (!n) {
    int e = g.new_node();
    int x = g.new_node();
    g.add_pred(e, x, []() { return true; });
    return {e, x};
  }
  switch (n->kind) {
    case seq_expr::kind_t::atom: {
      int e = g.new_node();
      int x = g.new_node();
      g.add_pred(e, x, n->pred ? n->pred : std::function<bool()>([]() { return true; }));
      return {e, x};
    }
    case seq_expr::kind_t::disj: {
      int e = g.new_node();
      int x = g.new_node();
      auto f1 = compile_node(g, n->lhs);
      auto f2 = compile_node(g, n->rhs);
      g.add_eps(e, f1.entry);
      g.add_eps(e, f2.entry);
      g.add_eps(f1.exit, x);
      g.add_eps(f2.exit, x);
      return {e, x};
    }
    case seq_expr::kind_t::concat: {
      auto f1 = compile_node(g, n->lhs);
      auto f2 = compile_node(g, n->rhs);
      int cur = f1.exit;
      for (unsigned int i = 0; i < n->min_cnt; ++i) {
        int nxt = g.new_node();
        g.add_tick(cur, nxt);
        cur = nxt;
      }
      if (n->unbounded) {
        g.add_eps(cur, f2.entry);
        g.add_tick(cur, cur);
      } else {
        for (unsigned int k = n->min_cnt; k <= n->max_cnt; ++k) {
          g.add_eps(cur, f2.entry);
          if (k < n->max_cnt) {
            int nxt = g.new_node();
            g.add_tick(cur, nxt);
            cur = nxt;
          }
        }
      }
      return {f1.entry, f2.exit};
    }
    case seq_expr::kind_t::repeat: {
      if (n->min_cnt == 0 && !n->unbounded && n->max_cnt == 0) {
        int e = g.new_node();
        int x = g.new_node();
        g.add_eps(e, x);
        return {e, x};
      }
      const unsigned int mand = (n->min_cnt == 0) ? 1U : n->min_cnt;
      std::vector<nfa_fragment> copies;
      copies.push_back(compile_node(g, n->lhs));
      for (unsigned int i = 1; i < mand; ++i) {
        auto f = compile_node(g, n->lhs);
        g.add_tick(copies.back().exit, f.entry);
        copies.push_back(f);
      }
      int final_exit = g.new_node();
      if (n->min_cnt == 0) {
        g.add_eps(copies.front().entry, final_exit);
      }
      if (n->unbounded) {
        g.add_eps(copies.back().exit, final_exit);
        g.add_tick(copies.back().exit, copies.back().entry);
      } else {
        g.add_eps(copies.back().exit, final_exit);
        for (unsigned int k = mand; k < n->max_cnt; ++k) {
          auto f = compile_node(g, n->lhs);
          g.add_tick(copies.back().exit, f.entry);
          g.add_eps(f.exit, final_exit);
          copies.push_back(f);
        }
      }
      return {copies.front().entry, final_exit};
    }
  }
  int e = g.new_node();
  int x = g.new_node();
  g.add_eps(e, x);
  return {e, x};
}

inline nfa_graph compile_sequence(const seq_expr& seq) {
  nfa_graph g;
  auto norm = normalize_zero_repeat(seq.root);
  auto frag = compile_node(g, norm);
  g.entry = frag.entry;
  g.exit = frag.exit;
  return g;
}

struct nfa_runner {
  const nfa_graph* graph = nullptr;
  std::vector<uint8_t> active;

  void start(const nfa_graph* g) {
    graph = g;
    active.assign(g->adj.size(), 0);
    active[static_cast<size_t>(g->entry)] = 1;
  }

  struct step_result {
    bool matched = false;
    bool alive = false;
  };

  step_result step_tick() {
    const size_t n = graph->adj.size();
    std::vector<uint8_t> reached(n, 0);
    std::vector<int> q;
    q.reserve(n);
    for (size_t i = 0; i < n; ++i) {
      if (active[i]) {
        reached[i] = 1;
        q.push_back(static_cast<int>(i));
      }
    }
    for (size_t head = 0; head < q.size(); ++head) {
      const int u = q[head];
      const auto& edges = graph->adj[static_cast<size_t>(u)];
      for (size_t ei = 0; ei < edges.size(); ++ei) {
        const nfa_edge& e = edges[ei];
        if (e.is_tick) continue;
        if (!e.is_eps && e.pred && !e.pred()) continue;
        const size_t to = static_cast<size_t>(e.to);
        if (!reached[to]) {
          reached[to] = 1;
          q.push_back(e.to);
        }
      }
    }

    step_result res;
    res.matched = (reached[static_cast<size_t>(graph->exit)] != 0);

    std::vector<uint8_t> next_active(n, 0);
    for (size_t u = 0; u < n; ++u) {
      if (!reached[u]) continue;
      const auto& edges = graph->adj[u];
      for (size_t ei = 0; ei < edges.size(); ++ei) {
        if (edges[ei].is_tick) {
          next_active[static_cast<size_t>(edges[ei].to)] = 1;
          res.alive = true;
        }
      }
    }
    active.swap(next_active);
    return res;
  }
};

struct assertion_attempt {
  nfa_runner ant;
  bool ant_alive = true;
  bool ant_matched_any = false;
  bool pending_next_con = false;
  std::vector<nfa_runner> cons;
};

struct sva_monitor_entry {
  bool is_cover = false;
  bool stopped = false;
  sc_core::sc_simcontext* simc = nullptr;
  std::string epoch_marker;
  std::weak_ptr<bool> scope_valid;
  std::shared_ptr<assertion_item_data> data;
  clock_spec clk;
  sc_obj_guard clk_guard;
  prop_expr prop;
  nfa_graph ant_nfa;
  nfa_graph con_nfa;
  std::vector<std::shared_ptr<sampler_slot_base>> slots;
  std::vector<assertion_attempt> attempts;
  sc_core::sc_severity severity = sc_core::SC_ERROR;
  bool no_throw = false;
  bool sim_initialized = false;
  std::string file;
  uint32_t line = 1;

  bool is_alive() const {
    if (stopped || scope_valid.expired()) return false;
    if (!epoch_marker.empty()) {
      sc_core::sc_simcontext* cur = sc_core::sc_curr_simcontext;
      if (cur == nullptr || cur != simc ||
          !cur->hierarchical_name_exists(nullptr, epoch_marker.c_str())) {
        return false;
      }
    }
    if (!clk_guard.is_alive()) return false;
    for (size_t i = 0; i < slots.size(); ++i) {
      if (slots[i] && !slots[i]->is_alive()) return false;
    }
    return true;
  }

  void detach_attrs_if_alive() {
    clk_guard.detach_if_alive();
    for (size_t i = 0; i < slots.size(); ++i) {
      if (slots[i]) slots[i]->detach_if_alive();
    }
  }

  void init_simulation() {
    clk.init();
    for (size_t i = 0; i < slots.size(); ++i) {
      if (slots[i]) slots[i]->init_pending();
    }
    sim_initialized = true;
  }

  bool eval_disable_pending() {
    if (!prop.has_disable || !prop.disable_cond.eval) return false;
    for (size_t i = 0; i < slots.size(); ++i) {
      if (slots[i]) slots[i]->set_eval_pending(true);
    }
    const bool disabled = prop.disable_cond.eval();
    for (size_t i = 0; i < slots.size(); ++i) {
      if (slots[i]) slots[i]->set_eval_pending(false);
    }
    return disabled;
  }

  void retire_all_disabled() {
    if (!attempts.empty()) {
      data->disabled_count += attempts.size();
      attempts.clear();
      data->active_count = 0;
    }
  }

  void sync_active_counts() {
    data->active_count = attempts.size();
    if (data->active_count > data->peak_active_count) {
      data->peak_active_count = data->active_count;
    }
  }

  void report_failure(const std::string& detail_msg) {
    std::ostringstream oss;
    oss << "Assertion '" << data->name << "' failed (" << detail_msg << ")";
    if (!data->expr_str.empty()) {
      oss << " [expr: " << data->expr_str << "]";
    }
    const std::string msg = oss.str();
    try {
      sc_core::sc_report_handler::report(
          severity, "FC4SC/SVA", msg.c_str(), file.c_str(), static_cast<int>(line));
    } catch (const sc_core::sc_report&) {
      if (!no_throw) {
        throw;
      }
    }
  }

  std::exception_ptr evaluate_step() {
    std::exception_ptr exc;
    const bool disabled_now = eval_disable_pending();
    const bool clk_edge = clk.check_edge_and_update();

    if (disabled_now) {
      retire_all_disabled();
      if (clk_edge) {
        for (size_t i = 0; i < slots.size(); ++i) {
          if (slots[i]) slots[i]->shift_on_clock();
        }
        data->attempt_count++;
        data->disabled_count++;
      }
      refresh_all();
      return exc;
    }

    if (clk_edge) {
      for (size_t i = 0; i < slots.size(); ++i) {
        if (slots[i]) slots[i]->shift_on_clock();
      }

      // Start new attempt on this clock tick
      assertion_attempt fresh;
      fresh.ant.start(&ant_nfa);
      attempts.push_back(std::move(fresh));
      data->attempt_count++;
      sync_active_counts();

      uint64_t fail_this_tick = 0;
      std::vector<assertion_attempt> next_attempts;
      next_attempts.reserve(attempts.size());

      for (size_t ai = 0; ai < attempts.size(); ++ai) {
        assertion_attempt& att = attempts[ai];
        bool retired = false;

        if (is_cover) {
          auto ar = att.ant.step_tick();
          att.ant_alive = ar.alive;
          if (ar.matched) {
            data->cover_count++;
            retired = true;
          } else if (!att.ant_alive) {
            retired = true;
          }
          if (!retired) next_attempts.push_back(std::move(att));
          continue;
        }

        // Assertion evaluation
        if (att.pending_next_con) {
          nfa_runner c;
          c.start(&con_nfa);
          att.cons.push_back(std::move(c));
          att.pending_next_con = false;
        }

        // Step existing in-flight consequents
        bool failed = false;
        std::vector<nfa_runner> next_cons;
        for (size_t ci = 0; ci < att.cons.size(); ++ci) {
          auto cr = att.cons[ci].step_tick();
          if (cr.matched) {
            continue;
          }
          if (!cr.alive) {
            failed = true;
            break;
          }
          next_cons.push_back(std::move(att.cons[ci]));
        }
        att.cons.swap(next_cons);

        if (failed) {
          data->fail_count++;
          fail_this_tick++;
          continue;
        }

        if (att.ant_alive) {
          auto ar = att.ant.step_tick();
          att.ant_alive = ar.alive;
          if (ar.matched) {
            att.ant_matched_any = true;
            if (!prop.is_implication) {
              data->pass_count++;
              retired = true;
            } else if (!prop.next_cycle) {
              nfa_runner c;
              c.start(&con_nfa);
              auto cr = c.step_tick();
              if (cr.matched) {
                // satisfied immediately
              } else if (!cr.alive) {
                data->fail_count++;
                fail_this_tick++;
                retired = true;
              } else {
                att.cons.push_back(std::move(c));
              }
            } else {
              att.pending_next_con = true;
            }
          } else if (!prop.is_implication && !att.ant_alive) {
            data->fail_count++;
            fail_this_tick++;
            retired = true;
          }
        }

        if (!retired && prop.is_implication &&
            !att.ant_alive && att.cons.empty() && !att.pending_next_con) {
          if (att.ant_matched_any) {
            data->pass_count++;
          } else {
            data->vacuous_count++;
          }
          retired = true;
        }

        if (!retired) {
          next_attempts.push_back(std::move(att));
        }
      }

      attempts.swap(next_attempts);
      data->active_count = attempts.size();

      if (fail_this_tick > 0) {
        try {
          report_failure("at clock edge");
        } catch (...) {
          exc = std::current_exception();
        }
      }
    }

    refresh_all();
    if (!clk_edge && eval_disable_pending()) {
      retire_all_disabled();
    }
    return exc;
  }

  void refresh_all() {
    clk.refresh_level();
    for (size_t i = 0; i < slots.size(); ++i) {
      if (slots[i]) slots[i]->refresh_pending();
    }
  }
};

class sva_post_init_channel final : public sc_core::sc_prim_channel {
public:
  explicit sva_post_init_channel(std::function<void()> on_update)
      : sc_core::sc_prim_channel(sc_core::sc_gen_unique_name("_fc4sc_sva_post_init")),
        on_update_(std::move(on_update)) {}

  void arm() { request_update(); }

  void update() override {
    if (on_update_) on_update_();
  }

private:
  std::function<void()> on_update_;
};

} // namespace detail
} // namespace sva

class sc_sva_controller final : public sc_core::sc_stage_callback_if {
public:
  static constexpr unsigned int kStageMask =
      sc_core::SC_POST_START_OF_SIMULATION |
      sc_core::SC_POST_UPDATE |
      sc_core::SC_PRE_TIMESTEP |
      sc_core::SC_PRE_STOP |
      sc_core::SC_PRE_PAUSE |
      sc_core::SC_POST_END_OF_SIMULATION;

  static sc_sva_controller& instance() {
    static sc_sva_controller inst;
    return inst;
  }

  void add_monitor(const std::shared_ptr<sva::detail::sva_monitor_entry>& entry) {
    prune_stale();
    ensure_registered();
    if (entry) {
      entry->simc = registered_simc_;
      entry->epoch_marker = epoch_marker_;
    }
    monitors_.push_back(entry);
    const sc_core::sc_status st = sc_core::sc_get_status();
    if (st == sc_core::SC_START_OF_SIMULATION) {
      arm_post_init_delta0();
    } else if (st == sc_core::SC_RUNNING || st == sc_core::SC_PAUSED ||
               st == sc_core::SC_SUSPENDED) {
      entry->init_simulation();
    }
  }

  void flush_timestep() {
    const sc_core::sc_status st = sc_core::sc_get_status();
    if (st == sc_core::SC_ELABORATION ||
        st == sc_core::SC_BEFORE_END_OF_ELABORATION ||
        st == sc_core::SC_END_OF_ELABORATION ||
        st == sc_core::SC_START_OF_SIMULATION) {
      return;
    }
    prune_stale();
    if (monitors_.empty()) return;
    // Snapshot list in case a callback modifies monitors_
    auto batch = monitors_;
    std::exception_ptr first_exc;
    for (size_t i = 0; i < batch.size(); ++i) {
      if (!batch[i] || !batch[i]->is_alive()) continue;
      if (!batch[i]->sim_initialized) {
        batch[i]->init_simulation();
      }
      std::exception_ptr exc = batch[i]->evaluate_step();
      if (exc && !first_exc) {
        first_exc = exc;
      }
    }
    if (first_exc) {
      std::rethrow_exception(first_exc);
    }
  }

  void stage_callback(const sc_core::sc_stage& stage) override {
    if (stage == sc_core::SC_POST_END_OF_SIMULATION) {
      destroy_hook_if_same_simc();
      delta0_armed_ = false;
      delta0_done_ = false;
      prune_stale();
      return;
    }

    prune_stale();
    if (monitors_.empty()) {
      if (stage == sc_core::SC_POST_UPDATE ||
          stage == sc_core::SC_PRE_STOP ||
          stage == sc_core::SC_PRE_PAUSE) {
        destroy_hook_if_same_simc();
      }
      return;
    }

    if (stage == sc_core::SC_POST_START_OF_SIMULATION) {
      arm_post_init_delta0();
      return;
    }

    if (stage == sc_core::SC_POST_UPDATE) {
      destroy_hook_if_same_simc();
      return;
    }

    if (stage == sc_core::SC_PRE_TIMESTEP ||
        stage == sc_core::SC_PRE_STOP ||
        stage == sc_core::SC_PRE_PAUSE) {
      destroy_hook_if_same_simc();
      flush_timestep();
    }
  }

  ~sc_sva_controller() override {
    for (size_t i = 0; i < monitors_.size(); ++i) {
      if (monitors_[i]) monitors_[i]->detach_attrs_if_alive();
    }
    monitors_.clear();
    destroy_hook_if_same_simc();
    if (registered_ &&
        sc_core::sc_curr_simcontext != nullptr &&
        sc_core::sc_curr_simcontext == registered_simc_ &&
        sc_core::sc_curr_simcontext->get_stage_cb_registry() == registered_registry_) {
      sc_core::sc_unregister_stage_callback(*this, kStageMask);
    }
    registered_ = false;
    registered_simc_ = nullptr;
    registered_registry_ = nullptr;
  }

private:
  sc_sva_controller() = default;
  sc_sva_controller(const sc_sva_controller&) = delete;
  sc_sva_controller& operator=(const sc_sva_controller&) = delete;

  void destroy_hook_if_same_simc() {
    if (hook_ == nullptr) return;
    sc_core::sc_simcontext* cur = sc_core::sc_curr_simcontext;
    if (cur != nullptr && cur == hook_simc_ &&
        cur->get_prim_channel_registry() == hook_prim_reg_ &&
        cur->get_object_manager() == hook_obj_mgr_ &&
        sc_core::sc_find_object(hook_name_.c_str()) == hook_) {
      delete hook_;
    }
    hook_ = nullptr;
    hook_simc_ = nullptr;
    hook_prim_reg_ = nullptr;
    hook_obj_mgr_ = nullptr;
    hook_name_.clear();
  }

  void ensure_registered() {
    sc_core::sc_simcontext* simc = sc_core::sc_curr_simcontext;
    if (simc == nullptr) simc = sc_core::sc_get_curr_simcontext();
    auto* reg = (simc != nullptr) ? simc->get_stage_cb_registry() : nullptr;
    if (simc != nullptr && reg != nullptr) {
      const bool new_sim_cycle =
          !registered_ ||
          registered_simc_ != simc ||
          registered_registry_ != reg ||
          epoch_marker_.empty() ||
          !simc->hierarchical_name_exists(nullptr, epoch_marker_.c_str());
      if (new_sim_cycle) {
        delta0_armed_ = false;
        delta0_done_ = false;
        hook_ = nullptr;
        hook_simc_ = nullptr;
        hook_prim_reg_ = nullptr;
        hook_obj_mgr_ = nullptr;
        hook_name_.clear();
        ++sim_epoch_;
        epoch_marker_ = "__fc4sc_sva_epoch_" + std::to_string(sim_epoch_);
        simc->register_hierarchical_name(nullptr, epoch_marker_);
      }
      sc_core::sc_register_stage_callback(*this, kStageMask);
      registered_ = true;
      registered_simc_ = simc;
      registered_registry_ = reg;

      if (hook_ != nullptr &&
          (simc != hook_simc_ ||
           simc->get_prim_channel_registry() != hook_prim_reg_ ||
           simc->get_object_manager() != hook_obj_mgr_ ||
           sc_core::sc_find_object(hook_name_.c_str()) != hook_)) {
        hook_ = nullptr;
        hook_simc_ = nullptr;
        hook_prim_reg_ = nullptr;
        hook_obj_mgr_ = nullptr;
        hook_name_.clear();
      }
      if (hook_ == nullptr && !delta0_done_ &&
          !simc->elaboration_done() && !sc_core::sc_is_running()) {
        hook_simc_ = simc;
        hook_prim_reg_ = simc->get_prim_channel_registry();
        hook_obj_mgr_ = simc->get_object_manager();
        hook_ = new sva::detail::sva_post_init_channel([this]() { this->on_prim_update(); });
        hook_name_ = hook_->name();
      }
    }
  }

  void spawn_post_init_method() {
    sc_core::sc_hierarchy_scope root_scope = sc_core::sc_hierarchy_scope::get_root();
    (void)root_scope;
    sc_core::sc_spawn_options opt;
    opt.spawn_method();
    sc_core::sc_spawn(
        [this]() { this->on_post_init_delta0(); },
        sc_core::sc_gen_unique_name("_fc4sc_sva_delta0"),
        &opt);
  }

  void arm_post_init_delta0() {
    if (delta0_armed_ || delta0_done_) return;
    delta0_armed_ = true;
    sc_core::sc_simcontext* simc = sc_core::sc_curr_simcontext;
    if (simc == nullptr) simc = sc_core::sc_get_curr_simcontext();
    if (hook_ != nullptr && simc != nullptr && simc == hook_simc_ &&
        simc->get_prim_channel_registry() == hook_prim_reg_ &&
        simc->get_object_manager() == hook_obj_mgr_ &&
        sc_core::sc_find_object(hook_name_.c_str()) == hook_) {
      hook_->arm();
    } else {
      spawn_post_init_method();
    }
  }

  void on_prim_update() {
    spawn_post_init_method();
  }

  void on_post_init_delta0() {
    delta0_done_ = true;
    prune_stale();
    for (size_t i = 0; i < monitors_.size(); ++i) {
      if (monitors_[i] && !monitors_[i]->sim_initialized) {
        monitors_[i]->init_simulation();
      }
    }
  }

  void prune_stale() {
    for (size_t i = 0; i < monitors_.size();) {
      if (!monitors_[i] || !monitors_[i]->is_alive()) {
        if (monitors_[i]) monitors_[i]->detach_attrs_if_alive();
        monitors_.erase(monitors_.begin() + static_cast<std::ptrdiff_t>(i));
      } else {
        ++i;
      }
    }
  }

  bool registered_ = false;
  bool delta0_armed_ = false;
  bool delta0_done_ = false;
  uint64_t sim_epoch_ = 0;
  std::string epoch_marker_;
  sc_core::sc_simcontext* registered_simc_ = nullptr;
  sc_core::sc_stage_callback_registry* registered_registry_ = nullptr;
  sva::detail::sva_post_init_channel* hook_ = nullptr;
  sc_core::sc_simcontext* hook_simc_ = nullptr;
  sc_core::sc_prim_channel_registry* hook_prim_reg_ = nullptr;
  sc_core::sc_object_manager* hook_obj_mgr_ = nullptr;
  std::string hook_name_;
  std::vector<std::shared_ptr<sva::detail::sva_monitor_entry>> monitors_;
};

class sva_handle {
public:
  sva_handle() = default;
  explicit sva_handle(std::shared_ptr<sva::detail::sva_monitor_entry> entry)
      : entry_(std::move(entry)) {}

  std::shared_ptr<assertion_item_data> data() const {
    return entry_ ? entry_->data : std::shared_ptr<assertion_item_data>();
  }

  uint64_t cover_count() const { return entry_ ? entry_->data->cover_count : 0; }
  uint64_t pass_count() const { return entry_ ? entry_->data->pass_count : 0; }
  uint64_t fail_count() const { return entry_ ? entry_->data->fail_count : 0; }
  uint64_t vacuous_count() const { return entry_ ? entry_->data->vacuous_count : 0; }
  uint64_t disabled_count() const { return entry_ ? entry_->data->disabled_count : 0; }
  uint64_t attempt_count() const { return entry_ ? entry_->data->attempt_count : 0; }
  uint64_t active_count() const { return entry_ ? entry_->data->active_count : 0; }
  uint64_t peak_active_count() const { return entry_ ? entry_->data->peak_active_count : 0; }

  void stop() {
    if (entry_) entry_->stopped = true;
  }

private:
  std::shared_ptr<sva::detail::sva_monitor_entry> entry_;
};

namespace sva {

inline sva_handle register_property(
    const std::string& name,
    const clock_spec& clk,
    const prop_expr& prop,
    const std::string& expr_str,
    bool is_cover,
    const sva_options& opts = sva_options(),
    const char* file = __FILE__,
    uint32_t line = __LINE__)
{
  const std::string f = opts.file.empty() ? std::string(file ? file : __FILE__) : opts.file;
  const uint32_t l = (opts.line > 0) ? opts.line : (line > 0 ? line : 1U);
  fc4sc::global* cntxt = (opts.cntxt != nullptr) ? opts.cntxt : fc4sc::global::getter();
  const unsigned int fid = fc4sc::global::get_file_id(f, cntxt);
  scp_base_data_model* scp = detail::resolve_sva_scope(clk.sc_obj, opts, f, l);

  auto item = std::make_shared<assertion_item_data>();
  item->name = name;
  item->assertion_kind = is_cover ? "cover" : "assert";
  item->name_component = opts.name_component;
  item->type_component = opts.type_component;
  item->expr_str = expr_str;
  item->file_id = (fid > 0) ? fid : 1U;
  item->line = (l > 0) ? l : 1U;
  item->weight = opts.weight;
  if (is_cover) {
    item->emit_cover_bin = true;
    item->emit_disabled_bin = true;
    item->emit_attempt_bin = true;
    item->emit_active_bin = true;
    item->emit_peak_active_bin = true;
  } else {
    item->emit_pass_bin = true;
    item->emit_fail_bin = true;
    item->emit_vacuous_bin = true;
    item->emit_disabled_bin = true;
    item->emit_attempt_bin = true;
    item->emit_active_bin = true;
    item->emit_peak_active_bin = true;
  }
  scp->assertion_cov.assertions.push_back(item);

  auto entry = std::make_shared<detail::sva_monitor_entry>();
  entry->is_cover = is_cover;
  entry->scope_valid = scp->valid;
  entry->data = item;
  entry->clk = clk;
  entry->clk_guard.bind(clk.sc_obj);
  entry->prop = prop;
  entry->slots = prop.deps;
  entry->ant_nfa = detail::compile_sequence(prop.antecedent);
  if (!is_cover && prop.is_implication) {
    entry->con_nfa = detail::compile_sequence(prop.consequent);
  }
  entry->severity = opts.severity;
  entry->no_throw = opts.no_throw;
  entry->file = f;
  entry->line = (l > 0) ? l : 1U;

  sc_sva_controller::instance().add_monitor(entry);
  return sva_handle(entry);
}

inline sva_handle assert_property(
    const std::string& name,
    const clock_spec& clk,
    const prop_expr& prop,
    const sva_options& opts = sva_options(),
    const std::string& expr_str = "",
    const char* file = __FILE__,
    uint32_t line = __LINE__)
{
  return register_property(name, clk, prop, expr_str, false, opts, file, line);
}

inline sva_handle cover_property(
    const std::string& name,
    const clock_spec& clk,
    const seq_expr& seq,
    const sva_options& opts = sva_options(),
    const std::string& expr_str = "",
    const char* file = __FILE__,
    uint32_t line = __LINE__)
{
  return register_property(name, clk, prop_expr(seq), expr_str, true, opts, file, line);
}

} // namespace sva

/*!
 * \brief Explicitly evaluates any pending Preponed SVA stage step (e.g. before sc_stop / after SC_ZERO_TIME).
 */
inline void sc_flush_sva() {
  sc_sva_controller::instance().flush_timestep();
}

} // namespace fc4sc

#define FC4SC_SVA_PAST_1(sig) ::fc4sc::sva::past((sig), 1)
#define FC4SC_SVA_PAST_2(sig, n) ::fc4sc::sva::past((sig), (n))
#define FC4SC_SVA_PAST_DISPATCH(_1, _2, NAME, ...) NAME
#define FC4SC_PAST(...) FC4SC_SVA_PAST_DISPATCH(__VA_ARGS__, FC4SC_SVA_PAST_2, FC4SC_SVA_PAST_1)(__VA_ARGS__)
#define FC4SC_ROSE(sig) ::fc4sc::sva::rose((sig))
#define FC4SC_FELL(sig) ::fc4sc::sva::fell((sig))
#define FC4SC_STABLE(sig) ::fc4sc::sva::stable((sig))
#define FC4SC_CHANGED(sig) ::fc4sc::sva::changed((sig))

#define FC4SC_ASSERT_PROPERTY_3(name, clk_edge, prop) \
  ::fc4sc::sva::assert_property((name), (clk_edge), (prop), ::fc4sc::sva_options(), #prop, __FILE__, __LINE__)
#define FC4SC_ASSERT_PROPERTY_4(name, clk_edge, prop, opts) \
  ::fc4sc::sva::assert_property((name), (clk_edge), (prop), (opts), #prop, __FILE__, __LINE__)
#define FC4SC_ASSERT_PROPERTY_DISPATCH(_1, _2, _3, _4, NAME, ...) NAME
#define FC4SC_ASSERT_PROPERTY(...) \
  FC4SC_ASSERT_PROPERTY_DISPATCH(__VA_ARGS__, FC4SC_ASSERT_PROPERTY_4, FC4SC_ASSERT_PROPERTY_3)(__VA_ARGS__)

#define FC4SC_COVER_PROPERTY_3(name, clk_edge, seq) \
  ::fc4sc::sva::cover_property((name), (clk_edge), (seq), ::fc4sc::sva_options(), #seq, __FILE__, __LINE__)
#define FC4SC_COVER_PROPERTY_4(name, clk_edge, seq, opts) \
  ::fc4sc::sva::cover_property((name), (clk_edge), (seq), (opts), #seq, __FILE__, __LINE__)
#define FC4SC_COVER_PROPERTY_DISPATCH(_1, _2, _3, _4, NAME, ...) NAME
#define FC4SC_COVER_PROPERTY(...) \
  FC4SC_COVER_PROPERTY_DISPATCH(__VA_ARGS__, FC4SC_COVER_PROPERTY_4, FC4SC_COVER_PROPERTY_3)(__VA_ARGS__)

#endif /* FC4SC_SVA_HPP */
