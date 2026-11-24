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

#include <limits>
#include <sstream>
#include <stdexcept>
#include <tuple>
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

  /*!
   *  \brief Create a dynamic copy of this cross with same type definition
   */
  cvp_base* create_instance(cvg_base* cvg_inst)
  {
    cross<Args...>* crs = new cross<Args...>;
    crs->total_coverpoints = this->total_coverpoints;
    crs->crs_data->name = this->crs_data->name;
    crs->crs_data->option = this->option();
    for (auto cvp : this->cvps_vec)
    {
      crs->cvps_vec.push_back(&(cvg_inst->get_coverpoint(cvp->name())));
      crs->crs_data->cross_cvps.push_back(cvg_inst->get_coverpoint(cvp->name()).get_data());
    }
    crs->crs_data->user_bins = this->crs_data->user_bins;
    crs->crs_data->auto_bins_size = this->crs_data->auto_bins_size;
    for (auto &bin : crs->crs_data->user_bins) {
      bin.hits = 0;
      if (bin.select)
        bin.select = bin.select->resolve(crs->cvps_vec, &this->cvps_vec);
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

  /*! Adds the user defined bins given to a constructor */
  void add_cross_bins() { }

  template <typename... Tail>
  void add_cross_bins(const cross_bin& bin, Tail... tail)
  {
    crs_data->user_bins.push_back(bin.data);
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
    for (auto &bin : user_bins)
      bin.size = 0;
    if (user_bins.empty()) return;

    for (auto &bin : user_bins)
      bin.select = bin.select->resolve(cvps_vec);

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

    std::vector<size_t> product(sizes.size(), 0);
    std::vector<bool> matched(user_bins.size());
    do {
      bool any = false;
      bool excluded = false;
      for (size_t i = 0; i < user_bins.size(); ++i)
      {
        matched[i] = user_bins[i].select->matches(product);
        any = any || matched[i];
        excluded = excluded || (matched[i] && user_bins[i].bin_type != default_);
      }
      // the products of ignore and illegal bins are in no other bin
      for (size_t i = 0; i < user_bins.size(); ++i)
        if (matched[i] && (user_bins[i].bin_type != default_ || !excluded))
          user_bins[i].size++;
      if (!any)
        crs_data->auto_bins_size++;
    } while (next_product(product, sizes));
  }

  /*!
   * \brief Calls build_cross_bins() eagerly when no crossed coverpoint has
   * deferred bins, or validates coverpoint membership and non-deferred binsof
   * conditions while deferring full resolution until ensure_bins().
   */
  void build_or_defer_cross_bins_or_free()
  {
    try {
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
  void sample_user_bins(const std::vector<std::vector<size_t>>& products)
  {
    auto& user_bins = crs_data->user_bins;
    std::vector<bool> hit(user_bins.size(), false);
    std::vector<const std::vector<size_t>*> auto_products;
    const std::vector<size_t>* illegal_product = nullptr;
    const cross_bin_data_model* illegal = nullptr;

    for (auto &product : products)
    {
      std::vector<size_t> matched;
      bool excluded = false;
      for (size_t i = 0; i < user_bins.size(); ++i)
      {
        if (!user_bins[i].select->matches(product)) continue;
        matched.push_back(i);
        excluded = excluded || (user_bins[i].bin_type != default_);
        if (user_bins[i].bin_type == illegal_ && illegal == nullptr) {
          illegal = &user_bins[i];
          illegal_product = &product;
        }
      }
      if (matched.empty())
        auto_products.push_back(&product);
      for (auto i : matched)
        if (user_bins[i].bin_type != default_ || !excluded)
          hit[i] = true;
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
      counted = counted || (user_bins[i].bin_type == default_);
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
    for(auto cvp_it : cvps_vec) {
      crs_data->cross_cvps.push_back(cvp_it->get_data());
    }
    add_cross_bins(binsofs...);
    build_or_defer_cross_bins_or_free();

    // registered once it is built: a cross which throws is not sampled
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
    if (!this->collect) return;
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
      sample_user_bins(products);
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
  void start()
  {
    collect = true;
  };

  /*!
   * \brief Disables sampling
   */
  void stop()
  {
    collect = false;
  };


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
