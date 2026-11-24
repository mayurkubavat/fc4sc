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
 * \file fc4sc_report.hpp
 * \brief Pure C++11 Markdown and self-contained HTML coverage report generator.
 *
 * Builds a deterministic report IR from either live in-memory fc4sc::global
 * models (via covVisitorBase) or parsed/merged UCIS XML ASTs (ucis_db::xml_node),
 * including optional toggle, assertion, and other UCIS metric summaries.
 */

#ifndef FC4SC_REPORT_HPP
#define FC4SC_REPORT_HPP

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "fc4sc_base.hpp"
#include "fc4sc_master.hpp"
#include "fc4sc_coverage_db.hpp"

namespace fc4sc {

struct report_options {
  std::string title;
  bool include_bins;
  bool include_ignore_illegal_bins;
  bool include_empty_scopes;
  size_t max_cross_missing_bins;
  bool include_toggle_coverage;
  bool include_assertion_coverage;
  bool include_other_ucis_metrics;

  report_options()
      : title("FC4SC Coverage Report"),
        include_bins(true),
        include_ignore_illegal_bins(true),
        include_empty_scopes(false),
        max_cross_missing_bins(64),
        include_toggle_coverage(true),
        include_assertion_coverage(true),
        include_other_ucis_metrics(true) {}
};

struct report_bin {
  std::string name;
  std::string kind;   // "bin", "ignore", "illegal", "default", "cross"
  std::string domain; // formatted intervals / sequences / indices
  uint64_t hits;
  unsigned int at_least;
  bool counted;
  bool covered;

  report_bin() : hits(0), at_least(1), counted(true), covered(false) {}
};

struct report_coverpoint {
  std::string name;
  std::string expr_str;
  std::string cond_str;
  uint64_t misses;
  unsigned int at_least;
  unsigned int weight;
  unsigned int goal;
  size_t covered_bins;
  size_t total_bins;
  double coverage_pct;
  std::vector<report_bin> bins;

  report_coverpoint()
      : misses(0), at_least(1), weight(1), goal(100),
        covered_bins(0), total_bins(0), coverage_pct(100.0) {}
};

struct report_cross {
  std::string name;
  std::string expr_str;
  unsigned int at_least;
  unsigned int weight;
  unsigned int goal;
  size_t covered_bins;
  size_t total_bins;
  size_t truncated_missing;
  double coverage_pct;
  std::vector<report_bin> bins;

  report_cross()
      : at_least(1), weight(1), goal(100),
        covered_bins(0), total_bins(0), truncated_missing(0), coverage_pct(100.0) {}
};

struct report_covergroup {
  std::string scope_path;
  std::string type_name;
  std::string name;
  std::string comment;
  std::string iff_str;
  bool enabled;
  unsigned int weight;
  unsigned int goal;
  bool per_instance;
  size_t covered_bins;
  size_t total_bins;
  double coverage_pct;
  std::vector<report_coverpoint> coverpoints;
  std::vector<report_cross> crosses;

  report_covergroup()
      : enabled(true), weight(1), goal(100), per_instance(false),
        covered_bins(0), total_bins(0), coverage_pct(100.0) {}
};

struct report_toggle_entry {
  std::string scope_path;
  std::string name;
  std::string type_name;
  std::string port_direction;
  unsigned int bit_width;
  uint64_t transitions_01;
  uint64_t transitions_10;
  uint64_t other_transitions;
  size_t covered_transitions;
  size_t total_transitions;
  double coverage_pct;

  report_toggle_entry()
      : bit_width(1), transitions_01(0), transitions_10(0),
        other_transitions(0), covered_transitions(0), total_transitions(0),
        coverage_pct(0.0) {}
};

struct report_assertion_entry {
  std::string scope_path;
  std::string name;
  std::string assertion_kind;
  std::string expr_str;
  uint64_t cover_count;
  uint64_t pass_count;
  uint64_t fail_count;
  uint64_t vacuous_count;
  uint64_t disabled_count;
  uint64_t attempt_count;
  uint64_t active_count;
  uint64_t peak_active_count;
  std::string status;

  report_assertion_entry()
      : cover_count(0), pass_count(0), fail_count(0), vacuous_count(0),
        disabled_count(0), attempt_count(0), active_count(0), peak_active_count(0) {}
};

struct report_other_metric {
  std::string metric_name; // "Block", "Condition", "Branch", "FSM"
  size_t covered;
  size_t total;
  double coverage_pct;

  report_other_metric() : covered(0), total(0), coverage_pct(100.0) {}
};

struct report_data {
  std::string title;
  double overall_coverage_pct;
  size_t total_covergroups;
  size_t enabled_covergroups;
  size_t covered_cg_bins;
  size_t total_cg_bins;
  std::vector<report_covergroup> covergroups;
  std::vector<report_toggle_entry> toggle_objects;
  std::vector<report_assertion_entry> assertions;
  std::vector<report_other_metric> other_metrics;
  std::vector<std::string> empty_scopes;

  report_data()
      : overall_coverage_pct(100.0), total_covergroups(0),
        enabled_covergroups(0), covered_cg_bins(0), total_cg_bins(0) {}
};

namespace detail {

inline std::string fmt_pct(double pct) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2) << pct << "%";
  return oss.str();
}

inline std::string escape_html(const std::string& in) {
  std::string out;
  out.reserve(in.size() + 16);
  for (size_t i = 0; i < in.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(in[i]);
    switch (in[i]) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default:
        if (c < 0x20 && c != '\t' && c != '\n' && c != '\r') {
          out += '?';
        } else {
          out += in[i];
        }
        break;
    }
  }
  return out;
}

inline std::string escape_md(const std::string& in) {
  std::string out;
  out.reserve(in.size() + 16);
  for (size_t i = 0; i < in.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(in[i]);
    switch (in[i]) {
      case '\\': out += "\\\\"; break;
      case '`': out += "\\`"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '&': out += "&amp;"; break;
      case '|': out += "\\|"; break;
      case '\r':
      case '\n':
      case '\t':
        out += ' ';
        break;
      default:
        if (c < 0x20) {
          out += '?';
        } else {
          out += in[i];
        }
        break;
    }
  }
  return out;
}

inline std::string trim_ws(const std::string& s) {
  const size_t first = s.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return std::string();
  const size_t last = s.find_last_not_of(" \t\r\n");
  return s.substr(first, last - first + 1);
}

inline std::string format_bin_domain_from_attrs(
    const std::vector<xml_user_attr>& attrs,
    const std::vector<std::pair<std::string, std::string> >& intervals,
    const std::vector<std::vector<std::string> >& sequences)
{
  std::map<std::string, std::string> attr_map;
  for (size_t i = 0; i < attrs.size(); ++i) {
    attr_map[attrs[i].key] = attrs[i].value;
  }

  std::ostringstream oss;
  bool first = true;
  if (!intervals.empty()) {
    for (size_t i = 0; i < intervals.size(); ++i) {
      if (!first) oss << ", ";
      first = false;
      const std::string kf = "fc4sc.range[" + std::to_string(i) + "].from";
      const std::string kt = "fc4sc.range[" + std::to_string(i) + "].to";
      std::string from = (attr_map.count(kf) > 0) ? attr_map[kf] : intervals[i].first;
      std::string to = (attr_map.count(kt) > 0) ? attr_map[kt] : intervals[i].second;
      if (from == to) {
        oss << from;
      } else {
        oss << "[" << from << ":" << to << "]";
      }
    }
  } else if (!sequences.empty()) {
    for (size_t i = 0; i < sequences.size(); ++i) {
      if (!first) oss << ", ";
      first = false;
      const std::string ks = "fc4sc.seq[" + std::to_string(i) + "]";
      if (attr_map.count(ks) > 0) {
        oss << attr_map[ks];
      } else {
        for (size_t j = 0; j < sequences[i].size(); ++j) {
          if (j > 0) oss << " => ";
          oss << sequences[i][j];
        }
      }
    }
  }
  return oss.str();
}

inline std::string bin_kind_label(bin_t bt) {
  switch (bt) {
    case bin_t::default_: return "bin";
    case bin_t::ignore_: return "ignore";
    case bin_t::illegal_: return "illegal";
    case bin_t::default_bin_: return "default";
  }
  return "bin";
}

inline report_toggle_entry build_toggle_entry(
    const std::string& scope_path,
    const toggle_object_data& obj)
{
  report_toggle_entry e;
  e.scope_path = scope_path;
  e.name = obj.name;
  e.type_name = obj.type_name;
  e.port_direction = obj.port_direction;
  e.bit_width = obj.bit_width;
  for (size_t bi = 0; bi < obj.bits.size(); ++bi) {
    const toggle_bit_data& bit = obj.bits[bi];
    for (size_t ti = 0; ti < bit.toggles.size(); ++ti) {
      const toggle_transition_data& td = bit.toggles[ti];
      if (td.excluded) continue;
      e.total_transitions++;
      if (td.count >= td.goal) {
        e.covered_transitions++;
      }
      if (td.from == "0" && td.to == "1") {
        e.transitions_01 += td.count;
      } else if (td.from == "1" && td.to == "0") {
        e.transitions_10 += td.count;
      } else {
        e.other_transitions += td.count;
      }
    }
  }
  e.coverage_pct = (e.total_transitions > 0)
      ? (100.0 * static_cast<double>(e.covered_transitions) / static_cast<double>(e.total_transitions))
      : 100.0;
  return e;
}

inline report_assertion_entry build_assertion_entry(
    const std::string& scope_path,
    const assertion_item_data& asrt)
{
  report_assertion_entry e;
  e.scope_path = scope_path;
  e.name = asrt.name;
  e.assertion_kind = asrt.assertion_kind.empty() ? std::string("assert") : asrt.assertion_kind;
  e.expr_str = asrt.expr_str;
  e.cover_count = asrt.cover_count;
  e.pass_count = asrt.pass_count;
  e.fail_count = asrt.fail_count;
  e.vacuous_count = asrt.vacuous_count;
  e.disabled_count = asrt.disabled_count;
  e.attempt_count = asrt.attempt_count;
  e.active_count = asrt.active_count;
  e.peak_active_count = asrt.peak_active_count;
  if (e.assertion_kind == "cover") {
    e.status = (e.cover_count > 0) ? "COVERED" : "UNCOVERED";
  } else if (e.fail_count > 0) {
    e.status = "FAIL";
  } else if (e.pass_count > 0) {
    e.status = "PASS";
  } else if (e.vacuous_count > 0 || e.disabled_count > 0) {
    e.status = "VACUOUS";
  } else {
    e.status = "NO_ATTEMPTS";
  }
  return e;
}

} // namespace detail

class report_visitor : public covVisitorBase {
public:
  report_visitor() : active_cvp_(nullptr), opts_() {}
  explicit report_visitor(const report_options& opts) : active_cvp_(nullptr), opts_(opts) {}

  report_data build_from_global(
      fc4sc::global* cntxt = fc4sc::global::getter())
  {
    data_ = report_data();
    data_.title = opts_.title;
    if (cntxt == nullptr) cntxt = fc4sc::global::getter();

    std::vector<scp_base_data_model*> top = fc4sc::global::get_top_scopes(cntxt);
    std::sort(top.begin(), top.end(), [](const scp_base_data_model* a, const scp_base_data_model* b) {
      const std::string ta = (a && a->type_data) ? a->type_data->type_name : std::string();
      const std::string tb = (b && b->type_data) ? b->type_data->type_name : std::string();
      if (ta != tb) return ta < tb;
      return (a ? a->name : std::string()) < (b ? b->name : std::string());
    });

    for (size_t i = 0; i < top.size(); ++i) {
      if (top[i] != nullptr) {
        traverse_scope(*top[i], "");
      }
    }

    data_.overall_coverage_pct = fc4sc::global::get_coverage(cntxt);
    return data_;
  }

  void visit(scp_base_data_model& scp) override {
    traverse_scope(scp, "");
  }

  void visit(cvg_base_data_model& cvg) override {
    cvg.sync_options();
    current_cg_ = report_covergroup();
    current_cg_.scope_path = current_scope_path_;
    current_cg_.type_name = cvg.type_data ? cvg.type_data->type_name : std::string();
    current_cg_.name = cvg.name;
    current_cg_.comment = cvg.option.comment;
    current_cg_.iff_str = cvg.iff_cond_str;
    current_cg_.enabled = cvg.enable;
    current_cg_.weight = cvg.option.weight;
    current_cg_.goal = cvg.option.goal;
    current_cg_.per_instance = cvg.option.per_instance;

    if (!cvg.enable) {
      current_cg_.coverage_pct = 100.0;
      return;
    }

    double weighted_sum = 0.0;
    double weight_total = 0.0;
    for (size_t i = 0; i < cvg.cvps.size(); ++i) {
      cvp_base_data_model* item = cvg.cvps[i];
      if (item == nullptr) continue;
      item->accept_visitor(*this);
    }

    for (size_t i = 0; i < current_cg_.coverpoints.size(); ++i) {
      const report_coverpoint& cp = current_cg_.coverpoints[i];
      current_cg_.covered_bins += cp.covered_bins;
      current_cg_.total_bins += cp.total_bins;
      weighted_sum += cp.coverage_pct * static_cast<double>(cp.weight);
      weight_total += static_cast<double>(cp.weight);
    }
    for (size_t i = 0; i < current_cg_.crosses.size(); ++i) {
      const report_cross& cr = current_cg_.crosses[i];
      current_cg_.covered_bins += cr.covered_bins;
      current_cg_.total_bins += cr.total_bins;
      weighted_sum += cr.coverage_pct * static_cast<double>(cr.weight);
      weight_total += static_cast<double>(cr.weight);
    }

    if (cvg.cvps.empty() || weight_total == 0.0 || weighted_sum == 0.0) {
      current_cg_.coverage_pct = (cvg.option.weight == 0) ? 100.0 : 0.0;
    } else {
      const double raw = weighted_sum / weight_total;
      current_cg_.coverage_pct = (raw >= static_cast<double>(cvg.option.goal)) ? 100.0 : raw;
    }
  }

  void visit(coverpoint_base_data_model& cvp) override {
    cvp.sync_options_from_parent();
    report_coverpoint rcp;
    rcp.name = cvp.name;
    rcp.expr_str = cvp.get_sample_expression_str();
    rcp.cond_str = cvp.get_sample_condition_str();
    rcp.misses = cvp.misses;
    rcp.at_least = cvp.option.at_least;
    rcp.weight = cvp.option.weight;
    rcp.goal = cvp.option.goal;

    active_cvp_ = &rcp;
    for (size_t i = 0; i < cvp.bins_data.size(); ++i) {
      if (cvp.bins_data[i] != nullptr) cvp.bins_data[i]->accept_visitor(*this);
    }
    if (opts_.include_ignore_illegal_bins) {
      for (size_t i = 0; i < cvp.illegal_bins_data.size(); ++i) {
        if (cvp.illegal_bins_data[i] != nullptr) cvp.illegal_bins_data[i]->accept_visitor(*this);
      }
      for (size_t i = 0; i < cvp.ignore_bins_data.size(); ++i) {
        if (cvp.ignore_bins_data[i] != nullptr) cvp.ignore_bins_data[i]->accept_visitor(*this);
      }
      for (size_t i = 0; i < cvp.default_bins_data.size(); ++i) {
        if (cvp.default_bins_data[i] != nullptr) cvp.default_bins_data[i]->accept_visitor(*this);
      }
    }
    active_cvp_ = nullptr;

    if (rcp.total_bins == 0) {
      rcp.coverage_pct = (rcp.weight == 0) ? 100.0 : 0.0;
    } else {
      const double raw = 100.0 * static_cast<double>(rcp.covered_bins) / static_cast<double>(rcp.total_bins);
      rcp.coverage_pct = (raw >= static_cast<double>(rcp.goal)) ? 100.0 : raw;
    }
    current_cg_.coverpoints.push_back(rcp);
  }

  void visit(cross_base_data_model& crs) override {
    crs.sync_options_from_parent();
    report_cross rcr;
    rcr.name = crs.name;
    rcr.at_least = crs.option.at_least;
    rcr.weight = crs.option.weight;
    rcr.goal = crs.option.goal;

    std::ostringstream expr_ss;
    for (size_t i = 0; i < crs.cross_cvps.size(); ++i) {
      if (i > 0) expr_ss << " x ";
      expr_ss << (crs.cross_cvps[i] ? crs.cross_cvps[i]->name : std::string("?"));
    }
    rcr.expr_str = expr_ss.str();

    int covered = 0;
    int total = 0;
    crs.get_coverage_bins(covered, total);
    rcr.covered_bins = (covered > 0) ? static_cast<size_t>(covered) : 0;
    rcr.total_bins = (total > 0) ? static_cast<size_t>(total) : 0;
    if (total <= 0) {
      rcr.coverage_pct = (crs.option.weight == 0) ? 100.0 : 0.0;
    } else {
      const double raw = 100.0 * static_cast<double>(covered) / static_cast<double>(total);
      rcr.coverage_pct = (raw >= static_cast<double>(rcr.goal)) ? 100.0 : raw;
    }

    if (opts_.include_bins) {
      const auto& auto_bins = crs.get_cross_bins();
      std::vector<std::pair<std::vector<size_t>, uint64_t> > sorted_auto(auto_bins.begin(), auto_bins.end());
      std::sort(sorted_auto.begin(), sorted_auto.end(),
          [](const std::pair<std::vector<size_t>, uint64_t>& a,
             const std::pair<std::vector<size_t>, uint64_t>& b) {
            return a.first < b.first;
          });
      for (size_t i = 0; i < sorted_auto.size(); ++i) {
        report_bin rb;
        rb.kind = "cross";
        rb.hits = sorted_auto[i].second;
        rb.at_least = crs.option.at_least;
        rb.counted = true;
        rb.covered = (rb.hits >= rb.at_least);
        std::ostringstream nm;
        nm << "<";
        for (size_t d = 0; d < sorted_auto[i].first.size(); ++d) {
          if (d > 0) nm << ",";
          nm << sorted_auto[i].first[d];
        }
        nm << ">";
        rb.name = nm.str();
        rb.domain = rb.name;
        rcr.bins.push_back(rb);
      }

      for (size_t i = 0; i < crs.user_bins.size(); ++i) {
        const auto& ub = crs.user_bins[i];
        const bool is_counted = (ub.bin_type == bin_t::default_ && ub.size > 0);
        if (!is_counted && !opts_.include_ignore_illegal_bins) continue;
        report_bin rb;
        rb.name = ub.name;
        rb.kind = detail::bin_kind_label(ub.bin_type);
        rb.domain = "user";
        rb.hits = ub.hits;
        rb.at_least = crs.option.at_least;
        rb.counted = is_counted;
        rb.covered = is_counted && (rb.hits >= rb.at_least);
        rcr.bins.push_back(rb);
      }

      // Bounded missing auto-bin enumeration
      const size_t cap = opts_.max_cross_missing_bins;
      size_t missing_added = 0;
      if (cap > 0 && !crs.cross_cvps.empty() && !crs.has_default_bin()) {
        std::vector<size_t> dims;
        bool valid_dims = true;
        for (size_t d = 0; d < crs.cross_cvps.size(); ++d) {
          if (!crs.cross_cvps[d] || crs.cross_cvps[d]->size() == 0) {
            valid_dims = false;
            break;
          }
          dims.push_back(crs.cross_cvps[d]->size());
        }
        if (valid_dims) {
          std::vector<size_t> idx(dims.size(), 0);
          bool done = false;
          while (!done) {
            if (auto_bins.find(idx) == auto_bins.end() && crs.is_auto_bin(idx)) {
              if (missing_added < cap) {
                report_bin rb;
                rb.kind = "cross";
                rb.hits = 0;
                rb.at_least = crs.option.at_least;
                rb.counted = true;
                rb.covered = (rb.at_least == 0);
                std::ostringstream nm;
                nm << "<";
                for (size_t d = 0; d < idx.size(); ++d) {
                  if (d > 0) nm << ",";
                  nm << idx[d];
                }
                nm << ">";
                rb.name = nm.str();
                rb.domain = rb.name;
                rcr.bins.push_back(rb);
                missing_added++;
              } else {
                rcr.truncated_missing++;
              }
            }
            size_t pos = dims.size();
            while (pos > 0) {
              pos--;
              idx[pos]++;
              if (idx[pos] < dims[pos]) break;
              idx[pos] = 0;
              if (pos == 0) done = true;
            }
            if (rcr.truncated_missing >= 1024) break;
          }
        }
      }
    }

    current_cg_.crosses.push_back(rcr);
  }

  void visit(bin_base_data_model& bin) override {
    if (active_cvp_ == nullptr) return;
    const bin_t bt = bin.get_bin_type();
    const bool counted = (bt == bin_t::default_);
    uint64_t hits = 0;
    const auto& ihits = bin.get_interval_hits();
    for (size_t i = 0; i < ihits.size(); ++i) {
      hits += ihits[i];
    }
    if (counted) {
      active_cvp_->total_bins++;
      if (hits >= active_cvp_->at_least) {
        active_cvp_->covered_bins++;
      }
    }
    if (opts_.include_bins) {
      report_bin rb;
      rb.name = bin.get_name();
      rb.kind = detail::bin_kind_label(bt);
      rb.hits = hits;
      rb.at_least = active_cvp_->at_least;
      rb.counted = counted;
      rb.covered = counted && (hits >= active_cvp_->at_least);
      if (bt == bin_t::default_bin_) {
        std::ostringstream dom;
        auto dr = bin.get_default_ranges();
        for (size_t i = 0; i < dr.size(); ++i) {
          if (i > 0) dom << ", ";
          const std::string& f = std::get<0>(dr[i]);
          const std::string& t = std::get<1>(dr[i]);
          if (f == t) dom << f;
          else dom << "[" << f << ":" << t << "]";
        }
        rb.domain = dom.str();
      } else {
        rb.domain = detail::format_bin_domain_from_attrs(
            bin.get_user_attrs(),
            bin.get_intervals_decimal(),
            bin.get_sequences_decimal());
      }
      active_cvp_->bins.push_back(rb);
    }
  }

private:
  void traverse_scope(scp_base_data_model& scp, const std::string& parent_path) {
    const std::string scope_path = parent_path + "/" + scp.name +
        "(" + (scp.type_data ? scp.type_data->type_name : std::string()) + ")";
    current_scope_path_ = scope_path;

    // Collect and sort covergroups deterministically by (type_name, inst_name)
    std::vector<cvg_base_data_model*> sorted_cvgs;
    for (std::unordered_map<std::string, std::vector<cvg_base_data_model*> >::iterator it = scp.cvgs.begin();
         it != scp.cvgs.end(); ++it) {
      for (size_t i = 0; i < it->second.size(); ++i) {
        if (it->second[i] != nullptr) sorted_cvgs.push_back(it->second[i]);
      }
    }
    std::sort(sorted_cvgs.begin(), sorted_cvgs.end(),
        [](const cvg_base_data_model* a, const cvg_base_data_model* b) {
          const std::string ta = (a && a->type_data) ? a->type_data->type_name : std::string();
          const std::string tb = (b && b->type_data) ? b->type_data->type_name : std::string();
          if (ta != tb) return ta < tb;
          return (a ? a->name : std::string()) < (b ? b->name : std::string());
        });

    for (size_t i = 0; i < sorted_cvgs.size(); ++i) {
      sorted_cvgs[i]->accept_visitor(*this);
      data_.total_covergroups++;
      if (current_cg_.enabled) {
        data_.enabled_covergroups++;
        data_.covered_cg_bins += current_cg_.covered_bins;
        data_.total_cg_bins += current_cg_.total_bins;
      }
      data_.covergroups.push_back(current_cg_);
    }

    bool scope_has_content = !sorted_cvgs.empty();

    if (opts_.include_toggle_coverage && !scp.toggle_cov.objects.empty()) {
      std::vector<std::shared_ptr<toggle_object_data> > objs = scp.toggle_cov.objects;
      std::sort(objs.begin(), objs.end(),
          [](const std::shared_ptr<toggle_object_data>& a,
             const std::shared_ptr<toggle_object_data>& b) {
            return (a ? a->name : std::string()) < (b ? b->name : std::string());
          });
      for (size_t i = 0; i < objs.size(); ++i) {
        if (objs[i]) {
          data_.toggle_objects.push_back(detail::build_toggle_entry(scope_path, *objs[i]));
          scope_has_content = true;
        }
      }
    }

    if (opts_.include_assertion_coverage && !scp.assertion_cov.assertions.empty()) {
      std::vector<std::shared_ptr<assertion_item_data> > asrts = scp.assertion_cov.assertions;
      std::sort(asrts.begin(), asrts.end(),
          [](const std::shared_ptr<assertion_item_data>& a,
             const std::shared_ptr<assertion_item_data>& b) {
            return (a ? a->name : std::string()) < (b ? b->name : std::string());
          });
      for (size_t i = 0; i < asrts.size(); ++i) {
        if (asrts[i]) {
          data_.assertions.push_back(detail::build_assertion_entry(scope_path, *asrts[i]));
          scope_has_content = true;
        }
      }
    }

    if (opts_.include_empty_scopes && !scope_has_content) {
      data_.empty_scopes.push_back(scope_path);
    }

    std::vector<scp_base_data_model*> children;
    for (std::unordered_map<std::string, scp_base_data_model*>::iterator it = scp.child_scp_insts.begin();
         it != scp.child_scp_insts.end(); ++it) {
      if (it->second != nullptr) children.push_back(it->second);
    }
    std::sort(children.begin(), children.end(),
        [](const scp_base_data_model* a, const scp_base_data_model* b) {
          const std::string ta = (a && a->type_data) ? a->type_data->type_name : std::string();
          const std::string tb = (b && b->type_data) ? b->type_data->type_name : std::string();
          if (ta != tb) return ta < tb;
          return (a ? a->name : std::string()) < (b ? b->name : std::string());
        });
    for (size_t i = 0; i < children.size(); ++i) {
      traverse_scope(*children[i], scope_path);
    }
  }

  report_data data_;
  report_covergroup current_cg_;
  report_coverpoint* active_cvp_;
  std::string current_scope_path_;
  report_options opts_;
};

namespace detail {

inline uint64_t xml_bin_count(const ucis_db::xml_node& bin_node) {
  const ucis_db::xml_node* contents = bin_node.element("contents");
  if (contents == nullptr) return 0;
  uint64_t val = 0;
  ucis_db::parse_uint(contents->get("coverageCount"), val);
  return val;
}

inline unsigned int xml_uint_attr(const ucis_db::xml_node& n, const std::string& attr, unsigned int def) {
  const std::string* p = n.attribute(attr);
  if (p == nullptr) return def;
  uint64_t v = def;
  if (!ucis_db::parse_uint(*p, v)) return def;
  return static_cast<unsigned int>(v);
}

inline void accumulate_bin_node(const ucis_db::xml_node& bin_node, size_t& covered, size_t& total) {
  if (bin_node.get("excluded") == "true") return;
  total++;
  const uint64_t cnt = xml_bin_count(bin_node);
  const unsigned int goal = xml_uint_attr(bin_node, "coverageCountGoal", 1U);
  if (cnt >= goal) covered++;
}

inline void collect_block_metrics(const ucis_db::xml_node& blk_cov, size_t& covered, size_t& total) {
  std::vector<const ucis_db::xml_node*> stack;
  for (size_t i = 0; i < blk_cov.children.size(); ++i) {
    stack.push_back(blk_cov.children[i].get());
  }
  while (!stack.empty()) {
    const ucis_db::xml_node* cur = stack.back();
    stack.pop_back();
    if (cur == nullptr) continue;
    if (cur->name == "blockBin" || cur->name == "bin") {
      accumulate_bin_node(*cur, covered, total);
      continue;
    }
    for (size_t i = 0; i < cur->children.size(); ++i) {
      stack.push_back(cur->children[i].get());
    }
  }
}

inline void collect_named_bin_metrics(
    const ucis_db::xml_node& root_node,
    const char* const* bin_names,
    size_t bin_names_count,
    size_t& covered,
    size_t& total)
{
  std::vector<const ucis_db::xml_node*> stack;
  stack.push_back(&root_node);
  while (!stack.empty()) {
    const ucis_db::xml_node* cur = stack.back();
    stack.pop_back();
    if (cur == nullptr) continue;
    bool is_target = false;
    for (size_t j = 0; j < bin_names_count; ++j) {
      if (cur->name == bin_names[j]) {
        is_target = true;
        break;
      }
    }
    if (is_target) {
      accumulate_bin_node(*cur, covered, total);
      continue;
    }
    for (size_t i = 0; i < cur->children.size(); ++i) {
      stack.push_back(cur->children[i].get());
    }
  }
}

} // namespace detail

namespace detail {

struct xml_merged_cvp {
  bool is_cross = false;
  unsigned int weight = 1;
  unsigned int goal = 100;
  unsigned int at_least = 1;
  std::map<std::pair<std::string, size_t>, uint64_t> cp_bins;
  std::map<std::string, uint64_t> cross_bins;
};

struct xml_merged_cg_type {
  unsigned int type_weight = 1;
  unsigned int type_goal = 100;
  bool merge_instances = false;
  bool has_enabled = false;
  bool has_authoritative_type_opts = false;
  bool has_type_goal = false;
  double inst_weighted_sum = 0.0;
  double inst_weight_total = 0.0;
  std::vector<std::string> cvp_order;
  std::map<std::string, xml_merged_cvp> cvps;
};

struct xml_disabled_cg_info {
  std::string cg_type;
  std::string scp_type;
  std::string inst_name;
  unsigned int type_weight = 1;
  unsigned int inst_weight = 1;
  unsigned int type_goal = 100;
  bool merge_instances = false;
  bool has_structured_meta = false;
};

inline std::string pct_decode_attr_field(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  auto hex_val = [](char ch) -> int {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
    if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
    return -1;
  };
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '%' && i + 2 < s.size()) {
      const int hi = hex_val(s[i + 1]);
      const int lo = hex_val(s[i + 2]);
      if (hi >= 0 && lo >= 0) {
        out.push_back(static_cast<char>((hi << 4) | lo));
        i += 2;
        continue;
      }
    }
    out.push_back(s[i]);
  }
  return out;
}

inline bool parse_disabled_cg_attr(
    const ucis_db::xml_node& attr,
    const std::string& inst_mod_name,
    const std::set<std::string>& known_cg_types,
    const std::map<std::string, std::set<std::string> >& known_cg_mods,
    xml_disabled_cg_info& out)
{
  const std::string key = attr.get("key");
  const std::string type = attr.get("type");
  const std::string val = trim_ws(attr.text);
  if (type != "str") return false;

  if (key == "fc4sc.disabled_cg") {
    if (val.rfind("v1|", 0) == 0) {
      std::vector<std::string> parts;
      size_t start = 0;
      while (true) {
        const size_t pos = val.find('|', start);
        if (pos == std::string::npos) {
          parts.push_back(val.substr(start));
          break;
        }
        parts.push_back(val.substr(start, pos - start));
        start = pos + 1;
      }
      if (parts.size() >= 8) {
        out.cg_type = pct_decode_attr_field(parts[1]);
        out.scp_type = pct_decode_attr_field(parts[2]);
        out.inst_name = pct_decode_attr_field(parts[3]);
        uint64_t v = 1;
        if (ucis_db::parse_uint(parts[4], v)) out.type_weight = static_cast<unsigned int>(v);
        v = 1;
        if (ucis_db::parse_uint(parts[5], v)) out.inst_weight = static_cast<unsigned int>(v);
        v = 100;
        if (ucis_db::parse_uint(parts[6], v)) out.type_goal = static_cast<unsigned int>(v);
        out.merge_instances = (parts[7] == "1" || parts[7] == "true");
        if (out.scp_type.empty()) out.scp_type = inst_mod_name;
        if (out.inst_name.empty()) out.inst_name = out.cg_type;
        out.has_structured_meta = true;
        return !out.cg_type.empty();
      }
    }
    if (!val.empty()) {
      out.cg_type = val;
      out.scp_type = inst_mod_name;
      out.inst_name = val;
      return true;
    }
    return false;
  }

  if (key.rfind("fc4sc.disabled_cg.", 0) == 0 && !val.empty()) {
    out.cg_type = val;
    out.scp_type = inst_mod_name;
    out.inst_name = val;
    return true;
  }

  if (!key.empty() && val == key && key.rfind("fc4sc.", 0) != 0 &&
      known_cg_types.count(key) > 0) {
    out.cg_type = key;
    out.inst_name = key;
    auto mit = known_cg_mods.find(key);
    if (mit != known_cg_mods.end() && mit->second.size() == 1) {
      out.scp_type = *mit->second.begin();
    } else {
      out.scp_type = inst_mod_name;
    }
    return true;
  }

  return false;
}

} // namespace detail

inline report_data build_from_xml(
    const ucis_db::xml_node& root,
    const report_options& opts = report_options())
{
  if (root.name != "UCIS") {
    throw std::runtime_error("The root element of a UCIS XML file must be <UCIS>");
  }

  report_data data;
  data.title = opts.title;

  std::map<ucis_db::xml_node*, std::string> paths = ucis_db::instance_paths(root);
  std::vector<std::pair<std::string, const ucis_db::xml_node*> > insts;
  for (std::map<ucis_db::xml_node*, std::string>::const_iterator it = paths.begin();
       it != paths.end(); ++it) {
    insts.push_back(std::make_pair(it->second, it->first));
  }
  std::sort(insts.begin(), insts.end(),
      [](const std::pair<std::string, const ucis_db::xml_node*>& a,
         const std::pair<std::string, const ucis_db::xml_node*>& b) {
        return a.first < b.first;
      });

  std::set<std::string> known_cg_types;
  std::map<std::string, std::set<std::string> > known_cg_mods;
  for (size_t ii = 0; ii < insts.size(); ++ii) {
    const ucis_db::xml_node& inst = *insts[ii].second;
    const std::string inst_mod = inst.get("moduleName");
    std::vector<ucis_db::xml_node*> cgs = inst.elements("covergroupCoverage");
    for (size_t cgi = 0; cgi < cgs.size(); ++cgi) {
      std::vector<ucis_db::xml_node*> cg_insts = cgs[cgi]->elements("cgInstance");
      for (size_t ci = 0; ci < cg_insts.size(); ++ci) {
        const ucis_db::xml_node* cgid = cg_insts[ci]->element("cgId");
        if (cgid != nullptr) {
          const std::string cgn = cgid->get("cgName");
          std::string modn = cgid->get("moduleName");
          if (modn.empty()) modn = inst_mod;
          if (!cgn.empty()) {
            known_cg_types.insert(cgn);
            if (!modn.empty()) known_cg_mods[cgn].insert(modn);
          }
        }
      }
    }
  }

  double total_cg_weighted = 0.0;
  double total_cg_weights = 0.0;
  std::map<std::string, detail::xml_merged_cg_type> cg_types;
  std::vector<std::string> cg_type_order;
  size_t anon_type_seq = 0;

  size_t blk_cov = 0, blk_tot = 0;
  size_t cnd_cov = 0, cnd_tot = 0;
  size_t br_cov = 0, br_tot = 0;
  size_t fsm_cov = 0, fsm_tot = 0;

  for (size_t ii = 0; ii < insts.size(); ++ii) {
    const std::string& scope_path = insts[ii].first;
    const ucis_db::xml_node& inst = *insts[ii].second;
    bool scope_has_content = false;

    if (opts.include_toggle_coverage) {
      std::vector<ucis_db::xml_node*> tcs = inst.elements("toggleCoverage");
      for (size_t tci = 0; tci < tcs.size(); ++tci) {
        std::vector<ucis_db::xml_node*> objs = tcs[tci]->elements("toggleObject");
        for (size_t oi = 0; oi < objs.size(); ++oi) {
          const ucis_db::xml_node& obj = *objs[oi];
          report_toggle_entry te;
          te.scope_path = scope_path;
          te.name = obj.get("name");
          te.type_name = obj.get("type");
          te.port_direction = obj.get("portDirection");
          std::vector<ucis_db::xml_node*> bits = obj.elements("toggleBit");
          te.bit_width = static_cast<unsigned int>(bits.size());
          for (size_t bi = 0; bi < bits.size(); ++bi) {
            std::vector<ucis_db::xml_node*> togs = bits[bi]->elements("toggle");
            for (size_t ti = 0; ti < togs.size(); ++ti) {
              const ucis_db::xml_node* bin = togs[ti]->element("bin");
              if (bin == nullptr || bin->get("excluded") == "true") continue;
              const uint64_t cnt = detail::xml_bin_count(*bin);
              const unsigned int goal = detail::xml_uint_attr(*bin, "coverageCountGoal", 1U);
              te.total_transitions++;
              if (cnt >= goal) te.covered_transitions++;
              const std::string from = togs[ti]->get("from");
              const std::string to = togs[ti]->get("to");
              if (from == "0" && to == "1") te.transitions_01 += cnt;
              else if (from == "1" && to == "0") te.transitions_10 += cnt;
              else te.other_transitions += cnt;
            }
          }
          te.coverage_pct = (te.total_transitions > 0)
              ? (100.0 * static_cast<double>(te.covered_transitions) / static_cast<double>(te.total_transitions))
              : 100.0;
          data.toggle_objects.push_back(te);
          scope_has_content = true;
        }
      }
    }

    if (opts.include_assertion_coverage) {
      std::vector<ucis_db::xml_node*> acs = inst.elements("assertionCoverage");
      for (size_t aci = 0; aci < acs.size(); ++aci) {
        std::vector<ucis_db::xml_node*> asrts = acs[aci]->elements("assertion");
        for (size_t ai = 0; ai < asrts.size(); ++ai) {
          const ucis_db::xml_node& a = *asrts[ai];
          assertion_item_data item;
          item.name = a.get("name");
          item.assertion_kind = a.get("assertionKind");
          if (const ucis_db::xml_node* b = a.element("coverBin")) item.cover_count = detail::xml_bin_count(*b);
          if (const ucis_db::xml_node* b = a.element("passBin")) item.pass_count = detail::xml_bin_count(*b);
          if (const ucis_db::xml_node* b = a.element("failBin")) item.fail_count = detail::xml_bin_count(*b);
          if (const ucis_db::xml_node* b = a.element("vacuousBin")) item.vacuous_count = detail::xml_bin_count(*b);
          if (const ucis_db::xml_node* b = a.element("disabledBin")) item.disabled_count = detail::xml_bin_count(*b);
          if (const ucis_db::xml_node* b = a.element("attemptBin")) item.attempt_count = detail::xml_bin_count(*b);
          if (const ucis_db::xml_node* b = a.element("activeBin")) item.active_count = detail::xml_bin_count(*b);
          if (const ucis_db::xml_node* b = a.element("peakActiveBin")) item.peak_active_count = detail::xml_bin_count(*b);
          std::vector<ucis_db::xml_node*> attrs = a.elements("userAttr");
          for (size_t ui = 0; ui < attrs.size(); ++ui) {
            if (attrs[ui]->get("key") == "fc4sc.expr") {
              item.expr_str = detail::trim_ws(attrs[ui]->text);
            }
          }
          data.assertions.push_back(detail::build_assertion_entry(scope_path, item));
          scope_has_content = true;
        }
      }
    }

    if (opts.include_other_ucis_metrics) {
      const size_t prev_tot = blk_tot + cnd_tot + br_tot + fsm_tot;
      std::vector<ucis_db::xml_node*> bcs = inst.elements("blockCoverage");
      for (size_t i = 0; i < bcs.size(); ++i) detail::collect_block_metrics(*bcs[i], blk_cov, blk_tot);

      static const char* const cond_bins[] = {"bin"};
      std::vector<ucis_db::xml_node*> ccs = inst.elements("conditionCoverage");
      for (size_t i = 0; i < ccs.size(); ++i) detail::collect_named_bin_metrics(*ccs[i], cond_bins, 1, cnd_cov, cnd_tot);

      static const char* const br_bins[] = {"branchBin"};
      std::vector<ucis_db::xml_node*> brcs = inst.elements("branchCoverage");
      for (size_t i = 0; i < brcs.size(); ++i) detail::collect_named_bin_metrics(*brcs[i], br_bins, 1, br_cov, br_tot);

      static const char* const fsm_bins[] = {"stateBin", "transitionBin"};
      std::vector<ucis_db::xml_node*> fcs = inst.elements("fsmCoverage");
      for (size_t i = 0; i < fcs.size(); ++i) detail::collect_named_bin_metrics(*fcs[i], fsm_bins, 2, fsm_cov, fsm_tot);
      if ((blk_tot + cnd_tot + br_tot + fsm_tot) > prev_tot) {
        scope_has_content = true;
      }
    }

    // Collect disabled covergroups (unambiguous fc4sc.disabled_cg* marker, or legacy key==val for known types)
    std::vector<detail::xml_disabled_cg_info> disabled_cgs;
    std::vector<ucis_db::xml_node*> inst_attrs = inst.elements("userAttr");
    const std::string inst_mod_name = inst.get("moduleName");
    for (size_t ui = 0; ui < inst_attrs.size(); ++ui) {
      detail::xml_disabled_cg_info dinfo;
      if (detail::parse_disabled_cg_attr(*inst_attrs[ui], inst_mod_name,
                                         known_cg_types, known_cg_mods, dinfo)) {
        disabled_cgs.push_back(dinfo);
      }
    }

    std::vector<ucis_db::xml_node*> cgs = inst.elements("covergroupCoverage");
    for (size_t cgi = 0; cgi < cgs.size(); ++cgi) {
      scope_has_content = true;
      const unsigned int type_weight = detail::xml_uint_attr(*cgs[cgi], "weight", 1U);
      unsigned int group_type_goal = 100U;
      bool has_explicit_type_goal = false;
      std::vector<ucis_db::xml_node*> cg_attrs = cgs[cgi]->elements("userAttr");
      for (size_t ui = 0; ui < cg_attrs.size(); ++ui) {
        if (cg_attrs[ui]->get("key") == "fc4sc.type_goal") {
          uint64_t gval = 100;
          if (ucis_db::parse_uint(detail::trim_ws(cg_attrs[ui]->text), gval)) {
            group_type_goal = static_cast<unsigned int>(gval);
            has_explicit_type_goal = true;
          }
        }
      }

      std::vector<ucis_db::xml_node*> cg_insts = cgs[cgi]->elements("cgInstance");

      std::string group_mod_name;
      std::string group_cg_name;
      bool group_merge_instances = false;
      unsigned int type_inst_goal = 100U;
      bool has_type_inst_goal = false;
      for (size_t ci = 0; ci < cg_insts.size(); ++ci) {
        const ucis_db::xml_node* cgid = cg_insts[ci]->element("cgId");
        if (cgid != nullptr) {
          if (group_cg_name.empty()) group_cg_name = cgid->get("cgName");
          if (group_mod_name.empty()) group_mod_name = cgid->get("moduleName");
        }
        const ucis_db::xml_node* cg_opts = cg_insts[ci]->element("options");
        if (cg_opts != nullptr) {
          if (cg_opts->get("merge_instances") == "true") {
            group_merge_instances = true;
          }
          if (cg_insts[ci]->get("name").empty()) {
            type_inst_goal = detail::xml_uint_attr(*cg_opts, "goal", 100U);
            has_type_inst_goal = true;
          }
        }
      }
      if (group_mod_name.empty()) group_mod_name = inst_mod_name;

      const std::string type_key = (!group_mod_name.empty() && !group_cg_name.empty())
          ? (group_mod_name + "\n" + group_cg_name)
          : ("__anon_" + std::to_string(anon_type_seq++));
      if (cg_types.count(type_key) == 0) {
        cg_type_order.push_back(type_key);
      }
      detail::xml_merged_cg_type& mtype = cg_types[type_key];
      mtype.type_weight = type_weight;
      mtype.has_authoritative_type_opts = true;
      if (has_explicit_type_goal) {
        mtype.type_goal = group_type_goal;
        mtype.has_type_goal = true;
      } else if (has_type_inst_goal && !mtype.has_type_goal) {
        mtype.type_goal = type_inst_goal;
        mtype.has_type_goal = true;
      }
      if (group_merge_instances) mtype.merge_instances = true;

      for (size_t ci = 0; ci < cg_insts.size(); ++ci) {
        const ucis_db::xml_node& cg_node = *cg_insts[ci];
        report_covergroup rcg;
        rcg.scope_path = scope_path;
        rcg.name = cg_node.get("name");
        const ucis_db::xml_node* cgid = cg_node.element("cgId");
        rcg.type_name = (cgid != nullptr) ? cgid->get("cgName") : std::string();
        const ucis_db::xml_node* cg_opts = cg_node.element("options");
        if (cg_opts != nullptr) {
          rcg.weight = detail::xml_uint_attr(*cg_opts, "weight", 1U);
          rcg.goal = detail::xml_uint_attr(*cg_opts, "goal", 100U);
          rcg.per_instance = (cg_opts->get("per_instance") == "true");
          rcg.comment = cg_opts->get("comment");
        }

        double cvp_wsum = 0.0;
        double cvp_wtot = 0.0;
        std::vector<ucis_db::xml_node*> cvps = cg_node.elements("coverpoint");
        for (size_t pi = 0; pi < cvps.size(); ++pi) {
          const ucis_db::xml_node& cp_node = *cvps[pi];
          report_coverpoint rcp;
          rcp.name = cp_node.get("name");
          const ucis_db::xml_node* cp_opts = cp_node.element("options");
          if (cp_opts != nullptr) {
            rcp.at_least = detail::xml_uint_attr(*cp_opts, "at_least", 1U);
            rcp.weight = detail::xml_uint_attr(*cp_opts, "weight", 1U);
            rcp.goal = detail::xml_uint_attr(*cp_opts, "goal", 100U);
          }

          const bool first_cp = (mtype.cvps.count(rcp.name) == 0);
          if (first_cp) {
            mtype.cvp_order.push_back(rcp.name);
            detail::xml_merged_cvp& mcp = mtype.cvps[rcp.name];
            mcp.is_cross = false;
            mcp.at_least = rcp.at_least;
            mcp.weight = rcp.weight;
            mcp.goal = rcp.goal;
          }
          detail::xml_merged_cvp& mcp = mtype.cvps[rcp.name];
          std::map<std::string, size_t> seen_bin_occ;

          std::vector<ucis_db::xml_node*> bins = cp_node.elements("coverpointBin");
          for (size_t bi = 0; bi < bins.size(); ++bi) {
            const ucis_db::xml_node& b = *bins[bi];
            if (ucis_db::merger::is_empty_placeholder_bin(b)) continue;
            const std::string btype = b.get("type");
            const bool counted = (btype.empty() || btype == "default");
            if (!counted && !opts.include_ignore_illegal_bins) continue;

            std::vector<xml_user_attr> attrs;
            std::vector<ucis_db::xml_node*> uas = b.elements("userAttr");
            for (size_t ui = 0; ui < uas.size(); ++ui) {
              xml_user_attr ua;
              ua.key = uas[ui]->get("key");
              ua.type = uas[ui]->get("type");
              ua.value = detail::trim_ws(uas[ui]->text);
              attrs.push_back(ua);
            }
            std::vector<std::pair<std::string, std::string> > ivs;
            uint64_t hits = 0;
            std::vector<ucis_db::xml_node*> ranges = b.elements("range");
            for (size_t ri = 0; ri < ranges.size(); ++ri) {
              ivs.push_back(std::make_pair(ranges[ri]->get("from"), ranges[ri]->get("to")));
              hits += detail::xml_bin_count(*ranges[ri]);
            }
            std::vector<std::vector<std::string> > seqs;
            std::vector<ucis_db::xml_node*> seq_nodes = b.elements("sequence");
            for (size_t si = 0; si < seq_nodes.size(); ++si) {
              hits += detail::xml_bin_count(*seq_nodes[si]);
              std::vector<std::string> vals;
              std::vector<ucis_db::xml_node*> svs = seq_nodes[si]->elements("seqValue");
              for (size_t vi = 0; vi < svs.size(); ++vi) {
                vals.push_back(detail::trim_ws(svs[vi]->text));
              }
              seqs.push_back(vals);
            }

            const std::string bname = b.get("name");
            if (counted) {
              rcp.total_bins++;
              if (hits >= rcp.at_least) rcp.covered_bins++;
              const size_t occ = seen_bin_occ[bname]++;
              mcp.cp_bins[std::make_pair(bname, occ)] += hits;
            }
            if (opts.include_bins) {
              report_bin rb;
              rb.name = bname;
              rb.kind = counted ? std::string("bin") : btype;
              rb.domain = detail::format_bin_domain_from_attrs(attrs, ivs, seqs);
              rb.hits = hits;
              rb.at_least = rcp.at_least;
              rb.counted = counted;
              rb.covered = counted && (hits >= rcp.at_least);
              rcp.bins.push_back(rb);
            }
          }
          if (rcp.total_bins == 0) {
            rcp.coverage_pct = (rcp.weight == 0) ? 100.0 : 0.0;
          } else {
            const double raw = 100.0 * static_cast<double>(rcp.covered_bins) / static_cast<double>(rcp.total_bins);
            rcp.coverage_pct = (raw >= static_cast<double>(rcp.goal)) ? 100.0 : raw;
          }
          rcg.covered_bins += rcp.covered_bins;
          rcg.total_bins += rcp.total_bins;
          cvp_wsum += rcp.coverage_pct * static_cast<double>(rcp.weight);
          cvp_wtot += static_cast<double>(rcp.weight);
          rcg.coverpoints.push_back(rcp);
        }

        std::vector<ucis_db::xml_node*> crosses = cg_node.elements("cross");
        for (size_t xi = 0; xi < crosses.size(); ++xi) {
          const ucis_db::xml_node& cr_node = *crosses[xi];
          report_cross rcr;
          rcr.name = cr_node.get("name");
          std::vector<ucis_db::xml_node*> exprs = cr_node.elements("crossExpr");
          for (size_t ei = 0; ei < exprs.size(); ++ei) {
            if (ei > 0) rcr.expr_str += " x ";
            rcr.expr_str += detail::trim_ws(exprs[ei]->text);
          }
          const ucis_db::xml_node* cr_opts = cr_node.element("options");
          if (cr_opts != nullptr) {
            rcr.at_least = detail::xml_uint_attr(*cr_opts, "at_least", 1U);
            rcr.weight = detail::xml_uint_attr(*cr_opts, "weight", 1U);
            rcr.goal = detail::xml_uint_attr(*cr_opts, "goal", 100U);
          }

          const bool first_cr = (mtype.cvps.count(rcr.name) == 0);
          if (first_cr) {
            mtype.cvp_order.push_back(rcr.name);
            detail::xml_merged_cvp& mcr = mtype.cvps[rcr.name];
            mcr.is_cross = true;
            mcr.at_least = rcr.at_least;
            mcr.weight = rcr.weight;
            mcr.goal = rcr.goal;
          }
          detail::xml_merged_cvp& mcr = mtype.cvps[rcr.name];

          std::vector<ucis_db::xml_node*> cbins = cr_node.elements("crossBin");
          for (size_t bi = 0; bi < cbins.size(); ++bi) {
            const ucis_db::xml_node& b = *cbins[bi];
            const std::string btype = b.get("type");
            const bool counted = (btype.empty() || btype == "default");
            if (!counted && !opts.include_ignore_illegal_bins) continue;
            const uint64_t hits = detail::xml_bin_count(b);
            const std::string bname = b.get("name");
            if (counted) {
              rcr.total_bins++;
              if (hits >= rcr.at_least) rcr.covered_bins++;
              mcr.cross_bins[bname] += hits;
            }
            if (opts.include_bins) {
              report_bin rb;
              rb.name = bname;
              rb.kind = counted ? std::string("cross") : btype;
              rb.domain = rb.name;
              rb.hits = hits;
              rb.at_least = rcr.at_least;
              rb.counted = counted;
              rb.covered = counted && (hits >= rcr.at_least);
              rcr.bins.push_back(rb);
            }
          }
          if (rcr.total_bins == 0) {
            rcr.coverage_pct = (rcr.weight == 0) ? 100.0 : 0.0;
          } else {
            const double raw = 100.0 * static_cast<double>(rcr.covered_bins) / static_cast<double>(rcr.total_bins);
            rcr.coverage_pct = (raw >= static_cast<double>(rcr.goal)) ? 100.0 : raw;
          }
          rcg.covered_bins += rcr.covered_bins;
          rcg.total_bins += rcr.total_bins;
          cvp_wsum += rcr.coverage_pct * static_cast<double>(rcr.weight);
          cvp_wtot += static_cast<double>(rcr.weight);
          rcg.crosses.push_back(rcr);
        }

        if ((rcg.coverpoints.empty() && rcg.crosses.empty()) || cvp_wtot == 0.0 || cvp_wsum == 0.0) {
          rcg.coverage_pct = (rcg.weight == 0) ? 100.0 : 0.0;
        } else {
          const double raw = cvp_wsum / cvp_wtot;
          rcg.coverage_pct = (raw >= static_cast<double>(rcg.goal)) ? 100.0 : raw;
        }
        data.total_covergroups++;
        data.enabled_covergroups++;
        data.covered_cg_bins += rcg.covered_bins;
        data.total_cg_bins += rcg.total_bins;
        mtype.has_enabled = true;
        mtype.inst_weighted_sum += rcg.coverage_pct * static_cast<double>(rcg.weight);
        mtype.inst_weight_total += static_cast<double>(rcg.weight);
        data.covergroups.push_back(rcg);
      }
    }

    for (size_t di = 0; di < disabled_cgs.size(); ++di) {
      const detail::xml_disabled_cg_info& d = disabled_cgs[di];
      scope_has_content = true;
      report_covergroup rcg;
      rcg.scope_path = scope_path;
      rcg.type_name = d.cg_type;
      rcg.name = d.inst_name;
      rcg.enabled = false;
      rcg.weight = d.inst_weight;
      rcg.goal = d.type_goal;
      rcg.coverage_pct = 100.0;
      data.total_covergroups++;
      data.covergroups.push_back(rcg);

      const std::string dtype_key = (!d.scp_type.empty() && !d.cg_type.empty())
          ? (d.scp_type + "\n" + d.cg_type)
          : ("__anon_dis_" + d.cg_type);
      if (cg_types.count(dtype_key) == 0) {
        cg_type_order.push_back(dtype_key);
      }
      detail::xml_merged_cg_type& mtype = cg_types[dtype_key];
      if (!mtype.has_authoritative_type_opts) {
        mtype.type_weight = d.type_weight;
        if (!mtype.has_type_goal) {
          mtype.type_goal = d.type_goal;
          if (d.has_structured_meta) mtype.has_type_goal = true;
        }
        if (d.merge_instances) mtype.merge_instances = true;
        if (d.has_structured_meta) mtype.has_authoritative_type_opts = true;
      }
      mtype.inst_weighted_sum += 100.0 * static_cast<double>(d.inst_weight);
      mtype.inst_weight_total += static_cast<double>(d.inst_weight);
    }

    if (opts.include_empty_scopes && !scope_has_content) {
      data.empty_scopes.push_back(scope_path);
    }
  }

  for (size_t ti = 0; ti < cg_type_order.size(); ++ti) {
    const detail::xml_merged_cg_type& mtype = cg_types[cg_type_order[ti]];
    double type_cov = 0.0;
    if (mtype.merge_instances) {
      if (!mtype.has_enabled) {
        type_cov = 100.0;
      } else {
        double msum = 0.0;
        double mweights = 0.0;
        for (size_t pi = 0; pi < mtype.cvp_order.size(); ++pi) {
          const detail::xml_merged_cvp& mc = mtype.cvps.find(mtype.cvp_order[pi])->second;
          size_t cov = 0;
          size_t tot = 0;
          if (mc.is_cross) {
            tot = mc.cross_bins.size();
            for (std::map<std::string, uint64_t>::const_iterator it = mc.cross_bins.begin();
                 it != mc.cross_bins.end(); ++it) {
              if (it->second >= mc.at_least) cov++;
            }
          } else {
            tot = mc.cp_bins.size();
            for (std::map<std::pair<std::string, size_t>, uint64_t>::const_iterator it = mc.cp_bins.begin();
                 it != mc.cp_bins.end(); ++it) {
              if (it->second >= mc.at_least) cov++;
            }
          }
          double cp_cov = 0.0;
          if (tot == 0) {
            cp_cov = (mc.weight == 0) ? 100.0 : 0.0;
          } else {
            const double raw = 100.0 * static_cast<double>(cov) / static_cast<double>(tot);
            cp_cov = (raw >= static_cast<double>(mc.goal)) ? 100.0 : raw;
          }
          msum += cp_cov * static_cast<double>(mc.weight);
          mweights += static_cast<double>(mc.weight);
        }
        if (mweights == 0.0 || mtype.cvp_order.empty() || msum == 0.0) {
          type_cov = (mtype.type_weight == 0) ? 100.0 : 0.0;
        } else {
          const double raw = msum / mweights;
          type_cov = (raw >= static_cast<double>(mtype.type_goal)) ? 100.0 : raw;
        }
      }
    } else {
      if (mtype.inst_weight_total == 0.0 || mtype.inst_weighted_sum == 0.0) {
        type_cov = (mtype.type_weight == 0) ? 100.0 : 0.0;
      } else {
        const double raw = mtype.inst_weighted_sum / mtype.inst_weight_total;
        type_cov = (raw >= static_cast<double>(mtype.type_goal)) ? 100.0 : raw;
      }
    }
    total_cg_weighted += type_cov * static_cast<double>(mtype.type_weight);
    total_cg_weights += static_cast<double>(mtype.type_weight);
  }

  auto push_other = [&](const char* nm, size_t cov, size_t tot) {
    if (tot > 0) {
      report_other_metric m;
      m.metric_name = nm;
      m.covered = cov;
      m.total = tot;
      m.coverage_pct = 100.0 * static_cast<double>(cov) / static_cast<double>(tot);
      data.other_metrics.push_back(m);
    }
  };
  push_other("Block", blk_cov, blk_tot);
  push_other("Condition", cnd_cov, cnd_tot);
  push_other("Branch", br_cov, br_tot);
  push_other("FSM", fsm_cov, fsm_tot);

  if (total_cg_weights > 0.0) {
    data.overall_coverage_pct = total_cg_weighted / total_cg_weights;
  } else if (insts.empty()) {
    data.overall_coverage_pct = 100.0;
  } else {
    data.overall_coverage_pct = 0.0;
  }

  return data;
}

inline report_data build_from_xml_file(
    const std::string& xml_path,
    const report_options& opts = report_options())
{
  auto root = ucis_db::read_xml_file(xml_path);
  return build_from_xml(*root, opts);
}

inline void render_markdown_report(
    std::ostream& os,
    const report_data& data,
    const report_options& opts = report_options())
{
  os << "# " << detail::escape_md(data.title) << "\n\n";
  os << "## Executive Summary\n\n";
  os << "| Metric | Value |\n";
  os << "|---|---|\n";
  os << "| Functional Coverage | " << detail::fmt_pct(data.overall_coverage_pct) << " |\n";
  os << "| Covergroups (Enabled / Total) | " << data.enabled_covergroups << " / " << data.total_covergroups << " |\n";
  os << "| Covergroup Bins (Covered / Total) | " << data.covered_cg_bins << " / " << data.total_cg_bins << " |\n";

  if (opts.include_toggle_coverage && !data.toggle_objects.empty()) {
    size_t tcov = 0, ttot = 0;
    for (size_t i = 0; i < data.toggle_objects.size(); ++i) {
      tcov += data.toggle_objects[i].covered_transitions;
      ttot += data.toggle_objects[i].total_transitions;
    }
    const double tpct = (ttot > 0) ? (100.0 * static_cast<double>(tcov) / static_cast<double>(ttot)) : 100.0;
    os << "| Toggle Coverage | " << detail::fmt_pct(tpct) << " (" << tcov << " / " << ttot << ") |\n";
  }

  if (opts.include_assertion_coverage && !data.assertions.empty()) {
    size_t apass = 0, afail = 0, acov = 0;
    for (size_t i = 0; i < data.assertions.size(); ++i) {
      if (data.assertions[i].fail_count > 0) afail++;
      else if (data.assertions[i].pass_count > 0) apass++;
      if (data.assertions[i].cover_count > 0) acov++;
    }
    os << "| Assertions | " << data.assertions.size() << " total (" << apass << " pass, " << afail << " fail, " << acov << " cover hit) |\n";
  }
  os << "\n";

  if (!data.covergroups.empty()) {
    os << "## Covergroup Summary\n\n";
    os << "| Scope | Type | Instance | Status | Weight | Goal | Bins | Coverage |\n";
    os << "|---|---|---|---|---|---|---|---|\n";
    for (size_t i = 0; i < data.covergroups.size(); ++i) {
      const report_covergroup& cg = data.covergroups[i];
      os << "| " << detail::escape_md(cg.scope_path)
         << " | " << detail::escape_md(cg.type_name)
         << " | " << detail::escape_md(cg.name)
         << " | " << (cg.enabled ? "Enabled" : "Disabled")
         << " | " << cg.weight
         << " | " << cg.goal
         << " | " << cg.covered_bins << " / " << cg.total_bins
         << " | " << detail::fmt_pct(cg.coverage_pct) << " |\n";
    }
    os << "\n";

    os << "## Covergroup Breakdown\n\n";
    for (size_t i = 0; i < data.covergroups.size(); ++i) {
      const report_covergroup& cg = data.covergroups[i];
      os << "### " << detail::escape_md(cg.type_name) << "::" << detail::escape_md(cg.name)
         << " (" << detail::fmt_pct(cg.coverage_pct) << ")\n\n";
      if (!cg.comment.empty()) {
        os << "- Comment: " << detail::escape_md(cg.comment) << "\n";
      }
      if (!cg.iff_str.empty()) {
        os << "- Guard (`iff`): `" << detail::escape_md(cg.iff_str) << "`\n";
      }
      if (!cg.enabled) {
        os << "- Status: Disabled\n\n";
        continue;
      }
      os << "\n";

      for (size_t pi = 0; pi < cg.coverpoints.size(); ++pi) {
        const report_coverpoint& cp = cg.coverpoints[pi];
        os << "#### Coverpoint `" << detail::escape_md(cp.name) << "` — "
           << detail::fmt_pct(cp.coverage_pct) << " (" << cp.covered_bins << "/" << cp.total_bins
           << ", at_least=" << cp.at_least << ", misses=" << cp.misses << ")\n\n";
        if (opts.include_bins && !cp.bins.empty()) {
          os << "| Bin | Kind | Values | Hits | At Least | Status |\n";
          os << "|---|---|---|---|---|---|\n";
          for (size_t bi = 0; bi < cp.bins.size(); ++bi) {
            const report_bin& b = cp.bins[bi];
            const char* st = !b.counted ? "N/A" : (b.covered ? "COVERED" : "UNCOVERED");
            os << "| " << detail::escape_md(b.name)
               << " | " << detail::escape_md(b.kind)
               << " | " << detail::escape_md(b.domain)
               << " | " << b.hits
               << " | " << b.at_least
               << " | " << st << " |\n";
          }
          os << "\n";
        }
      }

      for (size_t xi = 0; xi < cg.crosses.size(); ++xi) {
        const report_cross& cr = cg.crosses[xi];
        os << "#### Cross `" << detail::escape_md(cr.name) << "` (`" << detail::escape_md(cr.expr_str)
           << "`) — " << detail::fmt_pct(cr.coverage_pct) << " (" << cr.covered_bins << "/" << cr.total_bins << ")\n\n";
        if (opts.include_bins && !cr.bins.empty()) {
          os << "| Tuple | Kind | Hits | At Least | Status |\n";
          os << "|---|---|---|---|---|\n";
          for (size_t bi = 0; bi < cr.bins.size(); ++bi) {
            const report_bin& b = cr.bins[bi];
            const char* st = !b.counted ? "N/A" : (b.covered ? "COVERED" : "UNCOVERED");
            os << "| " << detail::escape_md(b.name)
               << " | " << detail::escape_md(b.kind)
               << " | " << b.hits
               << " | " << b.at_least
               << " | " << st << " |\n";
          }
          if (cr.truncated_missing > 0) {
            os << "\n*(" << cr.truncated_missing << " additional uncovered cross tuples truncated)*\n";
          }
          os << "\n";
        }
      }
    }
  }

  if (opts.include_toggle_coverage && !data.toggle_objects.empty()) {
    os << "## Toggle Coverage\n\n";
    os << "| Scope | Signal/Port | Type | Dir | Width | 0->1 | 1->0 | Other | Transitions | Coverage |\n";
    os << "|---|---|---|---|---|---|---|---|---|---|\n";
    for (size_t i = 0; i < data.toggle_objects.size(); ++i) {
      const report_toggle_entry& t = data.toggle_objects[i];
      os << "| " << detail::escape_md(t.scope_path)
         << " | " << detail::escape_md(t.name)
         << " | " << detail::escape_md(t.type_name)
         << " | " << detail::escape_md(t.port_direction)
         << " | " << t.bit_width
         << " | " << t.transitions_01
         << " | " << t.transitions_10
         << " | " << t.other_transitions
         << " | " << t.covered_transitions << " / " << t.total_transitions
         << " | " << detail::fmt_pct(t.coverage_pct) << " |\n";
    }
    os << "\n";
  }

  if (opts.include_assertion_coverage && !data.assertions.empty()) {
    os << "## Assertion Coverage\n\n";
    os << "| Scope | Name | Kind | Attempts | Pass | Fail | Vacuous | Disabled | Cover | Active | Peak Active | Status |\n";
    os << "|---|---|---|---|---|---|---|---|---|---|---|---|\n";
    for (size_t i = 0; i < data.assertions.size(); ++i) {
      const report_assertion_entry& a = data.assertions[i];
      os << "| " << detail::escape_md(a.scope_path)
         << " | " << detail::escape_md(a.name)
         << " | " << detail::escape_md(a.assertion_kind)
         << " | " << a.attempt_count
         << " | " << a.pass_count
         << " | " << a.fail_count
         << " | " << a.vacuous_count
         << " | " << a.disabled_count
         << " | " << a.cover_count
         << " | " << a.active_count
         << " | " << a.peak_active_count
         << " | " << detail::escape_md(a.status) << " |\n";
    }
    os << "\n";
  }

  if (opts.include_other_ucis_metrics && !data.other_metrics.empty()) {
    os << "## Other UCIS Metrics\n\n";
    os << "| Metric | Covered | Total | Coverage |\n";
    os << "|---|---|---|---|\n";
    for (size_t i = 0; i < data.other_metrics.size(); ++i) {
      const report_other_metric& m = data.other_metrics[i];
      os << "| " << detail::escape_md(m.metric_name)
         << " | " << m.covered
         << " | " << m.total
         << " | " << detail::fmt_pct(m.coverage_pct) << " |\n";
    }
    os << "\n";
  }

  if (opts.include_empty_scopes && !data.empty_scopes.empty()) {
    os << "## Empty Scopes\n\n";
    for (size_t i = 0; i < data.empty_scopes.size(); ++i) {
      os << "- `" << detail::escape_md(data.empty_scopes[i]) << "`\n";
    }
    os << "\n";
  }
}

inline void render_html_report(
    std::ostream& os,
    const report_data& data,
    const report_options& opts = report_options())
{
  auto badge_cls = [](double pct) -> const char* {
    if (pct >= 100.0) return "badge-pass";
    if (pct >= 50.0) return "badge-warn";
    return "badge-fail";
  };

  os << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"UTF-8\">\n"
     << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
     << "<title>" << detail::escape_html(data.title) << "</title>\n"
     << "<style>\n"
     << "body{font-family:-apple-system,BlinkMacSystemFont,\"Segoe UI\",Roboto,Helvetica,Arial,sans-serif;"
     << "margin:24px;background:#f7f9fc;color:#1f2937;line-height:1.45;}\n"
     << "h1,h2,h3,h4{color:#111827;margin-top:1.2em;}\n"
     << ".card{background:#ffffff;border:1px solid #e5e7eb;border-radius:8px;padding:16px;margin-bottom:16px;}\n"
     << "table{border-collapse:collapse;width:100%;margin:10px 0;font-size:0.92em;}\n"
     << "th,td{border:1px solid #e5e7eb;padding:6px 10px;text-align:left;vertical-align:top;}\n"
     << "th{background:#f3f4f6;font-weight:600;}\n"
     << "tr:nth-child(even){background:#fafafa;}\n"
     << ".badge{display:inline-block;padding:2px 8px;border-radius:9999px;font-size:0.82em;font-weight:600;}\n"
     << ".badge-pass{background:#dcfce7;color:#166534;}\n"
     << ".badge-warn{background:#fef3c7;color:#92400e;}\n"
     << ".badge-fail{background:#fee2e2;color:#991b1b;}\n"
     << ".badge-muted{background:#e5e7eb;color:#374151;}\n"
     << ".bar{width:120px;height:10px;background:#e5e7eb;border-radius:5px;overflow:hidden;display:inline-block;vertical-align:middle;margin-right:6px;}\n"
     << ".bar-fill{height:100%;background:#2563eb;}\n"
     << "details{margin:8px 0;border:1px solid #e5e7eb;border-radius:6px;padding:8px 12px;background:#fff;}\n"
     << "summary{cursor:pointer;font-weight:600;}\n"
     << "code{background:#f3f4f6;padding:1px 4px;border-radius:4px;font-size:0.9em;}\n"
     << "</style>\n</head>\n<body>\n";

  os << "<h1>" << detail::escape_html(data.title) << "</h1>\n";

  os << "<div class=\"card\">\n<h2>Executive Summary</h2>\n<table>\n"
     << "<tr><th>Metric</th><th>Value</th></tr>\n"
     << "<tr><td>Functional Coverage</td><td><span class=\"badge " << badge_cls(data.overall_coverage_pct) << "\">"
     << detail::fmt_pct(data.overall_coverage_pct) << "</span></td></tr>\n"
     << "<tr><td>Covergroups (Enabled / Total)</td><td>" << data.enabled_covergroups << " / " << data.total_covergroups << "</td></tr>\n"
     << "<tr><td>Covergroup Bins (Covered / Total)</td><td>" << data.covered_cg_bins << " / " << data.total_cg_bins << "</td></tr>\n";

  if (opts.include_toggle_coverage && !data.toggle_objects.empty()) {
    size_t tcov = 0, ttot = 0;
    for (size_t i = 0; i < data.toggle_objects.size(); ++i) {
      tcov += data.toggle_objects[i].covered_transitions;
      ttot += data.toggle_objects[i].total_transitions;
    }
    const double tpct = (ttot > 0) ? (100.0 * static_cast<double>(tcov) / static_cast<double>(ttot)) : 100.0;
    os << "<tr><td>Toggle Coverage</td><td><span class=\"badge " << badge_cls(tpct) << "\">"
       << detail::fmt_pct(tpct) << "</span> (" << tcov << " / " << ttot << ")</td></tr>\n";
  }

  if (opts.include_assertion_coverage && !data.assertions.empty()) {
    size_t apass = 0, afail = 0, acov = 0;
    for (size_t i = 0; i < data.assertions.size(); ++i) {
      if (data.assertions[i].fail_count > 0) afail++;
      else if (data.assertions[i].pass_count > 0) apass++;
      if (data.assertions[i].cover_count > 0) acov++;
    }
    os << "<tr><td>Assertions</td><td>" << data.assertions.size() << " total ("
       << apass << " pass, " << afail << " fail, " << acov << " cover hit)</td></tr>\n";
  }
  os << "</table>\n</div>\n";

  if (!data.covergroups.empty()) {
    os << "<div class=\"card\">\n<h2>Covergroup Summary</h2>\n<table>\n"
       << "<tr><th>Scope</th><th>Type</th><th>Instance</th><th>Status</th><th>Weight</th><th>Goal</th><th>Bins</th><th>Coverage</th></tr>\n";
    for (size_t i = 0; i < data.covergroups.size(); ++i) {
      const report_covergroup& cg = data.covergroups[i];
      os << "<tr><td>" << detail::escape_html(cg.scope_path) << "</td>"
         << "<td>" << detail::escape_html(cg.type_name) << "</td>"
         << "<td>" << detail::escape_html(cg.name) << "</td>"
         << "<td>" << (cg.enabled ? "Enabled" : "Disabled") << "</td>"
         << "<td>" << cg.weight << "</td>"
         << "<td>" << cg.goal << "</td>"
         << "<td>" << cg.covered_bins << " / " << cg.total_bins << "</td>"
         << "<td><span class=\"bar\"><span class=\"bar-fill\" style=\"width:"
         << std::min(100.0, std::max(0.0, cg.coverage_pct)) << "%\"></span></span>"
         << "<span class=\"badge " << badge_cls(cg.coverage_pct) << "\">"
         << detail::fmt_pct(cg.coverage_pct) << "</span></td></tr>\n";
    }
    os << "</table>\n</div>\n";

    os << "<div class=\"card\">\n<h2>Covergroup Breakdown</h2>\n";
    for (size_t i = 0; i < data.covergroups.size(); ++i) {
      const report_covergroup& cg = data.covergroups[i];
      os << "<details open>\n<summary>" << detail::escape_html(cg.type_name)
         << "::" << detail::escape_html(cg.name)
         << " — <span class=\"badge " << badge_cls(cg.coverage_pct) << "\">"
         << detail::fmt_pct(cg.coverage_pct) << "</span></summary>\n";
      if (!cg.comment.empty()) {
        os << "<p><strong>Comment:</strong> " << detail::escape_html(cg.comment) << "</p>\n";
      }
      if (!cg.iff_str.empty()) {
        os << "<p><strong>Guard (<code>iff</code>):</strong> <code>" << detail::escape_html(cg.iff_str) << "</code></p>\n";
      }
      if (!cg.enabled) {
        os << "<p><em>Disabled</em></p>\n</details>\n";
        continue;
      }
      for (size_t pi = 0; pi < cg.coverpoints.size(); ++pi) {
        const report_coverpoint& cp = cg.coverpoints[pi];
        os << "<details open>\n<summary>Coverpoint <code>" << detail::escape_html(cp.name)
           << "</code> — " << detail::fmt_pct(cp.coverage_pct)
           << " (" << cp.covered_bins << "/" << cp.total_bins
           << ", at_least=" << cp.at_least << ", misses=" << cp.misses << ")</summary>\n";
        if (opts.include_bins && !cp.bins.empty()) {
          os << "<table>\n<tr><th>Bin</th><th>Kind</th><th>Values</th><th>Hits</th><th>At Least</th><th>Status</th></tr>\n";
          for (size_t bi = 0; bi < cp.bins.size(); ++bi) {
            const report_bin& b = cp.bins[bi];
            const char* cls = !b.counted ? "badge-muted" : (b.covered ? "badge-pass" : "badge-fail");
            const char* st = !b.counted ? "N/A" : (b.covered ? "COVERED" : "UNCOVERED");
            os << "<tr><td>" << detail::escape_html(b.name) << "</td>"
               << "<td>" << detail::escape_html(b.kind) << "</td>"
               << "<td><code>" << detail::escape_html(b.domain) << "</code></td>"
               << "<td>" << b.hits << "</td>"
               << "<td>" << b.at_least << "</td>"
               << "<td><span class=\"badge " << cls << "\">" << st << "</span></td></tr>\n";
          }
          os << "</table>\n";
        }
        os << "</details>\n";
      }
      for (size_t xi = 0; xi < cg.crosses.size(); ++xi) {
        const report_cross& cr = cg.crosses[xi];
        os << "<details open>\n<summary>Cross <code>" << detail::escape_html(cr.name)
           << "</code> (<code>" << detail::escape_html(cr.expr_str) << "</code>) — "
           << detail::fmt_pct(cr.coverage_pct) << " (" << cr.covered_bins << "/" << cr.total_bins << ")</summary>\n";
        if (opts.include_bins && !cr.bins.empty()) {
          os << "<table>\n<tr><th>Tuple</th><th>Kind</th><th>Hits</th><th>At Least</th><th>Status</th></tr>\n";
          for (size_t bi = 0; bi < cr.bins.size(); ++bi) {
            const report_bin& b = cr.bins[bi];
            const char* cls = !b.counted ? "badge-muted" : (b.covered ? "badge-pass" : "badge-fail");
            const char* st = !b.counted ? "N/A" : (b.covered ? "COVERED" : "UNCOVERED");
            os << "<tr><td><code>" << detail::escape_html(b.name) << "</code></td>"
               << "<td>" << detail::escape_html(b.kind) << "</td>"
               << "<td>" << b.hits << "</td>"
               << "<td>" << b.at_least << "</td>"
               << "<td><span class=\"badge " << cls << "\">" << st << "</span></td></tr>\n";
          }
          os << "</table>\n";
          if (cr.truncated_missing > 0) {
            os << "<p><em>(" << cr.truncated_missing << " additional uncovered cross tuples truncated)</em></p>\n";
          }
        }
        os << "</details>\n";
      }
      os << "</details>\n";
    }
    os << "</div>\n";
  }

  if (opts.include_toggle_coverage && !data.toggle_objects.empty()) {
    os << "<div class=\"card\">\n<h2>Toggle Coverage</h2>\n<table>\n"
       << "<tr><th>Scope</th><th>Signal/Port</th><th>Type</th><th>Dir</th><th>Width</th><th>0-&gt;1</th><th>1-&gt;0</th><th>Other</th><th>Transitions</th><th>Coverage</th></tr>\n";
    for (size_t i = 0; i < data.toggle_objects.size(); ++i) {
      const report_toggle_entry& t = data.toggle_objects[i];
      os << "<tr><td>" << detail::escape_html(t.scope_path) << "</td>"
         << "<td>" << detail::escape_html(t.name) << "</td>"
         << "<td>" << detail::escape_html(t.type_name) << "</td>"
         << "<td>" << detail::escape_html(t.port_direction) << "</td>"
         << "<td>" << t.bit_width << "</td>"
         << "<td>" << t.transitions_01 << "</td>"
         << "<td>" << t.transitions_10 << "</td>"
         << "<td>" << t.other_transitions << "</td>"
         << "<td>" << t.covered_transitions << " / " << t.total_transitions << "</td>"
         << "<td><span class=\"badge " << badge_cls(t.coverage_pct) << "\">"
         << detail::fmt_pct(t.coverage_pct) << "</span></td></tr>\n";
    }
    os << "</table>\n</div>\n";
  }

  if (opts.include_assertion_coverage && !data.assertions.empty()) {
    os << "<div class=\"card\">\n<h2>Assertion Coverage</h2>\n<table>\n"
       << "<tr><th>Scope</th><th>Name</th><th>Kind</th><th>Attempts</th><th>Pass</th><th>Fail</th><th>Vacuous</th><th>Disabled</th><th>Cover</th><th>Active</th><th>Peak Active</th><th>Status</th></tr>\n";
    for (size_t i = 0; i < data.assertions.size(); ++i) {
      const report_assertion_entry& a = data.assertions[i];
      const char* cls = (a.status == "FAIL") ? "badge-fail" : ((a.status == "PASS" || a.status == "COVERED") ? "badge-pass" : "badge-warn");
      os << "<tr><td>" << detail::escape_html(a.scope_path) << "</td>"
         << "<td>" << detail::escape_html(a.name) << "</td>"
         << "<td>" << detail::escape_html(a.assertion_kind) << "</td>"
         << "<td>" << a.attempt_count << "</td>"
         << "<td>" << a.pass_count << "</td>"
         << "<td>" << a.fail_count << "</td>"
         << "<td>" << a.vacuous_count << "</td>"
         << "<td>" << a.disabled_count << "</td>"
         << "<td>" << a.cover_count << "</td>"
         << "<td>" << a.active_count << "</td>"
         << "<td>" << a.peak_active_count << "</td>"
         << "<td><span class=\"badge " << cls << "\">" << detail::escape_html(a.status) << "</span></td></tr>\n";
    }
    os << "</table>\n</div>\n";
  }

  if (opts.include_other_ucis_metrics && !data.other_metrics.empty()) {
    os << "<div class=\"card\">\n<h2>Other UCIS Metrics</h2>\n<table>\n"
       << "<tr><th>Metric</th><th>Covered</th><th>Total</th><th>Coverage</th></tr>\n";
    for (size_t i = 0; i < data.other_metrics.size(); ++i) {
      const report_other_metric& m = data.other_metrics[i];
      os << "<tr><td>" << detail::escape_html(m.metric_name) << "</td>"
         << "<td>" << m.covered << "</td>"
         << "<td>" << m.total << "</td>"
         << "<td><span class=\"badge " << badge_cls(m.coverage_pct) << "\">"
         << detail::fmt_pct(m.coverage_pct) << "</span></td></tr>\n";
    }
    os << "</table>\n</div>\n";
  }

  if (opts.include_empty_scopes && !data.empty_scopes.empty()) {
    os << "<div class=\"card\">\n<h2>Empty Scopes</h2>\n<ul>\n";
    for (size_t i = 0; i < data.empty_scopes.size(); ++i) {
      os << "<li><code>" << detail::escape_html(data.empty_scopes[i]) << "</code></li>\n";
    }
    os << "</ul>\n</div>\n";
  }

  os << "</body>\n</html>\n";
}

inline void generate_markdown_report(
    std::ostream& os,
    fc4sc::global* cntxt = fc4sc::global::getter(),
    const report_options& opts = report_options())
{
  report_visitor v(opts);
  const report_data data = v.build_from_global(cntxt);
  render_markdown_report(os, data, opts);
}

inline std::string generate_markdown_report(
    fc4sc::global* cntxt = fc4sc::global::getter(),
    const report_options& opts = report_options())
{
  std::ostringstream oss;
  generate_markdown_report(oss, cntxt, opts);
  return oss.str();
}

inline void save_markdown_report(
    const std::string& out_path,
    fc4sc::global* cntxt = fc4sc::global::getter(),
    const report_options& opts = report_options())
{
  std::ofstream ofs(out_path.c_str());
  if (!ofs) {
    throw std::runtime_error("Could not open markdown report file [" + out_path + "]");
  }
  generate_markdown_report(ofs, cntxt, opts);
}

inline std::string generate_markdown_report_from_xml(
    const ucis_db::xml_node& root,
    const report_options& opts = report_options())
{
  std::ostringstream oss;
  render_markdown_report(oss, build_from_xml(root, opts), opts);
  return oss.str();
}

inline std::string generate_markdown_report_from_xml_file(
    const std::string& xml_path,
    const report_options& opts = report_options())
{
  std::ostringstream oss;
  render_markdown_report(oss, build_from_xml_file(xml_path, opts), opts);
  return oss.str();
}

inline void save_markdown_report_from_xml(
    const std::string& xml_path,
    const std::string& out_path,
    const report_options& opts = report_options())
{
  std::ofstream ofs(out_path.c_str());
  if (!ofs) {
    throw std::runtime_error("Could not open markdown report file [" + out_path + "]");
  }
  render_markdown_report(ofs, build_from_xml_file(xml_path, opts), opts);
}

inline void generate_html_report(
    std::ostream& os,
    fc4sc::global* cntxt = fc4sc::global::getter(),
    const report_options& opts = report_options())
{
  report_visitor v(opts);
  const report_data data = v.build_from_global(cntxt);
  render_html_report(os, data, opts);
}

inline std::string generate_html_report(
    fc4sc::global* cntxt = fc4sc::global::getter(),
    const report_options& opts = report_options())
{
  std::ostringstream oss;
  generate_html_report(oss, cntxt, opts);
  return oss.str();
}

inline void save_html_report(
    const std::string& out_path,
    fc4sc::global* cntxt = fc4sc::global::getter(),
    const report_options& opts = report_options())
{
  std::ofstream ofs(out_path.c_str());
  if (!ofs) {
    throw std::runtime_error("Could not open html report file [" + out_path + "]");
  }
  generate_html_report(ofs, cntxt, opts);
}

inline std::string generate_html_report_from_xml(
    const ucis_db::xml_node& root,
    const report_options& opts = report_options())
{
  std::ostringstream oss;
  render_html_report(oss, build_from_xml(root, opts), opts);
  return oss.str();
}

inline std::string generate_html_report_from_xml_file(
    const std::string& xml_path,
    const report_options& opts = report_options())
{
  std::ostringstream oss;
  render_html_report(oss, build_from_xml_file(xml_path, opts), opts);
  return oss.str();
}

inline void save_html_report_from_xml(
    const std::string& xml_path,
    const std::string& out_path,
    const report_options& opts = report_options())
{
  std::ofstream ofs(out_path.c_str());
  if (!ofs) {
    throw std::runtime_error("Could not open html report file [" + out_path + "]");
  }
  render_html_report(ofs, build_from_xml_file(xml_path, opts), opts);
}

} // namespace fc4sc

#endif /* FC4SC_REPORT_HPP */
