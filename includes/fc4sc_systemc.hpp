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
 * \file fc4sc_systemc.hpp
 * \brief Optional SystemC 3.0.3 strobe sampling integration for FC4SC.
 *
 * Implements IEEE 1800-2023 19.7.1 covergroup type_option.strobe = true via
 * sc_core::sc_stage_callback_if (SC_PRE_TIMESTEP | SC_PRE_STOP | SC_PRE_PAUSE).
 * This header requires C++17 / SystemC 3.0.3 and is intentionally excluded
 * from includes/fc4sc.hpp so standalone C++11 builds remain dependency-free.
 */

#ifndef FC4SC_SYSTEMC_HPP
#define FC4SC_SYSTEMC_HPP

#include <algorithm>
#include <exception>
#include <unordered_set>
#include <vector>

#include <systemc>

#include "fc4sc.hpp"

namespace fc4sc {

class sc_strobe_controller final : public sc_core::sc_stage_callback_if,
                                   public detail::strobe_backend {
public:
  static constexpr unsigned int kStageMask =
      sc_core::SC_PRE_TIMESTEP | sc_core::SC_PRE_STOP | sc_core::SC_PRE_PAUSE;

  static sc_strobe_controller& instance() {
    static sc_strobe_controller inst;
    return inst;
  }

  void schedule(covergroup* cg) override {
    if (cg == nullptr) return;
    const sc_core::sc_status status = sc_core::sc_get_status();
    if (status == sc_core::SC_STOPPED || status == sc_core::SC_END_OF_SIMULATION) {
      cg->sample_immediate();
      return;
    }
    ensure_registered();
    if (pending_set_.insert(cg).second) {
      pending_.push_back(cg);
    }
  }

  void cancel(covergroup* cg) override {
    if (cg == nullptr) return;
    if (pending_set_.erase(cg) > 0) {
      pending_.erase(std::remove(pending_.begin(), pending_.end(), cg), pending_.end());
    }
    if (active_batch_ != nullptr) {
      for (size_t i = 0; i < active_batch_->size(); ++i) {
        if ((*active_batch_)[i] == cg) {
          (*active_batch_)[i] = nullptr;
        }
      }
    }
  }

  void flush_pending() {
    if (pending_.empty()) return;
    std::vector<covergroup*> batch;
    batch.swap(pending_);
    pending_set_.clear();

    std::vector<covergroup*>* prev_batch = active_batch_;
    const bool prev_flushing = flushing_;
    active_batch_ = &batch;
    flushing_ = true;
    struct BatchGuard {
      std::vector<covergroup*>*& slot;
      std::vector<covergroup*>* prev;
      bool& flushing_slot;
      bool prev_flush;
      ~BatchGuard() {
        slot = prev;
        flushing_slot = prev_flush;
      }
    } guard{active_batch_, prev_batch, flushing_, prev_flushing};

    std::exception_ptr first_exc;
    for (size_t i = 0; i < batch.size(); ++i) {
      covergroup* cg = batch[i];
      if (cg != nullptr) {
        try {
          cg->sample_immediate();
        } catch (...) {
          if (!first_exc) {
            first_exc = std::current_exception();
          }
        }
      }
    }
    if (first_exc) {
      std::rethrow_exception(first_exc);
    }
  }

  void stage_callback(const sc_core::sc_stage& stage) override {
    if ((static_cast<unsigned int>(stage) & kStageMask) != 0U) {
      flush_pending();
    }
  }

  ~sc_strobe_controller() override {
    if (detail::active_strobe_backend() == this) {
      detail::active_strobe_backend() = nullptr;
    }
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
  sc_strobe_controller() {
    detail::active_strobe_backend() = this;
  }

  sc_strobe_controller(const sc_strobe_controller&) = delete;
  sc_strobe_controller& operator=(const sc_strobe_controller&) = delete;

  void ensure_registered() {
    sc_core::sc_simcontext* simc = sc_core::sc_curr_simcontext;
    if (simc == nullptr) {
      simc = sc_core::sc_get_curr_simcontext();
    }
    auto* reg = (simc != nullptr) ? simc->get_stage_cb_registry() : nullptr;
    if (simc != nullptr && reg != nullptr) {
      if (!flushing_ &&
          (pending_.empty() || !registered_ || registered_simc_ != simc ||
           registered_registry_ != reg)) {
        sc_core::sc_register_stage_callback(*this, kStageMask);
        registered_ = true;
        registered_simc_ = simc;
        registered_registry_ = reg;
      }
    }
  }

  bool registered_ = false;
  bool flushing_ = false;
  sc_core::sc_simcontext* registered_simc_ = nullptr;
  sc_core::sc_stage_callback_registry* registered_registry_ = nullptr;
  std::vector<covergroup*> pending_;
  std::unordered_set<covergroup*> pending_set_;
  std::vector<covergroup*>* active_batch_ = nullptr;
};

namespace detail {

struct sc_strobe_auto_registrar {
  sc_strobe_auto_registrar() {
    (void)sc_strobe_controller::instance();
  }
};

inline const sc_strobe_auto_registrar sc_strobe_registrar_instance{};

} // namespace detail

/*!
 * \brief Explicitly flushes any queued strobe samples (e.g. after sc_start(SC_ZERO_TIME)
 * or inside sc_module::end_of_simulation()).
 */
inline void sc_flush_strobes() {
  sc_strobe_controller::instance().flush_pending();
}

} // namespace fc4sc

#endif /* FC4SC_SYSTEMC_HPP */
