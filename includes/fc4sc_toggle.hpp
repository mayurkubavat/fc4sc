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
 * \file fc4sc_toggle.hpp
 * \brief Optional SystemC signal and port toggle coverage tracker for FC4SC.
 *
 * Tracks 0->1, 1->0, and 4-state (0/1/X/Z) bit-by-bit transitions on SystemC
 * signals, buffers, clocks, resolved signals, ports, and exports, exporting
 * UCIS-compliant <toggleCoverage> via scp_base_data_model::toggle_cov.
 * Requires C++17 / SystemC 3.0.3 and is excluded from includes/fc4sc.hpp.
 */

#ifndef FC4SC_TOGGLE_HPP
#define FC4SC_TOGGLE_HPP

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <systemc>

#include "fc4sc.hpp"

namespace fc4sc {

enum class toggle_sample_mode {
  delta = 0,
  timestep = 1
};

enum class toggle_4state_mode {
  full_4state = 0,
  observed_4state = 1,
  two_state_only = 2
};

struct toggle_options {
  toggle_sample_mode sample_mode = toggle_sample_mode::delta;
  toggle_4state_mode state_mode = toggle_4state_mode::full_4state;
  std::string name_override;
  std::string type_override;
  fc4sc::scp_base* scope = nullptr;
  fc4sc::global* cntxt = nullptr;
  std::string file;
  uint32_t line = 0;
};

namespace detail {

template <typename T, typename Enable = void>
struct toggle_value_traits;

template <>
struct toggle_value_traits<bool> {
  static constexpr unsigned int bit_width = 1;
  static constexpr bool is_4state = false;
  static constexpr bool is_vector = false;
  static std::string default_type_name() { return "bool"; }
  static void extract(const bool& v, std::vector<char>& out) {
    out.assign(1, v ? '1' : '0');
  }
};

template <typename T>
struct toggle_value_traits<
    T, typename std::enable_if<(std::is_integral<T>::value || std::is_enum<T>::value) &&
                               !std::is_same<T, bool>::value>::type> {
  static constexpr unsigned int bit_width = static_cast<unsigned int>(sizeof(T) * 8);
  static constexpr bool is_4state = false;
  static constexpr bool is_vector = (bit_width > 1);
  static std::string default_type_name() { return "bitvector"; }
  static void extract(const T& v, std::vector<char>& out) {
    out.resize(bit_width);
    const uint64_t raw = static_cast<uint64_t>(v);
    for (unsigned int i = 0; i < bit_width; ++i) {
      out[i] = ((raw >> i) & 1ULL) ? '1' : '0';
    }
  }
};

template <int W>
struct toggle_value_traits<sc_dt::sc_int<W>> {
  static constexpr unsigned int bit_width = static_cast<unsigned int>(W);
  static constexpr bool is_4state = false;
  static constexpr bool is_vector = (W > 1);
  static std::string default_type_name() { return "sc_int"; }
  static void extract(const sc_dt::sc_int<W>& v, std::vector<char>& out) {
    out.resize(bit_width);
    for (int i = 0; i < W; ++i) {
      out[static_cast<size_t>(i)] = v[i].to_bool() ? '1' : '0';
    }
  }
};

template <int W>
struct toggle_value_traits<sc_dt::sc_uint<W>> {
  static constexpr unsigned int bit_width = static_cast<unsigned int>(W);
  static constexpr bool is_4state = false;
  static constexpr bool is_vector = (W > 1);
  static std::string default_type_name() { return "sc_uint"; }
  static void extract(const sc_dt::sc_uint<W>& v, std::vector<char>& out) {
    out.resize(bit_width);
    for (int i = 0; i < W; ++i) {
      out[static_cast<size_t>(i)] = v[i].to_bool() ? '1' : '0';
    }
  }
};

template <int W>
struct toggle_value_traits<sc_dt::sc_bigint<W>> {
  static constexpr unsigned int bit_width = static_cast<unsigned int>(W);
  static constexpr bool is_4state = false;
  static constexpr bool is_vector = (W > 1);
  static std::string default_type_name() { return "sc_bigint"; }
  static void extract(const sc_dt::sc_bigint<W>& v, std::vector<char>& out) {
    out.resize(bit_width);
    for (int i = 0; i < W; ++i) {
      out[static_cast<size_t>(i)] = v.test(i) ? '1' : '0';
    }
  }
};

template <int W>
struct toggle_value_traits<sc_dt::sc_biguint<W>> {
  static constexpr unsigned int bit_width = static_cast<unsigned int>(W);
  static constexpr bool is_4state = false;
  static constexpr bool is_vector = (W > 1);
  static std::string default_type_name() { return "sc_biguint"; }
  static void extract(const sc_dt::sc_biguint<W>& v, std::vector<char>& out) {
    out.resize(bit_width);
    for (int i = 0; i < W; ++i) {
      out[static_cast<size_t>(i)] = v.test(i) ? '1' : '0';
    }
  }
};

template <int W>
struct toggle_value_traits<sc_dt::sc_bv<W>> {
  static constexpr unsigned int bit_width = static_cast<unsigned int>(W);
  static constexpr bool is_4state = false;
  static constexpr bool is_vector = (W > 1);
  static std::string default_type_name() { return "sc_bv"; }
  static void extract(const sc_dt::sc_bv<W>& v, std::vector<char>& out) {
    out.resize(bit_width);
    for (int i = 0; i < W; ++i) {
      out[static_cast<size_t>(i)] = v.get_bit(i) ? '1' : '0';
    }
  }
};

template <>
struct toggle_value_traits<sc_dt::sc_logic> {
  static constexpr unsigned int bit_width = 1;
  static constexpr bool is_4state = true;
  static constexpr bool is_vector = false;
  static std::string default_type_name() { return "sc_logic"; }
  static void extract(const sc_dt::sc_logic& v, std::vector<char>& out) {
    char c = v.to_char();
    if (c == 'x') c = 'X';
    if (c == 'z') c = 'Z';
    if (c != '0' && c != '1' && c != 'Z') c = 'X';
    out.assign(1, c);
  }
};

template <int W>
struct toggle_value_traits<sc_dt::sc_lv<W>> {
  static constexpr unsigned int bit_width = static_cast<unsigned int>(W);
  static constexpr bool is_4state = true;
  static constexpr bool is_vector = (W > 1);
  static std::string default_type_name() { return "sc_lv"; }
  static void extract(const sc_dt::sc_lv<W>& v, std::vector<char>& out) {
    out.resize(bit_width);
    static const char kMap[4] = {'0', '1', 'Z', 'X'};
    for (int i = 0; i < W; ++i) {
      const unsigned int b = static_cast<unsigned int>(v.get_bit(i)) & 3U;
      out[static_cast<size_t>(i)] = kMap[b];
    }
  }
};

inline std::vector<std::pair<std::string, std::string>> sysc_scope_path(
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

inline scp_base_data_model* resolve_sysc_scope(
    const sc_core::sc_object* obj,
    const toggle_options& opts,
    const std::string& fallback_file,
    uint32_t fallback_line)
{
  if (opts.scope != nullptr) {
    return opts.scope->get_scp_data();
  }
  fc4sc::global* cntxt = (opts.cntxt != nullptr) ? opts.cntxt : fc4sc::global::getter();
  const std::string f = opts.file.empty() ? fallback_file : opts.file;
  const uint32_t l = (opts.line > 0) ? opts.line : (fallback_line > 0 ? fallback_line : 1U);
  return fc4sc::global::resolve_scope_path(sysc_scope_path(obj), f, l, cntxt);
}

inline uint64_t next_toggle_token() {
  static uint64_t seq = 1;
  return seq++;
}

struct toggle_monitor_entry {
  sc_core::sc_simcontext* simc = nullptr;
  sc_core::sc_object_manager* obj_mgr = nullptr;
  sc_core::sc_object* sc_obj = nullptr;
  std::string sc_name;
  std::string attr_name;
  std::string epoch_marker;
  std::unique_ptr<sc_core::sc_attribute<uint64_t>> token_attr;

  std::weak_ptr<bool> scope_valid;
  std::shared_ptr<toggle_object_data> data;
  toggle_sample_mode sample_mode = toggle_sample_mode::delta;
  toggle_4state_mode state_mode = toggle_4state_mode::full_4state;

  std::function<bool()> has_event;
  std::function<bool(std::vector<char>&)> sample_bits;

  bool initialized = false;
  bool needs_initial_sync = false;
  bool stopped = false;
  std::vector<char> prev_bits;

  bool is_alive() const {
    if (stopped || scope_valid.expired()) return false;
    if (!epoch_marker.empty()) {
      sc_core::sc_simcontext* cur = sc_core::sc_curr_simcontext;
      if (cur == nullptr || cur != simc ||
          !cur->hierarchical_name_exists(nullptr, epoch_marker.c_str())) {
        return false;
      }
    }
    if (sc_obj != nullptr) {
      sc_core::sc_simcontext* cur = sc_core::sc_curr_simcontext;
      if (cur == nullptr || cur != simc) return false;
      if (cur->get_object_manager() != obj_mgr) return false;
      if (sc_core::sc_find_object(sc_name.c_str()) != sc_obj) return false;
      if (token_attr != nullptr &&
          sc_obj->get_attribute(attr_name) != token_attr.get()) {
        return false;
      }
    }
    return true;
  }

  void detach_attr_if_alive() {
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

  void initialize_baseline(bool from_start_of_sim = true) {
    if (initialized) return;
    std::vector<char> bits;
    if (!sample_bits(bits) || bits.empty()) return;
    prev_bits = bits;
    initialized = true;
    needs_initial_sync = from_start_of_sim;
  }

  void force_sync_initial() {
    if (!needs_initial_sync) return;
    needs_initial_sync = false;
    std::vector<char> bits;
    if (sample_bits(bits) && bits.size() == prev_bits.size()) {
      prev_bits.swap(bits);
    }
  }

  void sync_initial_if_needed() {
    if (!needs_initial_sync) return;
    force_sync_initial();
  }

  void record_if_changed(bool require_event) {
    if (!initialized) {
      initialize_baseline(false);
      return;
    }
    sync_initial_if_needed();
    if (require_event && has_event && !has_event()) return;

    std::vector<char> curr_bits;
    if (!sample_bits(curr_bits) || curr_bits.size() != prev_bits.size()) return;

    for (size_t i = 0; i < curr_bits.size(); ++i) {
      const char from = prev_bits[i];
      const char to = curr_bits[i];
      if (from == to) continue;

      if (state_mode == toggle_4state_mode::two_state_only) {
        if ((from != '0' && from != '1') || (to != '0' && to != '1')) {
          continue;
        }
      }

      auto& toggles = data->bits[i].toggles;
      bool found = false;
      for (size_t t = 0; t < toggles.size(); ++t) {
        if (toggles[t].from.size() == 1 && toggles[t].from[0] == from &&
            toggles[t].to.size() == 1 && toggles[t].to[0] == to) {
          toggles[t].count++;
          found = true;
          break;
        }
      }
      if (!found && state_mode == toggle_4state_mode::observed_4state) {
        toggle_transition_data td;
        td.from = std::string(1, from);
        td.to = std::string(1, to);
        td.count = 1;
        toggles.push_back(td);
      }
    }
    prev_bits.swap(curr_bits);
  }
};

class toggle_post_init_channel final : public sc_core::sc_prim_channel {
public:
  explicit toggle_post_init_channel(std::function<void()> on_update)
      : sc_core::sc_prim_channel(sc_core::sc_gen_unique_name("_fc4sc_tog_post_init")),
        on_update_(std::move(on_update)) {}

  void arm() { request_update(); }

  void update() override {
    if (on_update_) on_update_();
  }

private:
  std::function<void()> on_update_;
};

} // namespace detail

class sc_toggle_controller final : public sc_core::sc_stage_callback_if {
public:
  static constexpr unsigned int kStageMask =
      sc_core::SC_POST_START_OF_SIMULATION |
      sc_core::SC_POST_UPDATE |
      sc_core::SC_PRE_TIMESTEP |
      sc_core::SC_PRE_STOP |
      sc_core::SC_PRE_PAUSE |
      sc_core::SC_POST_END_OF_SIMULATION;

  static sc_toggle_controller& instance() {
    static sc_toggle_controller inst;
    return inst;
  }

  void add_monitor(const std::shared_ptr<detail::toggle_monitor_entry>& entry) {
    prune_stale();
    ensure_registered();
    if (entry) {
      entry->simc = registered_simc_;
      entry->epoch_marker = epoch_marker_;
    }
    monitors_.push_back(entry);
    const sc_core::sc_status st = sc_core::sc_get_status();
    if (st == sc_core::SC_START_OF_SIMULATION) {
      entry->initialize_baseline(true);
      arm_post_init_delta0();
    } else if (st == sc_core::SC_RUNNING || st == sc_core::SC_PAUSED ||
               st == sc_core::SC_SUSPENDED) {
      entry->initialize_baseline(false);
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
    for (size_t i = 0; i < monitors_.size(); ++i) {
      if (monitors_[i] && monitors_[i]->sample_mode == toggle_sample_mode::timestep) {
        monitors_[i]->record_if_changed(false);
      }
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
      for (size_t i = 0; i < monitors_.size(); ++i) {
        if (monitors_[i]) monitors_[i]->initialize_baseline(true);
      }
      arm_post_init_delta0();
      return;
    }

    if (stage == sc_core::SC_POST_UPDATE) {
      destroy_hook_if_same_simc();
      for (size_t i = 0; i < monitors_.size(); ++i) {
        if (!monitors_[i]) continue;
        if (monitors_[i]->sample_mode == toggle_sample_mode::delta) {
          monitors_[i]->record_if_changed(true);
        } else {
          monitors_[i]->sync_initial_if_needed();
        }
      }
      return;
    }

    if (stage == sc_core::SC_PRE_TIMESTEP ||
        stage == sc_core::SC_PRE_STOP ||
        stage == sc_core::SC_PRE_PAUSE) {
      destroy_hook_if_same_simc();
      for (size_t i = 0; i < monitors_.size(); ++i) {
        if (!monitors_[i]) continue;
        if (monitors_[i]->sample_mode == toggle_sample_mode::timestep) {
          monitors_[i]->record_if_changed(false);
        } else {
          monitors_[i]->sync_initial_if_needed();
        }
      }
    }
  }

  ~sc_toggle_controller() override {
    for (size_t i = 0; i < monitors_.size(); ++i) {
      if (monitors_[i]) monitors_[i]->detach_attr_if_alive();
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
  sc_toggle_controller() = default;
  sc_toggle_controller(const sc_toggle_controller&) = delete;
  sc_toggle_controller& operator=(const sc_toggle_controller&) = delete;

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
        epoch_marker_ = "__fc4sc_tog_epoch_" + std::to_string(sim_epoch_);
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
        hook_ = new detail::toggle_post_init_channel([this]() { this->on_prim_update(); });
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
        sc_core::sc_gen_unique_name("_fc4sc_tog_delta0"),
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
      if (monitors_[i]) {
        monitors_[i]->force_sync_initial();
      }
    }
  }

  void prune_stale() {
    for (size_t i = 0; i < monitors_.size();) {
      if (!monitors_[i] || !monitors_[i]->is_alive()) {
        if (monitors_[i]) monitors_[i]->detach_attr_if_alive();
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
  detail::toggle_post_init_channel* hook_ = nullptr;
  sc_core::sc_simcontext* hook_simc_ = nullptr;
  sc_core::sc_prim_channel_registry* hook_prim_reg_ = nullptr;
  sc_core::sc_object_manager* hook_obj_mgr_ = nullptr;
  std::string hook_name_;
  std::vector<std::shared_ptr<detail::toggle_monitor_entry>> monitors_;
};

class toggle_handle {
public:
  toggle_handle() = default;
  explicit toggle_handle(std::shared_ptr<detail::toggle_monitor_entry> entry)
      : entry_(std::move(entry)) {}

  std::shared_ptr<toggle_object_data> data() const {
    return entry_ ? entry_->data : std::shared_ptr<toggle_object_data>();
  }

  uint64_t bit_count(const std::string& from, const std::string& to, unsigned int bit_idx = 0) const {
    if (!entry_ || !entry_->data || bit_idx >= entry_->data->bits.size()) return 0;
    const auto& toggles = entry_->data->bits[bit_idx].toggles;
    for (size_t i = 0; i < toggles.size(); ++i) {
      if (toggles[i].from == from && toggles[i].to == to) {
        return toggles[i].count;
      }
    }
    return 0;
  }

  void stop() {
    if (entry_) entry_->stopped = true;
  }

private:
  std::shared_ptr<detail::toggle_monitor_entry> entry_;
};

namespace detail {

template <typename T>
std::shared_ptr<toggle_object_data> build_toggle_object_data(
    const std::string& leaf_name,
    const std::string& full_name,
    const std::string& type_name,
    const std::string& port_dir,
    unsigned int file_id,
    uint32_t line,
    toggle_4state_mode state_mode)
{
  using traits = toggle_value_traits<T>;
  auto obj = std::make_shared<toggle_object_data>();
  obj->name = leaf_name;
  obj->full_name = full_name;
  obj->type_name = type_name;
  obj->port_direction = port_dir;
  obj->file_id = (file_id > 0) ? file_id : 1U;
  obj->line = (line > 0) ? line : 1U;
  obj->bit_width = traits::bit_width;
  obj->is_vector = traits::is_vector;
  obj->is_4state = traits::is_4state;

  const bool use_full_4state = traits::is_4state && (state_mode == toggle_4state_mode::full_4state);
  static const char kStates4[4] = {'0', '1', 'Z', 'X'};

  obj->bits.resize(traits::bit_width);
  for (unsigned int i = 0; i < traits::bit_width; ++i) {
    toggle_bit_data& bit = obj->bits[i];
    if (traits::is_vector) {
      bit.name = leaf_name + "[" + std::to_string(i) + "]";
      bit.index = i;
      bit.has_index = true;
    } else {
      bit.name = leaf_name;
      bit.index = 0;
      bit.has_index = false;
    }
    if (use_full_4state) {
      for (int a = 0; a < 4; ++a) {
        for (int b = 0; b < 4; ++b) {
          if (a == b) continue;
          toggle_transition_data td;
          td.from = std::string(1, kStates4[a]);
          td.to = std::string(1, kStates4[b]);
          td.count = 0;
          bit.toggles.push_back(td);
        }
      }
    } else {
      toggle_transition_data t01;
      t01.from = "0"; t01.to = "1"; t01.count = 0;
      toggle_transition_data t10;
      t10.from = "1"; t10.to = "0"; t10.count = 0;
      bit.toggles.push_back(t01);
      bit.toggles.push_back(t10);
    }
  }
  return obj;
}

template <typename T>
toggle_handle register_toggle_impl(
    const sc_core::sc_object* sc_obj,
    const std::string& port_dir,
    const std::string& default_type_name,
    std::function<bool()> has_event,
    std::function<bool(std::vector<char>&)> sample_bits,
    const toggle_options& opts,
    const char* file,
    uint32_t line)
{
  const std::string f = opts.file.empty() ? std::string(file ? file : __FILE__) : opts.file;
  const uint32_t l = (opts.line > 0) ? opts.line : (line > 0 ? line : 1U);
  fc4sc::global* cntxt = (opts.cntxt != nullptr) ? opts.cntxt : fc4sc::global::getter();
  const unsigned int fid = fc4sc::global::get_file_id(f, cntxt);
  scp_base_data_model* scp = resolve_sysc_scope(sc_obj, opts, f, l);

  const std::string full_name = (sc_obj != nullptr && sc_obj->name() != nullptr)
      ? std::string(sc_obj->name())
      : std::string("signal");
  const std::string leaf_name = !opts.name_override.empty()
      ? opts.name_override
      : ((sc_obj != nullptr && sc_obj->basename() != nullptr)
             ? std::string(sc_obj->basename())
             : full_name);
  const std::string type_name = !opts.type_override.empty()
      ? opts.type_override
      : ((sc_obj != nullptr && sc_obj->kind() != nullptr)
             ? std::string(sc_obj->kind())
             : default_type_name);

  auto obj_data = build_toggle_object_data<T>(
      leaf_name, full_name, type_name, port_dir, fid, l, opts.state_mode);
  scp->toggle_cov.objects.push_back(obj_data);
  if (toggle_value_traits<T>::is_4state && opts.state_mode != toggle_4state_mode::two_state_only) {
    scp->toggle_cov.metric_mode = "4state";
  }

  auto entry = std::make_shared<toggle_monitor_entry>();
  sc_core::sc_simcontext* simc = sc_core::sc_curr_simcontext;
  if (simc == nullptr) simc = sc_core::sc_get_curr_simcontext();
  entry->simc = simc;
  entry->obj_mgr = (simc != nullptr) ? simc->get_object_manager() : nullptr;
  entry->sc_obj = const_cast<sc_core::sc_object*>(sc_obj);
  entry->sc_name = full_name;
  if (entry->sc_obj != nullptr) {
    const uint64_t tok = next_toggle_token();
    entry->attr_name = "__fc4sc_toggle_tok_" + std::to_string(tok);
    entry->token_attr.reset(new sc_core::sc_attribute<uint64_t>(entry->attr_name));
    entry->token_attr->value = tok;
    entry->sc_obj->add_attribute(*entry->token_attr);
  }
  entry->scope_valid = scp->valid;
  entry->data = obj_data;
  entry->sample_mode = opts.sample_mode;
  entry->state_mode = opts.state_mode;
  entry->has_event = std::move(has_event);
  entry->sample_bits = std::move(sample_bits);

  sc_toggle_controller::instance().add_monitor(entry);
  return toggle_handle(entry);
}

} // namespace detail

template <typename T>
toggle_handle track_toggle(
    const sc_core::sc_signal_in_if<T>& sig,
    const toggle_options& opts = toggle_options(),
    const char* file = __FILE__,
    uint32_t line = __LINE__)
{
  const sc_core::sc_signal_in_if<T>* sig_ptr = &sig;
  const sc_core::sc_object* obj = dynamic_cast<const sc_core::sc_object*>(&sig);
  return detail::register_toggle_impl<T>(
      obj,
      "",
      detail::toggle_value_traits<T>::default_type_name(),
      [sig_ptr]() -> bool { return sig_ptr->event(); },
      [sig_ptr](std::vector<char>& out) -> bool {
        detail::toggle_value_traits<T>::extract(sig_ptr->read(), out);
        return true;
      },
      opts,
      file,
      line);
}

template <typename T>
toggle_handle track_toggle(
    const sc_core::sc_port_b<sc_core::sc_signal_in_if<T>>& port,
    const toggle_options& opts = toggle_options(),
    const char* file = __FILE__,
    uint32_t line = __LINE__)
{
  const sc_core::sc_port_b<sc_core::sc_signal_in_if<T>>* port_ptr = &port;
  return detail::register_toggle_impl<T>(
      &port,
      "in",
      "sc_in",
      [port_ptr]() -> bool {
        const auto* iface = dynamic_cast<const sc_core::sc_signal_in_if<T>*>(port_ptr->get_interface());
        return (iface != nullptr) ? iface->event() : false;
      },
      [port_ptr](std::vector<char>& out) -> bool {
        const auto* iface = dynamic_cast<const sc_core::sc_signal_in_if<T>*>(port_ptr->get_interface());
        if (iface == nullptr) return false;
        detail::toggle_value_traits<T>::extract(iface->read(), out);
        return true;
      },
      opts,
      file,
      line);
}

template <typename T>
toggle_handle track_toggle(
    const sc_core::sc_port_b<sc_core::sc_signal_inout_if<T>>& port,
    const toggle_options& opts = toggle_options(),
    const char* file = __FILE__,
    uint32_t line = __LINE__)
{
  const sc_core::sc_port_b<sc_core::sc_signal_inout_if<T>>* port_ptr = &port;
  std::string dir = "inout";
  if (port.kind() != nullptr) {
    const std::string kind(port.kind());
    if (kind == "sc_out" || kind == "sc_out_resolved" || kind == "sc_out_rv") {
      dir = "out";
    }
  }
  return detail::register_toggle_impl<T>(
      &port,
      dir,
      port.kind() ? std::string(port.kind()) : std::string("sc_inout"),
      [port_ptr]() -> bool {
        const auto* iface = dynamic_cast<const sc_core::sc_signal_in_if<T>*>(port_ptr->get_interface());
        return (iface != nullptr) ? iface->event() : false;
      },
      [port_ptr](std::vector<char>& out) -> bool {
        const auto* iface = dynamic_cast<const sc_core::sc_signal_in_if<T>*>(port_ptr->get_interface());
        if (iface == nullptr) return false;
        detail::toggle_value_traits<T>::extract(iface->read(), out);
        return true;
      },
      opts,
      file,
      line);
}

template <typename T>
toggle_handle track_toggle(
    const sc_core::sc_export<sc_core::sc_signal_in_if<T>>& exp,
    const toggle_options& opts = toggle_options(),
    const char* file = __FILE__,
    uint32_t line = __LINE__)
{
  const sc_core::sc_export<sc_core::sc_signal_in_if<T>>* exp_ptr = &exp;
  return detail::register_toggle_impl<T>(
      &exp,
      "in",
      "sc_export",
      [exp_ptr]() -> bool {
        const auto* iface = dynamic_cast<const sc_core::sc_signal_in_if<T>*>(exp_ptr->get_interface());
        return (iface != nullptr) ? iface->event() : false;
      },
      [exp_ptr](std::vector<char>& out) -> bool {
        const auto* iface = dynamic_cast<const sc_core::sc_signal_in_if<T>*>(exp_ptr->get_interface());
        if (iface == nullptr) return false;
        detail::toggle_value_traits<T>::extract(iface->read(), out);
        return true;
      },
      opts,
      file,
      line);
}

template <typename T>
toggle_handle track_toggle(
    const sc_core::sc_export<sc_core::sc_signal_inout_if<T>>& exp,
    const toggle_options& opts = toggle_options(),
    const char* file = __FILE__,
    uint32_t line = __LINE__)
{
  const sc_core::sc_export<sc_core::sc_signal_inout_if<T>>* exp_ptr = &exp;
  return detail::register_toggle_impl<T>(
      &exp,
      "inout",
      "sc_export",
      [exp_ptr]() -> bool {
        const auto* iface = dynamic_cast<const sc_core::sc_signal_in_if<T>*>(exp_ptr->get_interface());
        return (iface != nullptr) ? iface->event() : false;
      },
      [exp_ptr](std::vector<char>& out) -> bool {
        const auto* iface = dynamic_cast<const sc_core::sc_signal_in_if<T>*>(exp_ptr->get_interface());
        if (iface == nullptr) return false;
        detail::toggle_value_traits<T>::extract(iface->read(), out);
        return true;
      },
      opts,
      file,
      line);
}

namespace detail {

template <typename T>
bool try_track_typed_object(
    const sc_core::sc_object* obj,
    const toggle_options& opts,
    const char* file,
    uint32_t line,
    std::vector<toggle_handle>& out)
{
  if (const auto* p_in = dynamic_cast<const sc_core::sc_port_b<sc_core::sc_signal_in_if<T>>*>(obj)) {
    out.push_back(track_toggle(*p_in, opts, file, line));
    return true;
  }
  if (const auto* p_io = dynamic_cast<const sc_core::sc_port_b<sc_core::sc_signal_inout_if<T>>*>(obj)) {
    out.push_back(track_toggle(*p_io, opts, file, line));
    return true;
  }
  if (const auto* e_in = dynamic_cast<const sc_core::sc_export<sc_core::sc_signal_in_if<T>>*>(obj)) {
    out.push_back(track_toggle(*e_in, opts, file, line));
    return true;
  }
  if (const auto* e_io = dynamic_cast<const sc_core::sc_export<sc_core::sc_signal_inout_if<T>>*>(obj)) {
    out.push_back(track_toggle(*e_io, opts, file, line));
    return true;
  }
  if (const auto* sig = dynamic_cast<const sc_core::sc_signal_in_if<T>*>(obj)) {
    out.push_back(track_toggle(*sig, opts, file, line));
    return true;
  }
  return false;
}

template <typename T, typename... Seen>
struct is_any_type : std::false_type {};

template <typename T, typename Head, typename... Tail>
struct is_any_type<T, Head, Tail...>
    : std::integral_constant<bool, std::is_same<T, Head>::value || is_any_type<T, Tail...>::value> {};

template <typename T, typename... Seen>
inline bool try_track_if_distinct(
    const sc_core::sc_object* obj,
    const toggle_options& opts,
    const char* file,
    uint32_t line,
    std::vector<toggle_handle>& out)
{
  if constexpr (is_any_type<T, Seen...>::value) {
    (void)obj;
    (void)opts;
    (void)file;
    (void)line;
    (void)out;
    return false;
  } else {
    return try_track_typed_object<T>(obj, opts, file, line, out);
  }
}

template <int W>
inline bool try_track_sc_width(
    const sc_core::sc_object* obj,
    const toggle_options& opts,
    const char* file,
    uint32_t line,
    std::vector<toggle_handle>& out)
{
  return try_track_typed_object<sc_dt::sc_int<W>>(obj, opts, file, line, out) ||
         try_track_typed_object<sc_dt::sc_uint<W>>(obj, opts, file, line, out) ||
         try_track_typed_object<sc_dt::sc_bv<W>>(obj, opts, file, line, out) ||
         try_track_typed_object<sc_dt::sc_lv<W>>(obj, opts, file, line, out);
}

template <typename... ExtraTypes>
inline typename std::enable_if<sizeof...(ExtraTypes) == 0, bool>::type
try_track_extra_types(
    const sc_core::sc_object*,
    const toggle_options&,
    const char*,
    uint32_t,
    std::vector<toggle_handle>&)
{
  return false;
}

template <typename First, typename... Rest>
inline bool try_track_extra_types(
    const sc_core::sc_object* obj,
    const toggle_options& opts,
    const char* file,
    uint32_t line,
    std::vector<toggle_handle>& out)
{
  return try_track_typed_object<First>(obj, opts, file, line, out) ||
         try_track_extra_types<Rest...>(obj, opts, file, line, out);
}

template <typename... ExtraTypes>
inline void track_module_toggles_rec(
    const sc_core::sc_object* obj,
    const toggle_options& opts,
    const char* file,
    uint32_t line,
    std::vector<toggle_handle>& out)
{
  if (obj == nullptr) return;
  bool matched =
      try_track_typed_object<bool>(obj, opts, file, line, out) ||
      try_track_typed_object<sc_dt::sc_logic>(obj, opts, file, line, out) ||
      try_track_typed_object<char>(obj, opts, file, line, out) ||
      try_track_if_distinct<signed char, char>(obj, opts, file, line, out) ||
      try_track_if_distinct<unsigned char, char>(obj, opts, file, line, out) ||
      try_track_typed_object<short>(obj, opts, file, line, out) ||
      try_track_typed_object<unsigned short>(obj, opts, file, line, out) ||
      try_track_typed_object<int>(obj, opts, file, line, out) ||
      try_track_typed_object<unsigned int>(obj, opts, file, line, out) ||
      try_track_typed_object<long>(obj, opts, file, line, out) ||
      try_track_typed_object<unsigned long>(obj, opts, file, line, out) ||
      try_track_typed_object<long long>(obj, opts, file, line, out) ||
      try_track_typed_object<unsigned long long>(obj, opts, file, line, out) ||
      try_track_if_distinct<int8_t, char, signed char>(obj, opts, file, line, out) ||
      try_track_if_distinct<uint8_t, char, unsigned char>(obj, opts, file, line, out) ||
      try_track_if_distinct<int16_t, short>(obj, opts, file, line, out) ||
      try_track_if_distinct<uint16_t, unsigned short>(obj, opts, file, line, out) ||
      try_track_if_distinct<int32_t, int, long>(obj, opts, file, line, out) ||
      try_track_if_distinct<uint32_t, unsigned int, unsigned long>(obj, opts, file, line, out) ||
      try_track_if_distinct<int64_t, long, long long>(obj, opts, file, line, out) ||
      try_track_if_distinct<uint64_t, unsigned long, unsigned long long>(obj, opts, file, line, out) ||
      try_track_sc_width<1>(obj, opts, file, line, out) ||
      try_track_sc_width<2>(obj, opts, file, line, out) ||
      try_track_sc_width<4>(obj, opts, file, line, out) ||
      try_track_sc_width<8>(obj, opts, file, line, out) ||
      try_track_sc_width<16>(obj, opts, file, line, out) ||
      try_track_sc_width<32>(obj, opts, file, line, out) ||
      try_track_sc_width<64>(obj, opts, file, line, out) ||
      try_track_extra_types<ExtraTypes...>(obj, opts, file, line, out);
  (void)matched;

  const auto& children = obj->get_child_objects();
  for (size_t i = 0; i < children.size(); ++i) {
    track_module_toggles_rec<ExtraTypes...>(children[i], opts, file, line, out);
  }
}

} // namespace detail

template <typename... ExtraTypes>
inline std::vector<toggle_handle> track_module_toggles(
    const sc_core::sc_object& root,
    const toggle_options& opts = toggle_options(),
    const char* file = __FILE__,
    uint32_t line = __LINE__)
{
  std::vector<toggle_handle> handles;
  detail::track_module_toggles_rec<ExtraTypes...>(&root, opts, file, line, handles);
  return handles;
}

/*!
 * \brief Explicitly flushes timestep-mode toggle monitors (e.g. after sc_start(SC_ZERO_TIME)).
 */
inline void sc_flush_toggles() {
  sc_toggle_controller::instance().flush_timestep();
}

} // namespace fc4sc

#endif /* FC4SC_TOGGLE_HPP */
