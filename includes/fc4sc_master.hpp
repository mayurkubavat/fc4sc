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
                     AMIQ Consulting s.r.l. (contributors@@amiq.com)

               Date: 2018-Feb-20
******************************************************************************/

/*!
 \file fc_master.hpp
 \brief Global overseer

   This file contains a global table of covergroups and a "static" wrapper class
 for it. All type specific computation and report writing is done from here.
 */

#ifndef FC4SC_MASTER_HPP
#define FC4SC_MASTER_HPP

#include <unordered_map>
#include <fstream>
#include <stdint.h>
#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <ctime>

#include "fc4sc_base.hpp"

namespace fc4sc
{

/*!
 * \class global fc_master.hpp
 * \brief Static proxy to a \link fc4sc::main_controller \endlink instance
 */
class global
{

class default_scope_data_model : public scp_base_data_model 
{ 
public:

  int anonymous_count = 0;
  
  void add_cvg_data(cvg_base_data_model* cvg_data,std::string cvg_type_name, std::string scp_type_name, unsigned int cvg_file_id, uint32_t cvg_line)
  {
    cvg_insts[cvg_data->name] = cvg_data;
    cvgs[scp_type_name + "::" + cvg_type_name].push_back(cvg_data);

    cvg_data->parent_scp = this;

    scp_metadata* type_data;
    if(fc4sc::global::get_scopes_data(this->cntxt).count(scp_type_name) > 0) {
      type_data = fc4sc::global::get_scopes_data(this->cntxt)[scp_type_name];
    }
    else {
      type_data = new scp_metadata;
      type_data->type_name = scp_type_name;
      fc4sc::global::get_scopes_data(this->cntxt)[scp_type_name] = type_data;
    }
    
    if(type_data->cvg_type_table.count(cvg_type_name) > 0)
    {
      type_data->cvg_type_table[cvg_type_name]->cvg_insts.push_back(cvg_data);
    }
    else 
    {
      cvg_metadata* tmp = new cvg_metadata;
      tmp->type_name = cvg_type_name;
      tmp->scp_type_name = scp_type_name;
      tmp->file_id = cvg_file_id;
      tmp->line = cvg_line;
      tmp->cvg_insts.push_back(cvg_data);
      type_data->cvg_type_table[cvg_type_name] = tmp;
    }
    cvg_data->type_data = type_data->cvg_type_table[cvg_type_name];
  }

  void add_scp_data(scp_base_data_model* scp_data, std::string scp_type_name, unsigned int cvg_file_id, uint32_t cvg_line)
  {
    std::cerr << "Cannot register sub scopes to default scope\n";
    throw("Cannot register sub scopes to default scope\n");
  }

};


/*!
 *  \class default_scope fc_master.hpp
 *  \brief default scope for covergroups without user-defined scope
 *
 *  Special default scope for covergroups that have not
 *  been assigned to a user-defined scope
 */
class default_scope : public scp_base
{
  friend class global;

  default_scope(global* cntxt) : scp_data(new default_scope_data_model)
  {
    scp_data->cntxt = cntxt;
    scp_data->name = _FC4SC_DEFAULT_SCOPE_NAME_;
    scp_data->inst_file_id = fc4sc::global::get_file_id(__FILE__,scp_data->cntxt);
    scp_data->inst_line = __LINE__;
    scp_data->instance_id = fc4sc::global::create_instance_id(scp_data->cntxt);
    fc4sc::global::register_data(this->scp_data,_FC4SC_DEFAULT_SCOPE_TYPE_,fc4sc::global::get_file_id(__FILE__,scp_data->cntxt),__LINE__,scp_data->cntxt);
  }

public:

  scp_base_data_model* scp_data;

  /*! Destructor */
  virtual ~default_scope() { }
 
  scp_base_data_model* get_scp_data()
  {
    return scp_data;
  }
  
  std::string& name()
  {
    return scp_data->name;
  }

  std::string type_name()
  {
    return scp_data->type_data->type_name;
  }

  unsigned int& file_id()
  {
    return scp_data->type_data->file_id;
  }

  uint32_t& line()
  {
    return scp_data->type_data->line;
  }

 unsigned int& inst_file_id()
 {
   return scp_data->inst_file_id;
 }

 uint32_t& inst_line()
 {
   return scp_data->inst_line;
 }

 unsigned int& instance_id()
 {
   return scp_data->instance_id;
 }
};

public:

  /*!
   * \class main_controller fc_master.hpp
   * \brief Keeps evidence of all the covergroup instances and types
   */
  //class main_controller
  //{
  //  friend class global;

    default_scope* dflt_scp = nullptr;

    std::unordered_map<std::string,scp_metadata*> scps_data;

    std::vector<scp_base_data_model*> top_scps;

    /*! Table keeping file IDs corresponding to file names */
    std::unordered_map<unsigned int, std::string> file_id_to_name;

    /*! Table keeping file names cooresponding to file IDs */
    std::unordered_map<std::string, unsigned int> file_name_to_id;

    /*! key generation for file IDs*/
    unsigned int gkey = 1;

    /*! Name of the coverage database file, see set_coverage_db_name() */
    std::string coverage_db_name;

    /*!
     * \brief gets file_id_to_name table
    */
    const std::unordered_map<unsigned int,std::string>& internal_get_file_id_to_name_table()
    {
      return file_id_to_name;
    }

    /*!
     * \brief gets file name associated with file_id
    */
    std::string internal_get_file_id_to_name(unsigned int id)
    {
      return file_id_to_name[id];
    }

    unsigned int internal_create_instance_id() {
      return gkey++;
    }

    /*!
     * \brief gets unique id for file names
    */
    unsigned int internal_get_file_id(const std::string& file_name)
    {
      if (file_name_to_id.count(file_name) == 0) {
	unsigned int id = gkey++;
	file_id_to_name[id] = file_name;
	file_name_to_id[file_name] = id;
      }
      return file_name_to_id[file_name];
    }

    /*!
     * \brief Adds a new data to scope data
     * \param scope data pointer
    */
    void internal_register_data(scp_base_data_model* scp_data, std::string scp_type_name, unsigned int scp_file_id, uint32_t scp_line)
    {
      if (scp_data->name.empty())
      {
        std::size_t found = scp_type_name.find_last_of(':');
	if(found == std::string::npos) {
	  scp_data->name = scp_type_name + "_";
	}
	else {
          scp_data->name = scp_type_name.substr(found+1);
	}
      }
      top_scps.push_back(scp_data);
      if(scps_data.count(scp_type_name) > 0)
      {
        scps_data[scp_type_name]->scp_insts.push_back(scp_data);
      }
      else
      {
        scp_metadata* tmp = new scp_metadata;
	tmp->type_name = scp_type_name;
	tmp->file_id = scp_file_id;
	tmp->line = scp_line;
	tmp->scp_insts.push_back(scp_data);
	scps_data[scp_type_name] = tmp;
      }
      scp_data->type_data = scps_data[scp_type_name];
    }

   /*!
    * \brief gets scope data to introspect the coverage data
    */ 
    std::unordered_map<std::string,scp_metadata*>& internal_get_scopes_data() 
    {
      return scps_data;
    }

   /*!
    * \brief gets top scopes to introspect the coverage data
    */ 
    std::vector<scp_base_data_model*>& internal_get_top_scopes() 
    {
      return top_scps;
    }

   /*!
    * \brief Computes the coverage percentage across all instances of all types
    * \returns Double between 0 and 100
    */
    double internal_get_coverage()
    {
      double res = 0;
      double weights = 0;

      for (auto &scp_types : scps_data) {
        for (auto &types : scp_types.second->cvg_type_table) {
          res += internal_get_coverage(scp_types.first,types.first) * internal_type_option(scp_types.first,types.first).weight;
          weights += internal_type_option(scp_types.first,types.first).weight;
        }
      }

      if (scps_data.size() == 0) return 100;
      if (weights == 0)        return 0;
      return res / weights;
    }

   /*!
    * \brief Computes the coverage percentage across all instances of a given type
    * \param type Unmangled type name
    * \returns Double between 0 and 100
    */
    double internal_get_coverage(const std::string &scp_type, const std::string &type)
    {
      general_coverage data_visitor;
      return data_visitor.get_coverage(scp_type,type,this);
    }

    /*!
     * \brief Computes the coverage percentage across all instances of a given type
     * \param type Unmangled type name
     * \param hit_bins Total number of hit bins in instances (in the merged
     * instances if type_option.merge_instances is set)
     * \param total_bins Total number of bins in instances (in the merged
     * instances if type_option.merge_instances is set)
     * \returns Double between 0 and 100
     */
    double internal_get_coverage(const std::string &scp_type, const std::string &type, int &hit_bins, int &total_bins)
    {
      general_coverage data_visitor;
      return data_visitor.get_coverage(scp_type,type,hit_bins,total_bins,this);
    }

    cvg_type_option &internal_type_option(const std::string &scp_type, const std::string &type)
    {
      return scps_data[scp_type]->cvg_type_table[type]->type_option;
    }
    
    bool empty() const {
      return scps_data.empty();
    }

    scp_base* internal_get_default_scope()
    {
      if(dflt_scp == nullptr) {
        dflt_scp = new default_scope(this);
      }
      return dflt_scp;
    }

  //};

private:

/*!
 * \class merged_coverage fc_master.hpp
 * \brief Computes the coverage of a covergroup type with merged instances
 *
 * Used when type_option.merge_instances is set. The bins of all enabled
 * instances are merged: bins with the same name in a coverpoint are the
 * same bin, their hits are added up, and the type coverage is computed on
 * the merged bins as if they were the bins of a single instance. Cross bins
 * are merged in the same way, by the names of the bins they are made of.
 * The at_least, goal and weight options of a merged coverpoint or cross are
 * those of the first enabled instance which has it. With the
 * strict_sv_options type option, the weight and goal are those of the
 * type_option of the coverpoint or cross, as in SystemVerilog.
 */
class merged_coverage : public covVisitorBase
{
  /*! Key of a bin: its name and its occurrence among the bins of that name */
  typedef std::pair<std::string, size_t> bin_key_t;

  /*! Merged data of a coverpoint or a cross */
  struct merged_cvp
  {
    bool is_cross = false;
    uint64_t weight = 1;
    uint64_t goal = 100;
    uint64_t at_least = 1;

    /*! Coverpoint: hits of each merged bin */
    std::map<bin_key_t, uint64_t> bin_hits;

    /*! Cross: bins of each crossed coverpoint */
    std::vector<std::map<bin_key_t, bool>> cross_dims;

    /*! Cross: hits of each merged cross bin */
    std::map<std::vector<bin_key_t>, uint64_t> cross_hits;

    /*!
     * Cross with user defined bins: the number of automatically created
     * bins, and the hits of each user defined bin counted in the coverage,
     * by name. The instances of a covergroup type have the same bins.
     */
    bool has_user_bins = false;
    uint64_t auto_bins_size = 0;
    std::map<std::string, uint64_t> user_hits;
  };

  /*! Merged coverpoints and crosses, in the order they are first seen */
  std::vector<std::string> cvp_order;
  std::map<std::string, merged_cvp> cvps;

  /*! Returns the keys of the bins of a coverpoint, in the order of its bins */
  static std::vector<bin_key_t> bin_keys(const cvp_base_data_model* cvp)
  {
    std::vector<bin_key_t> keys;
    auto data = dynamic_cast<const coverpoint_base_data_model*>(cvp);
    if (data == nullptr) {
      // not a coverpoint: the index of the bin is the only thing known
      for (uint64_t i = 0; i < cvp->size(); ++i)
        keys.push_back(bin_key_t(std::to_string(i), 0));
      return keys;
    }
    std::map<std::string, size_t> seen;
    for (auto bin : data->bins_data)
    {
      const std::string& name = bin->get_name();
      keys.push_back(bin_key_t(name, seen[name]++));
    }
    return keys;
  }

  /*! Sets the options of a merged coverpoint or cross from its first instance */
  template <typename O>
  static void set_options(merged_cvp& merged, cvp_base_data_model& base, const O& option)
  {
    merged.at_least = option.at_least;
    if (base.parent_cvg != nullptr && strict_sv_options(base.parent_cvg->type_data)) {
      merged.weight = base.type_option().weight;
      merged.goal = base.type_option().goal;
    }
    else {
      merged.weight = option.weight;
      merged.goal = option.goal;
    }
  }

  merged_cvp& get_merged(const std::string& name, bool is_cross)
  {
    if (cvps.count(name) == 0) {
      cvp_order.push_back(name);
      cvps[name].is_cross = is_cross;
    }
    return cvps[name];
  }

public:

  int bin_total = 0;
  int bin_covered = 0;

  void visit(scp_base_data_model&) { }

  void visit(cvg_base_data_model& base)
  {
    base.sync_options();
    if (!base.enable) return;
    for (auto &cvp : base.cvps)
      cvp->accept_visitor(*this);
  }

  void visit(coverpoint_base_data_model& base)
  {
    base.sync_options_from_parent();
    bool first = (cvps.count(base.name) == 0);
    merged_cvp& merged = get_merged(base.name, false);
    if (first)
      set_options(merged, base, base.option);

    std::vector<bin_key_t> keys = bin_keys(&base);
    for (size_t i = 0; i < keys.size(); ++i)
    {
      uint64_t hits = 0;
      for (auto hitcount : base.bins_data[i]->get_interval_hits())
        hits += hitcount;
      merged.bin_hits[keys[i]] += hits;
    }
  }

  void visit(cross_base_data_model& base)
  {
    base.sync_options_from_parent();
    bool first = (cvps.count(base.name) == 0);
    merged_cvp& merged = get_merged(base.name, true);
    if (first) {
      set_options(merged, base, base.option);
      merged.has_user_bins = base.has_custom_auto_bins();
      merged.auto_bins_size = base.auto_bins_size;
    }

    for (auto &bin : base.user_bins)
      if (bin.bin_type == default_ && bin.size > 0)
        merged.user_hits[bin.name] += bin.hits;

    std::vector<std::vector<bin_key_t>> keys;
    for (auto cvp : base.cross_cvps)
      keys.push_back(bin_keys(cvp));

    if (merged.cross_dims.size() < keys.size())
      merged.cross_dims.resize(keys.size());
    for (size_t dim = 0; dim < keys.size(); ++dim)
      for (auto &key : keys[dim])
        merged.cross_dims[dim][key] = true;

    for (auto &bin : base.get_cross_bins())
    {
      std::vector<bin_key_t> cross_key;
      for (size_t dim = 0; dim < bin.first.size() && dim < keys.size(); ++dim)
        cross_key.push_back(keys[dim].at(bin.first[dim]));
      merged.cross_hits[cross_key] += bin.second;
    }
  }

  void visit(bin_base_data_model&) { }

  /*!
   * \brief Computes the coverage of a merged coverpoint or cross
   * \param merged The merged coverpoint or cross
   * \param covered Number of covered merged bins
   * \param total Total number of merged bins
   * \returns Double between 0 and 100
   */
  static double get_merged_coverage(const merged_cvp& merged, int &covered, int &total)
  {
    covered = 0;
    if (merged.is_cross) {
      total = merged.cross_dims.empty() ? 0 : 1;
      for (auto &dim : merged.cross_dims)
        total *= dim.size();
      if (merged.has_user_bins)
        total = merged.auto_bins_size + merged.user_hits.size();
      for (auto &bin : merged.cross_hits)
        covered += (bin.second >= merged.at_least);
      for (auto &bin : merged.user_hits)
        covered += (bin.second >= merged.at_least);
    }
    else {
      total = merged.bin_hits.size();
      for (auto &bin : merged.bin_hits)
        covered += (bin.second >= merged.at_least);
    }

    if (total == 0)
      return (merged.weight == 0) ? 100 : 0;

    double real = 100.0 * covered / total;
    return (real >= merged.goal) ? 100 : real;
  }

  /*!
   * \brief Computes the type coverage of one coverpoint or cross on the
   * merged instances of a covergroup type
   * \param type_data The covergroup type
   * \param name Name of the coverpoint or cross
   * \param covered Number of covered merged bins
   * \param total Total number of merged bins
   * \returns Double between 0 and 100; 100 if no instance is enabled
   */
  double get_cvp_coverage(cvg_metadata& type_data, const std::string& name, int &covered, int &total)
  {
    covered = 0;
    total = 0;
    for (auto cvg : type_data.cvg_insts)
      cvg->accept_visitor(*this);
    if (cvps.count(name) == 0)
      return 100;
    return get_merged_coverage(cvps[name], covered, total);
  }

  /*!
   * \brief Computes the merged coverage of all instances of a covergroup type
   * \param type_data The covergroup type
   * \returns Double between 0 and 100
   */
  double get_coverage(cvg_metadata& type_data)
  {
    bool enabled = false;
    for (auto cvg : type_data.cvg_insts) {
      enabled = enabled || cvg->enable;
      cvg->accept_visitor(*this);
    }

    // same results as the weighted average for a type without (enabled) instances
    if (type_data.cvg_insts.empty())
      return (type_data.type_option.weight == 0) ? 100 : 0;
    if (!enabled)
      return 100;

    double res = 0;
    double weights = 0;

    for (auto &name : cvp_order)
    {
      merged_cvp& merged = cvps[name];
      int covered = 0;
      int total = 0;
      double cvp_res = get_merged_coverage(merged, covered, total);

      this->bin_total += total;
      this->bin_covered += covered;

      res += cvp_res * merged.weight;
      weights += merged.weight;
    }

    if (weights == 0 || cvp_order.empty() || res == 0)
      return (type_data.type_option.weight == 0) ? 100 : 0;

    double real = res / weights;
    return (real >= type_data.type_option.goal) ? 100 : real;
  }

};

class general_coverage  : public covVisitorBase
{
public:

  double cvg_res = 0;
  double cvg_weight = 0;

  double cvp_res = 0;
  double cvp_weight = 0;

  uint64_t hitsum = 0;

  int bin_total = 0;
  int bin_covered = 0;

  void visit(scp_base_data_model& base){ }

  void visit(cvg_base_data_model& base)
  {
    base.sync_options();
    cvg_weight = base.option.weight;
    if(base.enable) {
      cvg_res = 0;
      double weights = 0;

      for (auto &cvp : base.cvps) {
        cvp->accept_visitor(*this);
        cvg_res += cvp_res * cvp_weight;
        weights += cvp_weight;
      }

      if (weights == 0 || base.cvps.size() == 0 || cvg_res == 0) {
        cvg_res = (base.option.weight == 0) ? 100 : 0;
	return;
      }

      double real = cvg_res / weights;
      cvg_res = (real >= base.option.goal) ? 100 : real;
    }
    else {
      cvg_res = 100;
    }
  }

  void visit(coverpoint_base_data_model& base)
  {
    base.sync_options_from_parent();
    cvp_weight = base.option.weight;
    this->bin_total += base.bins_data.size();
    if (base.bins_data.empty()) {
      cvp_res = (base.option.weight == 0) ? 100 : 0;
      return;
    }

    double res = 0;
    for (auto &bin : base.bins_data) {
      bin->accept_visitor(*this);
      res += (hitsum >= base.option.at_least);
    }

    this->bin_covered += res;

    double real = res * 100 / base.bins_data.size();

    cvp_res = (real >= base.option.goal) ? 100 : real;
  }

  void visit(cross_base_data_model& base)
  {
    base.sync_options_from_parent();
    cvp_weight = base.option.weight;
    
    int covered = 0;
    int total = 0;
    base.get_coverage_bins(covered, total);

    this->bin_total += total;

    if (total == 0) {
      cvp_res = (base.option.weight == 0) ? 100 : 0;
      return;
    }

    this->bin_covered += covered;

    double real = 100.0 * covered / total;
    cvp_res = (real >= base.option.goal) ? 100 : real;
  }

  void visit(bin_base_data_model& base)
  {
    hitsum = 0;
    for (auto hitcount : base.get_interval_hits())
      hitsum += hitcount;
  }

  double get_coverage(const std::string &scp_type, const std::string &type, fc4sc::global* cvg_cntxt)
  {
    this->bin_total = 0;
    this->bin_covered = 0;

    cvg_metadata* type_data = fc4sc::global::get_scopes_data(cvg_cntxt)[scp_type]->cvg_type_table[type];
    if (type_data->type_option.merge_instances) {
      merged_coverage merged;
      double ret = merged.get_coverage(*type_data);
      this->bin_total = merged.bin_total;
      this->bin_covered = merged.bin_covered;
      return ret;
    }

    std::vector<cvg_base_data_model*> cvgs = type_data->cvg_insts;

    double res = 0;
    double weights = 0;

    for (auto it : cvgs)
    {
      it->accept_visitor(*this);
      res += this->cvg_res * this->cvg_weight;
      weights += this->cvg_weight;
    }

    if (weights == 0 || cvgs.size() == 0 || res == 0)
      return (fc4sc::global::get_scopes_data(cvg_cntxt)[scp_type]->cvg_type_table[type]->type_option.weight == 0) ? 100 : 0;

    double real = res / weights;
    return (real >= fc4sc::global::get_scopes_data(cvg_cntxt)[scp_type]->cvg_type_table[type]->type_option.goal) ? 100 : real;
  }

    double get_coverage(const std::string &scp_type, const std::string &type, int &hit_bins, int &total_bins, fc4sc::global* cvg_cntxt)
    {
      double ret = get_coverage(scp_type, type, cvg_cntxt);
      hit_bins = this->bin_covered;
      total_bins = this->bin_total;
      return ret;
    }

};

public:

  /*!
   * \brief Manages the \link fc4sc::main_controller \endlink global instance
   */
  static global *getter()
  {
    static global *cntxt = new global;
    return cntxt;
  }

  static global *create_new_context()
  {
    return new global;
  }

  /*!
   * \brief Sets the name of the file which xml_printer::coverage_save()
   * writes when it is given no file name, as the SystemVerilog
   * $set_coverage_db_name
   */
  static void set_coverage_db_name(const std::string& file_name, fc4sc::global* cvg_cntxt = fc4sc::global::getter())
  {
    cvg_cntxt->coverage_db_name = file_name;
  }

  /*!
   * \brief Returns the name set by set_coverage_db_name(), or an empty string
   */
  static const std::string& get_coverage_db_name(fc4sc::global* cvg_cntxt = fc4sc::global::getter())
  {
    return cvg_cntxt->coverage_db_name;
  }

  static void delete_context(global* cntxt)
  {
    delete cntxt;
  }

  static scp_base* get_default_scope(fc4sc::global* cvg_cntxt = fc4sc::global::getter())
  {
    return cvg_cntxt->internal_get_default_scope();
  }

  /*!
   * \brief creates unique id for scopes
  */
  static unsigned int create_instance_id(fc4sc::global* cvg_cntxt = fc4sc::global::getter())
  {
    return cvg_cntxt->internal_create_instance_id();
  }

  /*!
   * \brief gets unique id for file names
  */
  static unsigned int get_file_id(std::string file_name,fc4sc::global* cvg_cntxt = fc4sc::global::getter())
  {
    return cvg_cntxt->internal_get_file_id(file_name);
  } 

  /*!
   * \brief get file ID to file name table
   */
  static const std::unordered_map<unsigned int,std::string>& get_file_id_to_name_table(fc4sc::global* cvg_cntxt = fc4sc::global::getter())
  {
    return cvg_cntxt->internal_get_file_id_to_name_table();
  }

  /*!
   * \brief Add scope data to global vector
   * \param scope data pointer
  */
  static void register_data(scp_base_data_model* scp_data, std::string scp_type_name, unsigned int scp_file_id, uint32_t scp_line, fc4sc::global* cvg_cntxt = fc4sc::global::getter())
  {
    cvg_cntxt->internal_register_data(scp_data,scp_type_name,scp_file_id,scp_line);
  }

  static cvg_type_option &type_option(const std::string &scp_type, const std::string &type, fc4sc::global* cvg_cntxt = fc4sc::global::getter())
  {
    return cvg_cntxt->internal_type_option(scp_type,type);
  }

  /*!
   * \brief get data model of scopes
  */
  static std::unordered_map<std::string,scp_metadata*>& get_scopes_data(fc4sc::global* cvg_cntxt = fc4sc::global::getter())
  {
    return cvg_cntxt->internal_get_scopes_data();
  }

  /*!
   * \brief get list of top scopes data model
  */
  static std::vector<scp_base_data_model*>& get_top_scopes(fc4sc::global* cvg_cntxt = fc4sc::global::getter())
  {
    return cvg_cntxt->internal_get_top_scopes();
  }

  /*!
  * \brief Computes the coverage percentage across all instances of all types
  * \returns Double between 0 and 100
  */
  static double get_coverage(fc4sc::global* cvg_cntxt = fc4sc::global::getter())
  {
    return cvg_cntxt->internal_get_coverage();
  }

  /*!
   * \brief Computes the coverage percentage across all instances of a given type
   * \param type Unmangled type name
   * \returns Double between 0 and 100
   */
  static double get_coverage(const std::string &scp_type, const std::string &type, fc4sc::global* cvg_cntxt = fc4sc::global::getter())
  {
    return cvg_cntxt->internal_get_coverage(scp_type,type);
  }

  /*!
   * \brief Computes the coverage percentage across all instances of a given type
   * \param type Unmangled type name
   * \param hit_bins Total number of hit bins across instances of same type
   * \param total_bins Total number of bins across instances of same type
   * \returns Double between 0 and 100
   */
  static double get_coverage(const std::string &scp_type, const std::string &type, int &hit_bins, int &total_bins, fc4sc::global* cvg_cntxt = fc4sc::global::getter())
  {
    return cvg_cntxt->internal_get_coverage(scp_type, type, hit_bins, total_bins);
  }

  /*!
   * \brief Computes the type coverage of a coverpoint or cross
   *
   * If type_option.merge_instances is set, the type coverage of a
   * coverpoint or cross is computed on its merged bins, as for the type
   * coverage of the covergroup. Otherwise it is the weighted average of
   * its coverage in each enabled instance of the covergroup type it is in,
   * with the weights of these covergroup instances, as for the type
   * coverage of the covergroup. It is 100 if no instance is enabled or all
   * the weights are 0. A coverpoint or cross which is in no covergroup is
   * its only instance.
   *   With the strict_sv_options type option, the weight of each instance
   * is the option.weight of the coverpoint or cross in that instance, as in
   * SystemVerilog, and the coverage is 100 when it reaches the goal of the
   * type_option of the coverpoint or cross.
   * \param cvp Data of the coverpoint or cross
   * \param covered Number of covered bins, summed over the instances
   * \param total Total number of bins, summed over the instances
   * \returns Double between 0 and 100
   */
  static double get_cvp_coverage(cvp_base_data_model* cvp, int &covered, int &total)
  {
    covered = 0;
    total = 0;

    std::vector<cvp_base_data_model*> insts;
    std::vector<double> weights;
    cvg_base_data_model* cvg = cvp->parent_cvg;
    const bool strict = (cvg != nullptr && strict_sv_options(cvg->type_data));

    if (cvg == nullptr || cvg->type_data == nullptr) {
      insts.push_back(cvp);
      weights.push_back(1);
    }
    else {
      if (cvg->type_data->type_option.merge_instances) {
        merged_coverage merged;
        return merged.get_cvp_coverage(*cvg->type_data, cvp->name, covered, total);
      }

      // all the instances of a covergroup type have the same coverpoints
      // and crosses, in the same order
      size_t index = std::find(cvg->cvps.begin(), cvg->cvps.end(), cvp) - cvg->cvps.begin();
      for (auto inst : cvg->type_data->cvg_insts)
      {
        if (!inst->enable || index >= inst->cvps.size() || inst->cvps[index]->name != cvp->name)
          continue;
        insts.push_back(inst->cvps[index]);
        weights.push_back(inst->option.weight);
      }
    }

    double res = 0;
    double weight_sum = 0;
    for (size_t i = 0; i < insts.size(); ++i)
    {
      general_coverage data_visitor;
      insts[i]->accept_visitor(data_visitor);
      // the option.weight of the coverpoint or cross in this instance
      const double weight = strict ? data_visitor.cvp_weight : weights[i];
      covered += data_visitor.bin_covered;
      total += data_visitor.bin_total;
      res += data_visitor.cvp_res * weight;
      weight_sum += weight;
    }

    if (weight_sum == 0)
      return 100;
    const double real = res / weight_sum;
    if (strict && real >= cvp->type_option().goal)
      return 100;
    return real;
  }

  static bool is_empty(fc4sc::global* cvg_cntxt = fc4sc::global::getter())
  {
    return cvg_cntxt->empty();
  }

  virtual ~global()
  {
    for (auto scp_it : scps_data) {
      delete scp_it.second;
    }
  }

};

} // namespace fc4sc

#endif /* FC4SC_MASTER_HPP */
