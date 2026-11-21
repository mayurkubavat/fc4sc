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
 \file fc_options.hpp
 \brief Structures for options and type options

   This file contains the definitions for the configurable options
 of coverage objects
 */

#ifndef FC4SC_OPTIONS_HPP
#define FC4SC_OPTIONS_HPP

#include <string>
#include <ostream>

/*!
 * \class cvg_option fc_options.hpp
 * \brief Covergroup option declaration
 */
struct cvg_option
{
  /*! Weight of instance when computing coverage */
  unsigned int weight;

  /*! Target coverage percentage */
  unsigned int goal;

  /*! Comment for this coverage */
  std::string comment;

  /*! Minimum of hits for each bin */
  unsigned int at_least;

  /*! Max number of generated bins (inherited by coverpoints; automatic bin generation not yet implemented) */
  unsigned int auto_bin_max;

  /*! Issue warning if bins overlap in child coverpoints */
  bool detect_overlap;

  /*! Number of missing bins in child crosses to be printed */
  unsigned int cross_num_print_missing;

  /*!
   * Save the coverage of this instance in the coverage database. As in
   * SystemVerilog, every instance contributes to the coverage of its
   * covergroup type. The option is used when the strict_sv_options type
   * option is set: an instance without per_instance is then not written
   * to the database on its own, but merged with the other such instances
   * of its type into one covergroup instance, with an empty name and the
   * name of the type as its alias, which holds the type coverage. Without strict_sv_options every instance is
   * written, as SystemVerilog allows.
   */
  bool per_instance;

  /*!
   * Only applies when the merge_instances type option is set, and is used
   * when the strict_sv_options type option is set: get_inst_coverage() of
   * the covergroup instance, of its coverpoints and of its crosses returns
   * the instance coverage when it is set, and the type coverage, as
   * get_coverage(), when it is not, as in SystemVerilog.
   * Without strict_sv_options get_inst_coverage() always returns the
   * instance coverage.
   */
  bool get_inst_coverage;

  /*!
   * \brief Sets all values to default
   */
  cvg_option()
  {
    this->weight = 1;
    this->goal = 100;
    this->at_least = 1;
    this->auto_bin_max = 10;
    this->detect_overlap = 0;

    this->cross_num_print_missing = 0;
    this->per_instance = 0;
    this->get_inst_coverage = 0;
  }

};

/*!
 * \class cvp_option fc_options.hpp
 * \brief Coverpoint option declaration
 */
struct cvp_option
{
  /*! Weight of instance when computing coverage */
  unsigned int weight;

  /*! Target coverage percentage */
  unsigned int goal;

  /*! Comment for this coverage */
  std::string comment;

  /*! Minimum of hits for each bin */
  unsigned int at_least;

  /*! Max number of generated bins (automatic bin generation not yet implemented) */
  unsigned int auto_bin_max;

  /*! Issue a warning for each pair of regular bins of the coverpoint which
   *  have values in common. The check is made once, on the first sample of
   *  the coverpoint after the option is set: the options are set after the
   *  bins are defined, and setting a field cannot start the check. */
  bool detect_overlap;

  /*!
   * \brief Sets all values to default
   */
  cvp_option()
  {
    this->weight = 1;
    this->goal = 100;
    this->at_least = 1;
    this->auto_bin_max = 10;
    this->detect_overlap = 0;
  }

};

/*!
 * \class cross_option fc_options.hpp
 * \brief Covergroup option declaration
 */
struct cross_option // cross option declaration
{
  /*! Weight of instance when computing coverage */
  unsigned int weight;

  /*! Target coverage percentage */
  unsigned int goal;

  /*! Comment for this coverage */
  std::string comment;

  /*! Minimum of hits for each bin */
  unsigned int at_least;

  /*! Number of missing (never hit) bins written to the coverage database
   *  in addition to the hit bins. Define FC4SC_SAVE_ALL_CROSS_BINS to write
   *  every missing bin; like FC4SC_NO_THROW, it changes inline code, so it
   *  must be defined for every file of a program (on the command line). */
  unsigned int cross_num_print_missing;

  /*!
   * \brief Sets all values to default
   */
  cross_option()
  {
    weight = 1;
    goal = 100;
    at_least = 1;
    cross_num_print_missing = 0;
  }

};

/*!
 * \class cvp_type_option fc_options.hpp
 * \brief Coverpoint and cross type option declaration
 *
 *   The type options of a coverpoint or cross are shared by all the
 * instances of its covergroup type, as the static type_option of
 * SystemVerilog. They are used when the strict_sv_options type option of
 * the covergroup is set: when type_option.merge_instances is also set, the
 * type coverage of the covergroup is the average of the merged coverage of
 * its coverpoints and crosses weighted by their type_option.weight, and the
 * type coverage of a coverpoint or cross reaches 100 at its
 * type_option.goal. They are also the options written for the coverpoints
 * and crosses of the covergroup instance which holds the type coverage in
 * the coverage database.
 */
struct cvp_type_option
{
  /*! Weight of this coverpoint or cross in the type coverage */
  unsigned int weight;

  /*! Target type coverage percentage */
  unsigned int goal;

  /*! Comment for the type of this coverpoint or cross */
  std::string comment;

  /*!
   * \brief Sets all values to default
   */
  cvp_type_option()
  {
    weight = 1;
    goal = 100;
  }
};

/*!
 * \class cvg_type_option fc_options.hpp
 * \brief Covergroup option declaration
 */
struct cvg_type_option // covergroup type_option declaration
{
  /*! Weight of instance when computing coverage */
  unsigned int weight;

  /*! Target coverage percentage */
  unsigned int goal;

  /*! Comment for this coverage */
  std::string comment;

  /*!
   * Computes the type coverage on the union of the bins of all instances
   * instead of as the weighted average of the instance coverages. The
   * options of the coverpoints and crosses are taken from the first enabled
   * instance.
   */
  bool merge_instances;

  /*!
   * Gives the options their SystemVerilog meaning (IEEE 1800-2017, 19.7,
   * 19.7.1 and 19.11) for this covergroup type, which changes the coverage
   * database and some coverage numbers, so it is off by default:
   *  - option.per_instance: only the instances which set it are written to
   *    the coverage database. The others are merged into one covergroup
   *    instance with an empty name, which no instance can have, and the
   *    name of the type as its alias, whose bins are the union of their
   *    bins, matched by name, with the hits added up: a reader of the
   *    database computes the type coverage from it as for merge_instances,
   *    as the weighted average of these instances cannot be computed
   *    without their data.
   *  - option.get_inst_coverage: with merge_instances, get_inst_coverage()
   *    returns the type coverage unless it is set.
   *  - the type_option of the coverpoints and crosses: with merge_instances,
   *    their weight and goal are used for the type coverage instead of the
   *    options of the first instance; their goal is also used for the type
   *    coverage of a coverpoint or cross without merge_instances.
   *  - without merge_instances, the type coverage of a coverpoint or cross
   *    is its coverage in each instance weighted by its option.weight in
   *    that instance, instead of the weight of the covergroup instance.
   * Set it, as merge_instances, before any coverage is computed or saved.
   */
  bool strict_sv_options;

  /*!
   * \brief Sets all values to default
   */
  cvg_type_option()
  {
    this->weight = 1;
    this->goal = 100;
    this->merge_instances = 0;
    this->strict_sv_options = 0;
  }
};

#endif /* FC4SC_OPTIONS_HPP */
