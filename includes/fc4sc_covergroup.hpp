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
 \file covergroup.hpp
 \brief Covergroup implementation

   This file contains the covergroup implementation
 with all the non-user functions
 */

#ifndef FC4SC_COVERGROUP_HPP
#define FC4SC_COVERGROUP_HPP

#include "fc4sc_base.hpp"

#include <unordered_map>
#include <typeinfo>
#include <tuple>

namespace fc4sc
{

class dynamic_covergroup;
class dynamic_covergroup_factory;

class covergroup_data_model : public cvg_base_data_model
{
public:
};

/*!
 * \class covergroup covergroup.hpp
 * \brief Covergroup option declaration
 */
class covergroup : public cvg_base
{

  friend class dynamic_covergroup_factory;
  template<typename T,typename U>
  friend class dynamic_coverpoint_factory;

  /*! Pointer to this object's data */
  covergroup_data_model* cvg_data = new covergroup_data_model;

  /*! For ensuring the covergroup's data has not been deallocated */
  std::weak_ptr<bool> valid_data = cvg_data->valid;

protected:
  /*
   * This function registers a coverpoint instance inside this covergroup.
   * It receives as arguments a pointer to the coverpoint to be registered,
   * the coverpoint name, the sample expression lambda function and string,
   * and finally, the sample condition lambda function and string.
   * Finally, it returns a coverpoint constructed with the given arguments.
   * The purpose of this function is to be used for coverpoint instantiation
   * via the COVERPOINT macro and should not be explicitly used!
   */
  template<typename T>
  coverpoint <T> register_cvp(coverpoint <T>* cvp, std::string&& cvp_name,
    std::function<T()>&& sample_expr, std::string&& sample_expr_str,
    std::function<bool()>&& sample_cond, std::string&& sample_cond_str) {

    // NOTE: VERY important! Do not attempt to dereference the cvp pointer in
    // any way because it points to uninitialized memory!
    cvg_base::register_cvp(cvp);
    coverpoint<T> cvp_structure;
    cvp_structure.has_sample_expression = true;
    cvp_structure.sample_expression = sample_expr;
    cvp_structure.sample_condition = sample_cond;
    cvp_structure.cvp_data->sample_expression_str = sample_expr_str;
    cvp_structure.cvp_data->sample_condition_str = sample_cond_str;
    cvp_structure.name() = cvp_name;
    cvg_data->add_cvp_data(cvp_structure.cvp_data);
    return cvp_structure;
  }

  std::unordered_map<cvp_base *, cvp_metadata_t> cvp_strings;

  /*!
   * \brief Registers an instance WITH NO SCOPE and some info to \link fc4sc::global_access \endlink
   * \param type_name Type of covergroup stringified
   * \param file_name File of declaration
   * \param line  Line of declaration
   * \param inst_name Name of the instance
   */
  covergroup(const char *type_name, const char *file_name = "", int line = 1, const std::string &inst_name = "", const char *inst_file_name = "", int inst_line = 1, fc4sc::global* cntxt = fc4sc::global::getter())
  {
    const char* dinfo_scp = nullptr;
    auto& dinfo = fc4sc::detail::current_derived_type_info();
    if (dinfo.active && dinfo.type_name != nullptr) {
      type_name = dinfo.type_name;
      file_name = dinfo.file_name;
      line = dinfo.line;
      dinfo_scp = dinfo.scp_type_name;
      dinfo.active = false;
    }
    cvg_data->name = inst_name;
    cvg_data->inst_file_id = fc4sc::global::get_file_id(inst_file_name,cntxt);
    cvg_data->inst_line = inst_line;
    fc4sc::global::get_default_scope(cntxt)->get_scp_data()->name_check(cvg_data->name,type_name);
    fc4sc::global::get_default_scope(cntxt)->get_scp_data()->add_cvg_data(this->cvg_data,type_name,(dinfo_scp != nullptr) ? dinfo_scp : _FC4SC_DEFAULT_SCOPE_TYPE_,fc4sc::global::get_file_id(file_name,cntxt),line);
    fc4sc::global::get_default_scope(cntxt)->register_cvg(this);
  }

  /*!
   * \brief Registers an instance WITH NO SCOPE and some info to \link fc4sc::global_access \endlink
   * \param type_name Type of covergroup stringified
   * \param file_name File of declaration
   * \param line  Line of declaration
   * \param inst_name Name of the instance
   */
  covergroup(const char *type_name, std::string scp_type_name, const char *file_name = "", int line = 1, const std::string &inst_name = "", const char *inst_file_name = "", int inst_line = 1, fc4sc::global* cntxt = fc4sc::global::getter())
  {
    auto& dinfo = fc4sc::detail::current_derived_type_info();
    if (dinfo.active && dinfo.type_name != nullptr) {
      type_name = dinfo.type_name;
      file_name = dinfo.file_name;
      line = dinfo.line;
      if (dinfo.scp_type_name != nullptr)
        scp_type_name = dinfo.scp_type_name;
      dinfo.active = false;
    }
    cvg_data->name = inst_name;
    cvg_data->inst_file_id = fc4sc::global::get_file_id(inst_file_name,cntxt);
    cvg_data->inst_line = inst_line;
    fc4sc::global::get_default_scope(cntxt)->get_scp_data()->name_check(cvg_data->name,type_name);
    fc4sc::global::get_default_scope(cntxt)->get_scp_data()->add_cvg_data(this->cvg_data,type_name,scp_type_name,fc4sc::global::get_file_id(file_name,cntxt),line);
    fc4sc::global::get_default_scope(cntxt)->register_cvg(this);
  }

   /*!
   * \brief Registers an instance and some info to \link fc4sc::global_access \endlink
   * \param mod Reference to parent scope instance
   * \param type_name Type of covergroup stringified
   * \param scp_type_name Stringified type of the scope in which the covergroup is defined
   * \param file_name File of declaration
   * \param line  Line of declaration
   * \param inst_name Name of the instance
   */
  covergroup(scp_base& mod, const char *type_name, std::string scp_type_name, const char *file_name = "", int line = 1, const std::string &inst_name = "", const char *inst_file_name = "", int inst_line = 1)
  {
    auto& dinfo = fc4sc::detail::current_derived_type_info();
    if (dinfo.active && dinfo.type_name != nullptr) {
      type_name = dinfo.type_name;
      file_name = dinfo.file_name;
      line = dinfo.line;
      if (dinfo.scp_type_name != nullptr)
        scp_type_name = dinfo.scp_type_name;
      dinfo.active = false;
    }
    cvg_data->name = inst_name;
    cvg_data->inst_file_id = fc4sc::global::get_file_id(inst_file_name,mod.get_scp_data()->cntxt);
    cvg_data->inst_line = inst_line;
    mod.get_scp_data()->name_check(cvg_data->name,type_name);
    mod.get_scp_data()->add_cvg_data(this->cvg_data,type_name,scp_type_name,fc4sc::global::get_file_id(file_name,mod.get_scp_data()->cntxt),line);
    mod.register_cvg(this);
  }

  bool add_iff_init(std::function<bool()> pred, const char* expr_str)
  {
    add_iff(std::move(pred), expr_str != nullptr ? std::string(expr_str) : std::string());
    return true;
  }

  uint32_t set_strings(cvp_base *cvp, void *sample_point, const std::string& cvp_name, const std::string& expr_name) {
    cvp_strings[cvp] = cvp_metadata_t(sample_point, cvp_name, expr_name);
    return 0;
  }

  virtual cvp_metadata_t get_strings(cvp_base *cvp) {
    return cvp_strings[cvp];
  }

  /*! Re-enabled default constructor for dynamic_covergroup_factory */
  //covergroup() = delete;
  /*! Disabled */
  covergroup &operator=(const covergroup &other) = delete;
  /*! Disabled */
  covergroup(const covergroup &other) = delete;
  /*! Disabled */
  covergroup(covergroup &&other) = delete;
  // ! Disabled
  covergroup &operator=(covergroup &&other) = delete;

  /*! Destructor */
  virtual ~covergroup() {
    if (detail::strobe_backend* backend = detail::active_strobe_backend()) {
      backend->cancel(this);
    }
  }

public:

  void sample_immediate() {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    cvg_data->sync_options();
    if (!this->is_enabled()) {
      std::cerr << "Warning: attempted to sample a disabled covergroup\n";
      return;
    }
    if (!cvg_data->collect) {
      for (auto& cvp : this->cvps) {
        cvp->on_stopped_sample();
      }
      return;
    }
    if (!this->eval_iff()) {
      return;
    }
    for (auto& cvp : this->cvps) {
      try {
        cvp->sample();
      }
      catch(illegal_bin_sample_exception &e) {
        e.update_cvg_info(this->name());
        std::cerr << e.what() << std::endl;
#ifndef FC4SC_NO_THROW // By default the simulation will stop
        std::cerr << "Stopping simulation\n";
        throw(e);
#endif
      }
    }
  }

  virtual void sample() override {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    cvg_data->sync_options();
    if (!this->is_enabled()) {
      std::cerr << "Warning: attempted to sample a disabled covergroup\n";
      return;
    }
    if (!cvg_data->collect) {
      for (auto& cvp : this->cvps) {
        cvp->on_stopped_sample();
      }
      return;
    }
    if (cvg_data->type_data != nullptr && cvg_data->type_data->type_option.strobe) {
      detail::strobe_backend* backend = detail::active_strobe_backend();
      if (backend == nullptr) {
#ifndef FC4SC_NO_THROW
        throw std::logic_error(
            "covergroup::sample: type_option.strobe is true, but no strobe backend is registered (include fc4sc_systemc.hpp)");
#else
        std::cerr << "Warning: covergroup::sample: type_option.strobe is true without a registered strobe backend; sampling immediately\n";
        sample_immediate();
        return;
#endif
      }
      backend->schedule(this);
      return;
    }
    sample_immediate();
  }

  void set_iff(std::function<bool()> pred, const std::string& expr_str = "")
  {
    if (valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    cvg_data->iff_cond = std::move(pred);
    cvg_data->iff_cond_str = expr_str;
  }

  void set_iff(bool constant_cond, const std::string& expr_str = "")
  {
    set_iff(std::function<bool()>([constant_cond]() -> bool { return constant_cond; }), expr_str);
  }

  void set_iff(const bool* flag_ptr, const std::string& expr_str = "")
  {
    if (flag_ptr == nullptr)
      throw std::invalid_argument("covergroup::set_iff: null bool pointer");
    set_iff(std::function<bool()>([flag_ptr]() -> bool { return *flag_ptr; }), expr_str);
  }

  template <typename Pred,
            typename std::enable_if<
                !std::is_same<detail::decay_t<Pred>, bool>::value &&
                !std::is_convertible<detail::decay_t<Pred>, const bool*>::value &&
                !std::is_same<detail::decay_t<Pred>, std::nullptr_t>::value &&
                !std::is_same<detail::decay_t<Pred>, std::function<bool()>>::value,
                int>::type = 0>
  void set_iff(Pred&& pred, const std::string& expr_str = "")
  {
    set_iff(std::function<bool()>(std::forward<Pred>(pred)), expr_str);
  }

  void add_iff(std::function<bool()> pred, const std::string& expr_str = "")
  {
    if (valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    if (!pred) return;
    if (cvg_data->iff_cond) {
      auto prev = cvg_data->iff_cond;
      cvg_data->iff_cond = [prev, pred]() -> bool { return prev() && pred(); };
      if (!expr_str.empty()) {
        if (!cvg_data->iff_cond_str.empty())
          cvg_data->iff_cond_str += " && ";
        cvg_data->iff_cond_str += expr_str;
      }
    } else {
      cvg_data->iff_cond = std::move(pred);
      cvg_data->iff_cond_str = expr_str;
    }
  }

  void clear_iff()
  {
    if (valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    cvg_data->iff_cond = std::function<bool()>();
    cvg_data->iff_cond_str.clear();
  }

  bool has_iff() const override
  {
    return valid_data.use_count() > 0 && static_cast<bool>(cvg_data->iff_cond);
  }

  const std::string& get_iff_str() const
  {
    if (valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    return cvg_data->iff_cond_str;
  }

  bool eval_iff() const override
  {
    if (valid_data.use_count() == 0) return true;
    return !cvg_data->iff_cond || cvg_data->iff_cond();
  }

  template <typename... Refs>
  void bind_sample_args(Refs&... refs)
  {
    typedef std::function<void(const detail::decay_t<Refs>&...)> fn_t;
    auto ptrs = std::make_tuple(&refs...);
    auto fn = std::make_shared<fn_t>([ptrs](const detail::decay_t<Refs>&... vals) {
      assign_bound_tuple(ptrs, std::forward_as_tuple(vals...),
                         detail::make_index_seq<sizeof...(Refs)>());
    });
    bound_sampler_fn_ = fn;
    bound_sampler_type_ = &typeid(fn_t);
  }

  template <typename... Args>
  void set_sampler(std::function<void(Args...)> fn)
  {
    typedef std::function<void(const detail::decay_t<Args>&...)> fn_t;
    auto wrapped = std::make_shared<fn_t>([fn](const detail::decay_t<Args>&... vals) {
      fn(vals...);
    });
    bound_sampler_fn_ = wrapped;
    bound_sampler_type_ = &typeid(fn_t);
  }

  template <typename... CallArgs>
  void sample_with(CallArgs&&... args)
  {
    typedef std::function<void(const detail::decay_t<CallArgs>&...)> fn_t;
    if (!bound_sampler_fn_ || bound_sampler_type_ == nullptr ||
        *bound_sampler_type_ != typeid(fn_t)) {
      throw std::invalid_argument(
          "covergroup::sample_with: unbound or mismatched sample argument types");
    }
    auto* fn = static_cast<fn_t*>(bound_sampler_fn_.get());
    (*fn)(std::forward<CallArgs>(args)...);
    this->sample();
  }

  virtual void disable_cvg() {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    throw("Cannot dynamically disable statically defined covergroup " + this->name());
  }

private:

  std::shared_ptr<void> bound_sampler_fn_;
  const std::type_info* bound_sampler_type_ = nullptr;

  template <typename PtrTuple, typename ValTuple, size_t... I>
  static void assign_bound_tuple(const PtrTuple& ptrs, const ValTuple& vals,
                                 detail::index_seq<I...>)
  {
    (void)std::initializer_list<int>{((*std::get<I>(ptrs) = std::get<I>(vals)), 0)...};
  }

  /*! The context of the coverage model of this instance */
  fc4sc::global* coverage_context() const
  {
    if (this->parent_scp == nullptr)
      return fc4sc::global::getter();
    return this->parent_scp->get_scp_data()->cntxt;
  }

public:

  /*!
   * \brief Computes coverage of an instance. With the strict_sv_options and
   * merge_instances type options, it is the type coverage unless
   * option.get_inst_coverage is set, as in SystemVerilog.
   * \returns Coverage percentage of this instance
   */
  double get_inst_coverage()  const 
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    cvg_data->sync_options();
    if(this->is_enabled() && inst_coverage_is_type_coverage(cvg_data))
      return fc4sc::global::get_coverage(cvg_data->type_data->scp_type_name, cvg_data->type_data->type_name, coverage_context());
    if(this->is_enabled()) {
      double res = 0;
      double weights = 0;

      for (auto &cvp : cvps) {
        res += cvp->get_inst_coverage() * cvp->get_weight();
        weights += cvp->get_weight();

      }

      if (weights == 0 || cvps.size() == 0 || res == 0)
        return (this->option().weight == 0) ? 100 : 0;

      double real = res / weights;
      return (real >= this->option().goal) ? 100 : real;
    }
    else {
      std::cerr << "Warning: called get_inst_coverage on disabled covergroup\n";
      return 100;
    }
  }

  /*!
   * \brief Computes coverage of an instance, or the type coverage as
   * get_inst_coverage()
   * \param bins_covered Number of covered bins
   * \param total Total number of bins in this covergroup
   * \returns Coverage percentage of this instance
   */
  double get_inst_coverage(int &bins_covered, int &total) const 
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    cvg_data->sync_options();
    if(this->is_enabled() && inst_coverage_is_type_coverage(cvg_data))
      return fc4sc::global::get_coverage(cvg_data->type_data->scp_type_name, cvg_data->type_data->type_name,
                                         bins_covered, total, coverage_context());
    if(this->is_enabled()) {
      double res = 0;
      double weights = 0;
      int bins_aux = 0;
      int total_aux = 0;

      bins_covered = 0;
      total = 0;

      for (auto &cvp : cvps)
      {
        res += cvp->get_inst_coverage(bins_aux, total_aux) * cvp->get_weight();
        bins_covered += bins_aux;
        total += total_aux;
        weights += cvp->get_weight();
      }

      if (weights == 0 || cvps.size() == 0 || res == 0)
      {
        return (this->option().weight == 0) ? 100 : 0;
      }

      double real = res / weights;
      return (real >= this->option().goal) ? 100 : real;
    }
    else {
      std::cerr << "Warning: called get_inst_coverage on disabled covergroup\n";
      return 100;
    }
  }

  /*!
   *  \brief Computes coverage count across all instances of this type
   *  \returns Coverage percentage of this instance
   */
  double get_coverage(fc4sc::global* cvg_cntxt = nullptr)
  {
    if(cvg_cntxt == nullptr)
    {
      if(this->parent_scp == nullptr) {
        cvg_cntxt = fc4sc::global::getter();
      }
      else {
        cvg_cntxt = this->parent_scp->get_scp_data()->cntxt;
      }
    }
    return fc4sc::global::get_coverage(this->scp_type_name(), this->type_name(), cvg_cntxt);
  }

  /*!
   * \brief Computes coverage of an instance
   * \param bins_covered Number of covered bins
   * \param total Total number of bins in this covergroup
   * \returns Coverage percentage of this instance
   */
  double get_coverage(int &bins_covered, int &total, fc4sc::global* cvg_cntxt = nullptr)
  {
    if(cvg_cntxt == nullptr)
    {
      if(this->parent_scp == nullptr) {
        cvg_cntxt = fc4sc::global::getter();
      }
      else {
        cvg_cntxt = this->parent_scp->get_scp_data()->cntxt;
      }
    }
    return fc4sc::global::get_coverage(this->scp_type_name(), this->type_name(), bins_covered, total, cvg_cntxt);
  }

  /*!
   * \brief Enables sampling on this covergroup without mutating child coverpoint/cross collect flags
   */
  void start() override
  {
    if (valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    cvg_data->collect = true;
  }

  /*!
   * \brief Disables sampling on this covergroup without mutating child coverpoint/cross collect flags
   */
  void stop() override
  {
    if (valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    cvg_data->collect = false;
  }

  bool is_collecting() const
  {
    if (valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    return cvg_data->collect;
  }

  cvg_base_data_model* get_cvg_data() override
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    return cvg_data;
  }

  const cvg_base_data_model* get_cvg_data() const override
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    return cvg_data;
  }

  std::string& name() override
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    return cvg_data->name;
  }

  const std::string& name() const override
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    return cvg_data->name;
  }

  /*!
   * \brief Sets the name of this instance, which is also the name it is
   * written with to the coverage database. The name must not be empty or
   * be in use by another covergroup or scope instance in the same scope.
   * \param inst_name New name of this instance
   */
  void set_inst_name(const std::string &inst_name)
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    std::string old_name = cvg_data->name;
    if (inst_name == old_name)
      return;
    if (inst_name.empty()) {
      std::cerr << "Error: the name of covergroup " << old_name << " cannot be set to an empty string\n";
      throw("Cannot set the name of a covergroup to an empty string");
    }

    scp_base_data_model* scp_data = cvg_data->parent_scp;
    if (scp_data != nullptr) {
      if (scp_data->cvg_insts.count(inst_name) > 0 || scp_data->child_scp_insts.count(inst_name) > 0) {
        std::cerr << "Error: " << inst_name << " scope name already in use\n";
        throw("Cannot set the name of a covergroup to a name already in use");
      }
      auto it = scp_data->cvg_insts.find(old_name);
      if (it != scp_data->cvg_insts.end() && it->second == cvg_data)
        scp_data->cvg_insts.erase(it);
      scp_data->cvg_insts[inst_name] = cvg_data;
    }
    if (this->parent_scp != nullptr) {
      auto it = this->parent_scp->cvg_insts.find(old_name);
      if (it != this->parent_scp->cvg_insts.end() && it->second == this)
        this->parent_scp->cvg_insts.erase(it);
      this->parent_scp->cvg_insts[inst_name] = this;
    }
    cvg_data->name = inst_name;
  }

  cvg_option& option() const
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    return cvg_data->option;
  }

  cvg_type_option& type_option() override
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    return cvg_data->type_data->type_option;
  }

  const cvg_type_option& type_option() const override
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    return cvg_data->type_data->type_option;
  }

  std::string& type_name() override
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    return cvg_data->type_data->type_name;
  }

  const std::string& type_name() const override
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    return cvg_data->type_data->type_name;
  }

  std::string& scp_type_name()
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    return cvg_data->type_data->scp_type_name;
  }
  
  unsigned int& file_id()
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    return cvg_data->type_data->file_id;
  }

  uint32_t& line()
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    return cvg_data->type_data->line;
  }

  unsigned int& inst_file_id()
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    return cvg_data->inst_file_id;
  }

  uint32_t& inst_line()
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    return cvg_data->inst_line;
  }

  bool is_enabled() const override
  {
    if(valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    return cvg_data->enable;
  }

  template <typename T>
  std::vector<interval_t<T>> get_uncovered_intervals(const std::string& cvp_name) const
  {
    const cvp_base& base = this->get_coverpoint(cvp_name);
    const auto* typed = dynamic_cast<const coverpoint<T>*>(&base);
    if (typed == nullptr) {
      throw std::invalid_argument("covergroup::get_uncovered_intervals: type mismatch for coverpoint " + cvp_name);
    }
    return typed->get_uncovered_intervals();
  }

  template <typename T>
  std::vector<interval_t<T>> get_uncovered_intervals(const coverpoint<T>& cvp) const
  {
    return cvp.get_uncovered_intervals();
  }

  std::map<std::string, std::vector<interval_t<std::string>>> get_uncovered_intervals() const
  {
    if (valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    std::map<std::string, std::vector<interval_t<std::string>>> out;
    for (const auto* cvp : cvps) {
      const auto* cbase = dynamic_cast<const coverpoint_base*>(cvp);
      if (cbase != nullptr) {
        out[cvp->name()] = cbase->get_uncovered_intervals_str();
      }
    }
    return out;
  }

  std::vector<std::string> get_uncovered_bins(const std::string& cvp_or_cross_name) const
  {
    return this->get_coverpoint(cvp_or_cross_name).get_uncovered_bins();
  }

  std::map<std::string, std::vector<std::string>> get_uncovered_bins() const
  {
    if (valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    std::map<std::string, std::vector<std::string>> out;
    for (const auto* cvp : cvps) {
      out[cvp->name()] = cvp->get_uncovered_bins();
    }
    return out;
  }

  std::map<std::string, std::vector<uncovered_bin_info>> get_uncovered_bin_infos() const
  {
    if (valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    std::map<std::string, std::vector<uncovered_bin_info>> out;
    for (const auto* cvp : cvps) {
      out[cvp->name()] = cvp->get_uncovered_bin_infos();
    }
    return out;
  }

  template <typename... Args>
  CrossQueueType<Args...> get_uncovered_cross_tuples(
      const std::string& cross_name,
      uint64_t max_tuples = cross_with_max_tuples,
      bool* truncated = nullptr) const
  {
    const cvp_base& base = this->get_coverpoint(cross_name);
    const auto* crs = dynamic_cast<const cross<Args...>*>(&base);
    if (crs == nullptr) {
      throw std::invalid_argument("covergroup::get_uncovered_cross_tuples: type mismatch for cross " + cross_name);
    }
    return crs->get_uncovered_cross_tuples(max_tuples, truncated);
  }

  template <typename... Args>
  CrossQueueType<Args...> get_uncovered_cross_tuples(
      const cross<Args...>& crs,
      uint64_t max_tuples = cross_with_max_tuples,
      bool* truncated = nullptr) const
  {
    return crs.get_uncovered_cross_tuples(max_tuples, truncated);
  }

  std::map<std::string, std::vector<std::vector<std::string>>> get_uncovered_cross_tuples(
      uint64_t max_tuples = 65536,
      bool* truncated = nullptr) const
  {
    if (valid_data.use_count() == 0) {
      std::cerr << "Error: coverage data has been deleted\n";
      throw("Error: coverage data has been deleted");
    }
    if (truncated != nullptr) *truncated = false;
    std::map<std::string, std::vector<std::vector<std::string>>> out;
    for (const auto* cvp : cvps) {
      const auto* crs = dynamic_cast<const cross_base*>(cvp);
      if (crs != nullptr) {
        bool local_trunc = false;
        out[cvp->name()] = crs->get_uncovered_cross_tuples_str(max_tuples, &local_trunc);
        if (local_trunc && truncated != nullptr) *truncated = true;
      }
    }
    return out;
  }

};

template <typename... Args>
class typed_covergroup : public covergroup
{
protected:
  typed_covergroup(std::string inst_name = "", const char *auto_inst_file_name = "",
                   int auto_line = 1, fc4sc::global* cntxt = fc4sc::global::getter())
    : covergroup("typed_covergroup", __FILE__, __LINE__, inst_name,
                 auto_inst_file_name, auto_line, cntxt) {}

  typed_covergroup(scp_base& scp, std::string inst_name = "",
                   const char *auto_inst_file_name = "", int auto_line = 1)
    : covergroup(scp, "typed_covergroup", scp.type_name(), __FILE__, __LINE__,
                 inst_name, auto_inst_file_name, auto_line) {}

public:
  using covergroup::sample;
  virtual void sample(Args... args) = 0;
};

namespace detail {

class factory_clone_host : public cvg_base
{
  covergroup_data_model dummy_data_;

public:
  std::string host_name = "factory_clone_host";
  std::string host_type = "factory_clone_host";
  std::string host_scp = _FC4SC_DEFAULT_SCOPE_TYPE_;
  unsigned int host_fid = 0;
  uint32_t host_line = 0;
  cvg_option host_opt;
  cvg_type_option host_type_opt;

  cvg_base_data_model* get_cvg_data() override { return &dummy_data_; }
  const cvg_base_data_model* get_cvg_data() const override { return &dummy_data_; }
  bool is_enabled() const override { return true; }
  void disable_cvg() override {}
  cvp_metadata_t get_strings(cvp_base*) override { return cvp_metadata_t(); }
  cvg_option& option() const override { return const_cast<cvg_option&>(host_opt); }
  cvg_type_option& type_option() override { return host_type_opt; }
  const cvg_type_option& type_option() const override { return host_type_opt; }
  std::string& type_name() override { return host_type; }
  const std::string& type_name() const override { return host_type; }
  unsigned int& file_id() override { return host_fid; }
  uint32_t& line() override { return host_line; }
  unsigned int& inst_file_id() override { return host_fid; }
  uint32_t& inst_line() override { return host_line; }
  std::string& name() override { return host_name; }
  const std::string& name() const override { return host_name; }
  double get_inst_coverage() const override { return 0; }
  double get_inst_coverage(int& h, int& t) const override { h = 0; t = 0; return 0; }
  void start() override {}
  void stop() override {}
  void sample() override {}

  ~factory_clone_host() override
  {
    dummy_data_.cvps.clear();
    cvps.clear();
  }
};

} // namespace detail

/*!
 * \class dynamic_covergroup_factory covergroup.hpp
 * \brief class for creating dynamic covergroup definition.
 */
class dynamic_covergroup_factory {
public:

  std::string type_name;
  std::string file_name;
  uint32_t line;

  /*! Array of associated coverpoints and crosses */
  std::vector<cvp_base *> cvps;

  scope_factory_base* scp_type_factory = nullptr;

  std::function<bool()> iff_cond;
  std::string iff_cond_str;

  dynamic_covergroup_factory(const char *type_name, const char *file_name = "", int line = 1)
  {
    this->type_name = type_name;
    this->file_name = file_name;
    this->line = line;
  }

  dynamic_covergroup_factory(scope_factory_base& scp,const char *type_name, const char *file_name = "", int line = 1) 
  {
    this->type_name = type_name;
    this->file_name = file_name;
    this->line = line;
    scp.register_sub_cvg_type(this);
  }

  std::string get_scp_type_name()
  {
    if(scp_type_factory != nullptr) {
      return scp_type_factory->get_scp_type_name();
    }
    else {
      return _FC4SC_DEFAULT_SCOPE_TYPE_;
    }
  }

  /*!
   * \brief Create a coverpoint without specifying a sample condition expression. Adds a coverpoint to the dynamic
   * covergroup factory which will be used again when the dynamic covergroup is instanced. Returns a dynamic coverpoint
   * factory object for the user to add bins to the created coverpoint. template specifies the type of sample expression
   */
  template<typename T>
  dynamic_coverpoint_factory<T,bool()> create_coverpoint(std::string cvp_name, std::function<T> sample_expr) 
  {
    typedef typename std::function<T>::result_type ret_type;

    for(auto cvp_it : this->cvps) {
      if(cvp_it->name() == cvp_name)
        throw ("Cannot have multiple coverpoints with same name " + cvp_name + " in covergroup" + " " + this->type_name);
    }

    coverpoint<ret_type>* dyn_cvp = new coverpoint<ret_type>;
    dyn_cvp->name() = cvp_name;
    dyn_cvp->has_sample_expression = true;
    dyn_cvp->cvp_data->sample_expression_str = "dynamic sample";
    dyn_cvp->cvp_data->sample_condition_str = "dynamic sample";
    dyn_cvp->sample_condition = [](){ return true; };
    this->cvps.push_back(dyn_cvp);

    dynamic_coverpoint_factory<T,bool()> dyn_cvp_fact;
    dyn_cvp_fact.cvp_fact = dyn_cvp;
    dyn_cvp_fact.sample_expression = sample_expr;
    dyn_cvp_fact.sample_condition = [](){ return true; };
    return dyn_cvp_fact;
  }

  /*!
   * \brief Create a coverpoint. Adds a coverpoint to the dynam covergroup factory 
   * which will be used again when the dynamic covergroup is instanced. Returns a dynamic coverpoint
   * factory object for the user to add bins to the created coverpoint. Template args needed to specify sample
   * expression type and sample condition expression type
   */
  template<typename T,typename U>
  dynamic_coverpoint_factory<T,U> create_coverpoint(std::string cvp_name, std::function<T> sample_expr, std::function<U> cond_expr ) 
  {
    typedef typename std::function<T>::result_type ret_type;

    for(auto cvp_it : this->cvps) {
      if(cvp_it->name() == cvp_name)
        throw ("Cannot have multiple coverpoints with same name " + cvp_name + " in covergroup" + " " + this->type_name);
    }

    coverpoint<ret_type>* dyn_cvp = new coverpoint<ret_type>;
    dyn_cvp->name() = cvp_name;
    dyn_cvp->has_sample_expression = true;
    dyn_cvp->cvp_data->sample_expression_str = "dynamic sample";
    dyn_cvp->cvp_data->sample_condition_str = "dynamic sample";
    this->cvps.push_back(dyn_cvp);

    dynamic_coverpoint_factory<T,U> dyn_cvp_fact;
    dyn_cvp_fact.cvp_fact = dyn_cvp;
    dyn_cvp_fact.sample_expression = sample_expr;
    dyn_cvp_fact.sample_condition = cond_expr;
    return dyn_cvp_fact;
  }

  /*!
   * \brief Add cross to dynamic_covergroup type
   * \param cross name strig
   * \param pointers to dynamic_coverpoint_factory of crossed coverpoints
   */
  template<typename ... Args>
  void add_cross(std::string crs_name, Args&... args)
  {
    internal_add_cross(crs_name,(args.cvp_fact)...);
  }

  /*!
   * \brief Helper function to add cross to covergroup
   */
  template<typename ... Args>
  void internal_add_cross(std::string& crs_name, coverpoint<Args>*... args)
  {
    cross<Args...>* crs = new cross<Args...>(crs_name,args...);
    this->cvps.push_back(crs);
  }

  template<typename... Args, typename... Restrictions>
  void add_cross_with_bins(const std::string& crs_name, coverpoint<Args>*... args,
                           Restrictions... restrictions)
  {
    for (auto cvp_it : this->cvps) {
      if (!crs_name.empty() && cvp_it->name() == crs_name)
        throw std::invalid_argument("Duplicate cross name " + crs_name + " in covergroup " + this->type_name);
    }
    cross<Args...>* crs = new cross<Args...>(crs_name, args..., restrictions...);
    this->cvps.push_back(crs);
  }

  template <typename... Args, typename... Restrictions, size_t... I>
  void add_cross_by_name_impl(const std::string& crs_name,
                              const std::vector<std::string>& cvp_names,
                              detail::index_seq<I...>,
                              Restrictions... restrictions)
  {
    add_cross_with_bins<Args...>(
        crs_name,
        get_coverpoint_factory_ptr_checked<typename std::tuple_element<I, std::tuple<Args...>>::type>(
            cvp_names[I])...,
        restrictions...);
  }

  template <typename T>
  coverpoint<T>* get_coverpoint_factory_ptr_checked(const std::string& cvp_name) const
  {
    auto* ptr = get_coverpoint_factory_ptr<T>(cvp_name);
    if (ptr == nullptr)
      throw std::invalid_argument("Coverpoint " + cvp_name + " not found or type mismatch in " + this->type_name);
    return ptr;
  }

  template <typename T>
  coverpoint<T>* get_coverpoint_factory_ptr(const std::string& cvp_name) const
  {
    for (auto* cvp_it : this->cvps) {
      if (cvp_it->name() == cvp_name)
        return dynamic_cast<coverpoint<T>*>(cvp_it);
    }
    return nullptr;
  }

  template <typename... Args, typename... Restrictions>
  void add_cross_by_name(const std::string& crs_name,
                         const std::vector<std::string>& cvp_names,
                         Restrictions... restrictions)
  {
    if (cvp_names.size() != sizeof...(Args))
      throw std::invalid_argument("add_cross_by_name: coverpoint count mismatch");
    add_cross_by_name_impl<Args...>(crs_name, cvp_names,
                                    detail::make_index_seq<sizeof...(Args)>(),
                                    restrictions...);
  }

  void set_iff(std::function<bool()> pred, const std::string& expr_str = "")
  {
    iff_cond = std::move(pred);
    iff_cond_str = expr_str;
  }

  void set_iff(bool constant_cond, const std::string& expr_str = "")
  {
    set_iff(std::function<bool()>([constant_cond]() -> bool { return constant_cond; }), expr_str);
  }

  void set_iff(const bool* flag_ptr, const std::string& expr_str = "")
  {
    if (flag_ptr == nullptr)
      throw std::invalid_argument("dynamic_covergroup_factory::set_iff: null bool pointer");
    set_iff(std::function<bool()>([flag_ptr]() -> bool { return *flag_ptr; }), expr_str);
  }

  template <typename Pred,
            typename std::enable_if<
                !std::is_same<detail::decay_t<Pred>, bool>::value &&
                !std::is_convertible<detail::decay_t<Pred>, const bool*>::value &&
                !std::is_same<detail::decay_t<Pred>, std::nullptr_t>::value &&
                !std::is_same<detail::decay_t<Pred>, std::function<bool()>>::value,
                int>::type = 0>
  void set_iff(Pred&& pred, const std::string& expr_str = "")
  {
    set_iff(std::function<bool()>(std::forward<Pred>(pred)), expr_str);
  }

  void clear_iff()
  {
    iff_cond = std::function<bool()>();
    iff_cond_str.clear();
  }

  void extend(const dynamic_covergroup_factory& base)
  {
    detail::factory_clone_host host;
    for (auto* existing : this->cvps)
      host.register_cvp(existing);
    std::vector<cvp_base*> cloned_cvps;
    cloned_cvps.reserve(base.cvps.size());
    try {
      for (auto* base_cvp : base.cvps) {
        if (!base_cvp->name().empty()) {
          for (auto* existing : this->cvps) {
            if (existing->name() == base_cvp->name())
              throw std::invalid_argument(
                  "Duplicate coverpoint/cross name " + base_cvp->name() + " when extending " + base.type_name);
          }
          for (auto* cloned_prev : cloned_cvps) {
            if (cloned_prev->name() == base_cvp->name())
              throw std::invalid_argument(
                  "Duplicate coverpoint/cross name " + base_cvp->name() + " when extending " + base.type_name);
          }
        }
        cvp_base* cloned = base_cvp->create_instance(&host);
        host.register_cvp(cloned);
        cloned_cvps.push_back(cloned);
      }
    } catch (...) {
      for (auto* c : cloned_cvps) {
        delete c->get_data();
        delete c;
      }
      throw;
    }
    this->cvps.insert(this->cvps.end(), cloned_cvps.begin(), cloned_cvps.end());
    if (base.iff_cond) {
      if (this->iff_cond) {
        auto base_pred = base.iff_cond;
        auto drv_pred = this->iff_cond;
        this->iff_cond = [base_pred, drv_pred]() -> bool {
          return base_pred() && drv_pred();
        };
        if (!base.iff_cond_str.empty() && !this->iff_cond_str.empty())
          this->iff_cond_str = "(" + base.iff_cond_str + ") && (" + this->iff_cond_str + ")";
        else if (!base.iff_cond_str.empty())
          this->iff_cond_str = base.iff_cond_str;
      } else {
        this->iff_cond = base.iff_cond;
        this->iff_cond_str = base.iff_cond_str;
      }
    }
  }

  ~dynamic_covergroup_factory()
  {
    for (auto cvp : this->cvps)
    {
      delete cvp->get_data();
      delete cvp;
    }
  }
};


/*!
 * \class dynamic_covergroup covergroup.hpp
 * \brief instances dynamic covergroup types. Constructor requires a dynamic_covergroup_factory
 * to specify the type of the dynamic covergroup instance
 */
class dynamic_covergroup : public covergroup {
private:
  void seed_cvp_type_option(cvp_base* next_cvp)
  {
    if (next_cvp == nullptr || next_cvp->get_data() == nullptr)
      return;
    cvg_metadata* td = this->get_cvg_data()->type_data;
    if (td == nullptr)
      return;
    const std::string& cvp_name = next_cvp->name();
    const cvp_type_option& own_opt = next_cvp->get_data()->own_type_option;
    const cvp_type_option defaults;
    if (td->cvp_type_options.count(cvp_name) == 0 &&
        (own_opt.weight != defaults.weight ||
         own_opt.goal != defaults.goal ||
         own_opt.comment != defaults.comment ||
         own_opt.distribute_first != defaults.distribute_first)) {
      td->cvp_type_options[cvp_name] = own_opt;
      if (own_opt.distribute_first != defaults.distribute_first) {
        td->synced_cvp_distribute_first_[cvp_name] = own_opt.distribute_first;
        td->cvp_distribute_first_overridden_[cvp_name] = true;
      }
    }
  }

public:

  dynamic_covergroup(dynamic_covergroup_factory& cvg_type, std::string inst_name = "",const char *inst_file_name = "", int inst_line = 1, fc4sc::global* cntxt = fc4sc::global::getter()) : covergroup(cvg_type.type_name.c_str(),cvg_type.get_scp_type_name(),cvg_type.file_name.c_str(),cvg_type.line,inst_name,inst_file_name,inst_line,cntxt)
  {
    if (cvg_type.iff_cond)
      this->set_iff(cvg_type.iff_cond, cvg_type.iff_cond_str);
    //iterate through each coverpoint of the covergroup type and create a dynamic instance of the coverpoint
    for (auto cvp_it : cvg_type.cvps) 
    {
      cvp_base* next_cvp = cvp_it->create_instance(this);
      this->cvg_base::register_cvp(next_cvp);
      seed_cvp_type_option(next_cvp);
      this->get_cvg_data()->add_cvp_data(next_cvp->get_data());
    }

  }

  dynamic_covergroup(scp_base& scp,dynamic_covergroup_factory& cvg_type, std::string inst_name = "",const char *inst_file_name = "", int inst_line = 1) : covergroup(scp,cvg_type.type_name.c_str(),cvg_type.get_scp_type_name(),cvg_type.file_name.c_str(),cvg_type.line,inst_name,inst_file_name,inst_line)
  {
    if (cvg_type.iff_cond)
      this->set_iff(cvg_type.iff_cond, cvg_type.iff_cond_str);
    //iterate through each coverpoint of the covergroup type and create a dynamic instance of the coverpoint
    for (auto cvp_it : cvg_type.cvps) 
    {
      cvp_base* next_cvp = cvp_it->create_instance(this);
      this->cvg_base::register_cvp(next_cvp);
      seed_cvp_type_option(next_cvp);
      this->get_cvg_data()->add_cvp_data(next_cvp->get_data());
    }

  }

  using covergroup::sample;

  template <typename First, typename... Rest>
  void sample(First&& first, Rest&&... rest)
  {
    this->sample_with(std::forward<First>(first), std::forward<Rest>(rest)...);
  }
  
  void disable_cvg() {
    get_cvg_data()->enable = false;
  }

  ~dynamic_covergroup()
  {
    for (size_t i = cvps.size(); i > 0; --i)
    {
      delete cvps[i - 1];
    }
  }

};

} // namespace fc4sc

#endif /* FC4SC_COVERGROUP_HPP */
