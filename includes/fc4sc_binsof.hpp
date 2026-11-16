/******************************************************************************

   Copyright 2003-2018 AMIQ Consulting s.r.l.

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

#ifndef FC4SC_BINSONF_HPP
#define FC4SC_BINSONF_HPP

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "fc4sc_intervals.hpp"
#include "fc4sc_coverpoint.hpp"

namespace fc4sc {

/*!
 * \brief Defines a class for select expressions of the bins of a cross
 *
 *   A select expression is made of binsof conditions combined with the
 * operators !, && and ||, as in SystemVerilog (IEEE 1800-2017, 19.6.1):
 * binsof(a) && !binsof(b).intersect(1). It selects products of the bins of
 * the crossed coverpoints, for a user defined bin of a cross. See
 * fc4sc::binsof and fc4sc::cross_bin.
 */
class select_expression
{
protected:

  select_expression() { }

  explicit select_expression(std::shared_ptr<const cross_select_node> node) : node(node) { }

  /*! Root of the expression */
  std::shared_ptr<const cross_select_node> node;

  friend class cross_bin;
  friend select_expression operator!(const select_expression& operand);
  friend select_expression operator&&(const select_expression& lhs, const select_expression& rhs);
  friend select_expression operator||(const select_expression& lhs, const select_expression& rhs);

  static select_expression combine(cross_select_node::op_t op, const select_expression& lhs,
                                   const select_expression* rhs)
  {
    auto node = std::make_shared<cross_select_node>();
    node->op = op;
    node->lhs = lhs.node;
    if (rhs != nullptr) node->rhs = rhs->node;
    return select_expression(node);
  }
};

/*! \brief Selects the products which the operand does not select */
inline select_expression operator!(const select_expression& operand)
{
  return select_expression::combine(cross_select_node::not_, operand, nullptr);
}

/*! \brief Selects the products which both operands select */
inline select_expression operator&&(const select_expression& lhs, const select_expression& rhs)
{
  return select_expression::combine(cross_select_node::and_, lhs, &rhs);
}

/*! \brief Selects the products which either operand selects */
inline select_expression operator||(const select_expression& lhs, const select_expression& rhs)
{
  return select_expression::combine(cross_select_node::or_, lhs, &rhs);
}

/*!
 * \brief Defines a class for the binsof conditions of select expressions
 * \tparam T Type of the values of the coverpoint
 *
 *   binsof<T>(&cvp) selects the products of a cross which have any bin of
 * the coverpoint cvp, and binsof<T>(&cvp, "b") the products which have
 * its bin b, as the SystemVerilog binsof(cvp) and binsof(cvp.b). If b is
 * a bin array, all its bins are selected; a single bin of the array is
 * named as "b[2]". The coverpoint may also be given by reference, and
 * with C++17 the type can be left out, as in binsof(&cvp).
 *
 *   intersect() keeps the selected bins which have a value in the values
 * it is given, as the SystemVerilog binsof(cvp) intersect {[0:3], 7},
 * which is written binsof<int>(&cvp).intersect(interval(0, 3), 7). It
 * takes values, intervals and vectors of them. The values of a transition
 * bin are the values of its sequences. As in SystemVerilog,
 * !binsof<int>(&cvp).intersect(7) selects the products whose bin of cvp
 * has no value 7.
 */
template <typename T>
class binsof : public select_expression
{
  /*! The condition: its coverpoint, bin and intersect values */
  coverpoint<T>* cvp = nullptr;
  std::string bin_name;
  bool has_values = false;
  std::vector<interval_t<T>> values;

  void add_value(const T& value) { values.push_back(interval_t<T>(value, value)); }
  void add_value(const interval_t<T>& intv)
  {
    values.push_back((intv.first <= intv.second) ? intv : interval_t<T>(intv.second, intv.first));
  }
  void add_value(const std::vector<T>& vec) { for (auto &v : vec) add_value(v); }
  void add_value(const std::vector<interval_t<T>>& vec) { for (auto &v : vec) add_value(v); }

  void add_values() { }

  template <typename Head, typename... Tail>
  void add_values(const Head& head, const Tail&... tail)
  {
    add_value(head);
    add_values(tail...);
  }

  bool has_value(T low, T high) const
  {
    for (auto &intv : values)
      if (intv.first <= high && low <= intv.second)
        return true;
    return false;
  }

  /*! Returns which bins of the coverpoint this condition selects */
  std::vector<bool> select_bins() const
  {
    std::vector<bool> selected(cvp->bins.size(), false);
    bool found = false;
    for (size_t i = 0; i < cvp->bins.size(); ++i)
    {
      bin_data_model<T>* data = cvp->bins[i].bin_data;
      const std::string& name = data->name;
      if (!bin_name.empty() && name != bin_name &&
          name.compare(0, bin_name.size() + 1, bin_name + "[") != 0)
        continue;
      found = true;
      if (!has_values) {
        selected[i] = true;
        continue;
      }
      for (auto &intv : data->intervals)
        selected[i] = selected[i] || has_value(intv.first, intv.second);
      auto trans_data = dynamic_cast<transition_bin_data_model<T>*>(data);
      if (trans_data != nullptr)
        for (auto &seq : trans_data->sequences)
          for (auto value : seq)
            selected[i] = selected[i] || has_value(value, value);
    }
    if (!found)
      throw std::invalid_argument("binsof: no bin " + bin_name + " in coverpoint " + cvp->name());
    return selected;
  }

  /*! Builds the node of this condition */
  void make_node()
  {
    auto node = std::make_shared<cross_select_node>();
    node->cvp = cvp;
    binsof<T> condition = *this;
    condition.node.reset();
    node->select_bins = [condition]() { return condition.select_bins(); };
    this->node = node;
  }

public:

  /*!
   * \brief Selects the bins of a coverpoint
   * \param cvp The coverpoint
   * \param bin_name Name of the bin to select, or empty for all the bins
   */
  explicit binsof(coverpoint<T>* cvp, const std::string& bin_name = "") : cvp(cvp), bin_name(bin_name)
  {
    if (cvp == nullptr)
      throw std::invalid_argument("binsof: no coverpoint");
    make_node();
  }

  /*!
   * \brief Selects the bins of a coverpoint
   * \param cvp The coverpoint
   * \param bin_name Name of the bin to select, or empty for all the bins
   */
  explicit binsof(coverpoint<T>& cvp, const std::string& bin_name = "") : binsof(&cvp, bin_name) { }

  /*!
   * \brief Keeps the selected bins which have a value in the given ones
   * \param args Values, intervals, or vectors of values or intervals
   * \returns The new condition
   */
  template <typename... Args>
  binsof<T> intersect(const Args&... args) const
  {
    binsof<T> condition = *this;
    condition.has_values = true;
    condition.add_values(args...);
    condition.make_node();
    return condition;
  }
};

/*!
 * \brief Defines a class for the user defined bins of crosses
 *
 *   A user defined bin of a cross holds the products of the bins of the
 * crossed coverpoints which its select expression selects. The
 * SystemVerilog
 *
 *   cross a, b {
 *     bins lo_one = binsof(a.lo) && binsof(b) intersect {1};
 *     ignore_bins ign = binsof(a.hi);
 *     illegal_bins bad = binsof(a.lo) && binsof(b.zero);
 *   }
 *
 * is written
 *
 *   cross<int, int>(this, "a_x_b", &a, &b,
 *     cross_bin("lo_one", binsof<int>(&a, "lo") && binsof<int>(&b).intersect(1)),
 *     ignore_cross_bin("ign", binsof<int>(&a, "hi")),
 *     illegal_cross_bin("bad", binsof<int>(&a, "lo") && binsof<int>(&b, "zero")))
 *
 *   As in SystemVerilog (IEEE 1800-2017, 19.6), the cross has a bin
 * created automatically for each product of bins which is in no user
 * defined bin, ignore bin or illegal bin. The products of ignore bins and
 * illegal bins are in no other bin, and are not counted in the coverage.
 * A sample of an illegal product throws an illegal_bin_sample_exception,
 * as an illegal bin of a coverpoint does. A user defined bin is hit once
 * by a sample which hits any of its products, and a bin which has no
 * products is not counted in the coverage. The UCIS XML file has a
 * crossBin for each user defined bin, with its name and type, and the
 * index -1 for each coverpoint, as it has no single product of bins.
 */
class cross_bin
{
public:

  /*! Data of the bin */
  cross_bin_data_model data;

  /*!
   * \brief Takes the bin name and its select expression
   * \param name Name of the bin
   * \param select Select expression of the bin
   */
  cross_bin(const std::string& name, const select_expression& select)
  {
    data.name = name;
    data.bin_type = default_;
    data.select = select.node;
  }

  virtual ~cross_bin() = default;
};

/*!
 * \brief Defines a class for ignore bins of crosses, as the SystemVerilog
 * "ignore_bins b = binsof(a.lo);". See fc4sc::cross_bin.
 */
class ignore_cross_bin : public cross_bin
{
public:
  ignore_cross_bin(const std::string& name, const select_expression& select) : cross_bin(name, select)
  {
    data.bin_type = ignore_;
  }
};

/*!
 * \brief Defines a class for illegal bins of crosses, as the SystemVerilog
 * "illegal_bins b = binsof(a.lo);". See fc4sc::cross_bin.
 */
class illegal_cross_bin : public cross_bin
{
public:
  illegal_cross_bin(const std::string& name, const select_expression& select) : cross_bin(name, select)
  {
    data.bin_type = illegal_;
  }
};

}	// namespace fc4sc

#endif
