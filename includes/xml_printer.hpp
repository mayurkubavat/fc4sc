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
#ifndef UCIS_PRINTER_HPP
#define UCIS_PRINTER_HPP

#include <map>
#include <memory>
#include <sstream>
#include <string>

#include "fc4sc_base.hpp"
#include "fc4sc_coverage_db.hpp"

typedef enum fc4sc_format {
  ucis_xml = 1
} fc4sc_format;

class xml_printer : public fc4sc::covVisitorBase {
    
  /*! key generation for UCIS XML */
  int gkey = 1;

  /*! number of coverpointBin elements written for the active coverpoint */
  size_t coverpoint_bins_written_ = 0;

  /*! output stream for xml file */
  std::ostream& stream;

  /*! optional physical file path associated with the output stream */
  std::string physical_name_;

  static bool has_coverpoints(const fc4sc::cvg_base_data_model& cvg)
  {
    for (auto cvp : cvg.cvps)
      if (dynamic_cast<const fc4sc::coverpoint_base_data_model*>(cvp) != nullptr)
        return true;
    return false;
  }

public:

  xml_printer(std::ostream& out_stream, const std::string& physical_name = "")
      : stream(out_stream), physical_name_(physical_name) {}

  /*!
   * \brief gets another unique key for UCIS XML generation
  */
  int get_unique_key()
  {
    return gkey++;
  }

  /*!
   * \brief initialize key var for UCIS XML generation
  */
  void init_unique_key()
  {
    gkey = 1;
  }

  /*!
   * \brief Function called to print an UCIS XML with all the data
   * \param stream Where to print
   */
  void print_data_xml(fc4sc::global* cntxt)
  {
    // Header
    stream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
           << "\n";
    stream << "<UCIS xmlns=\"UCIS\" xmlns:ucis=\"http://www.w3.org/2001/XMLSchema-instance\" ucisVersion=\"1.0\" ";
    const char* writer = getenv("USER");
    stream << " writtenBy=\""
           << escape_xml_chars(writer ? writer : "")
           << "\"\n";

    std::time_t cur_time = std::time(0);
    std::tm* time_now = std::localtime(&cur_time);
    stream << " writtenTime=\""
           << time_now->tm_year+1900 << '-'
	   << (time_now->tm_mon + 1)/10 << (time_now->tm_mon + 1)%10 << '-'
	   << time_now->tm_mday/10 << time_now->tm_mday%10 << "T"
	   << time_now->tm_hour/10 << time_now->tm_hour%10 << ":"
	   << time_now->tm_min/10 << time_now->tm_min%10 << ":"
	   << time_now->tm_sec/10 << time_now->tm_sec%10
           << "\"";
    stream << ">\n";

    const auto& raw_table = fc4sc::global::get_file_id_to_name_table(cntxt);
    std::map<unsigned int, std::string> sorted_files(raw_table.begin(), raw_table.end());
    for (const auto &fname_it : sorted_files)
    {
      const std::string& fname = fname_it.second.empty()
          ? std::string("unknown")
          : fname_it.second;
      stream << "<sourceFiles ";
      stream << " fileName=\""
	     << escape_xml_chars(fname)
	     << "\"";
      stream << " id=\""
             << fname_it.first
             << "\" ";
      stream << "/>\n";
    }

    const std::string& ctx_db_name = fc4sc::global::get_coverage_db_name(cntxt);
    const std::string phys_name = !physical_name_.empty()
        ? physical_name_
        : (!ctx_db_name.empty() ? ctx_db_name : std::string("ucis_db.xml"));

    stream << "<historyNodes ";
    stream << "historyNodeId=\"" << 200 << "\" \n";
    stream << "logicalName=\""
           << "default"
           << "\" \n";
    stream << "physicalName=\""
           << escape_xml_chars(phys_name)
           << "\" \n";
    /*
    stream << "kind=\""
           << "string"
           << "\" \n";
    */
    stream << "testStatus=\""
           << "true"
           << "\" \n";
    /*
    stream << "simtime=\""
           << "1.051732E7"
           << "\" \n";
    stream << "timeunit=\""
           << "string"
           << "\" \n";
    stream << "runCwd=\""
           << "string"
           << "\" \n";
    stream << "cpuTime=\""
           << "1.051732E7"
           << "\" \n";
    stream << "seed=\""
           << "string"
           << "\" ";
    stream << "cmd=\""
           << "string"
           << "\" \n";
    stream << "args=\""
           << "string"
           << "\" ";
    stream << "compulsory=\""
           << "string"
           << "\" \n";
    */
    stream << "date=\""
           << time_now->tm_year+1900 << '-'
	   << (time_now->tm_mon + 1)/10 << (time_now->tm_mon + 1)%10 << '-'
	   << time_now->tm_mday/10 << time_now->tm_mday%10 << "T"
	   << time_now->tm_hour/10 << time_now->tm_hour%10 << ":"
	   << time_now->tm_min/10 << time_now->tm_min%10 << ":"
	   << time_now->tm_sec/10 << time_now->tm_sec%10
           << "\" \n";
      
    const char* user = getenv("USER");
    stream << "userName=\""
           << escape_xml_chars(user ? user : "")
           << "\" \n";
    /*
    stream << "cost=\""
           << "1000.00"
           << "\" \n";
    */
    stream << "toolCategory=\""
           << "UCIS:Sim"
           << "\" \n";
    stream << "ucisVersion=\""
           << "1.0"
           << "\" \n";
    stream << "vendorId=\""
           << "Accellera"
           << "\" \n";
    stream << "vendorTool=\""
           << "FC4SC"
           << "\" \n";
    stream << "vendorToolVersion=\""
           << "1.0"
           << "\" \n";
    /*
    stream << "sameTests=\""
           << "42"
           << "\" ";
    stream << "comment=\""
           << "string"
           << "\" \n";
    */
    stream << ">\n";
    stream << "</historyNodes>\n";

    init_unique_key();

    for(auto scope_inst_it : fc4sc::global::get_top_scopes(cntxt))
    {
      scope_inst_it->accept_visitor(*this);
    }
   

    stream << "</UCIS>\n";

  }

  void visit(fc4sc::scp_base_data_model& base)
  {
    stream << "<instanceCoverages ";
    stream << "name=\""
           << escape_xml_chars(base.name)
           << "\" \n";
    stream << "moduleName=\""
           << escape_xml_chars(base.type_data->type_name)
    	   << "\" \n";
    stream << "key=\"" << get_unique_key() << "\" \n";
    stream << "instanceId=\"" << base.instance_id << "\" \n";
    if(base.parent_scp != nullptr) {
      stream << "parentInstanceId=\"" << base.parent_scp->instance_id << "\" \n";
    }
    stream << ">\n";

    stream << "<id ";
    stream << "file=\"" << (base.type_data->file_id > 0 ? base.type_data->file_id : 1U) << "\" ";
    stream << "line=\"" << (base.type_data->line > 0 ? base.type_data->line : 1U) << "\" ";
    stream << "inlineCount=\""
           << "1"
           << "\" ";
    stream << "/>\n";

    // Optional toggleCoverage (UCIS.xsd sequence: after <id>, before <assertionCoverage> and <covergroupCoverage>)
    if (!base.toggle_cov.objects.empty()) {
      stream << "<toggleCoverage ";
      if (!base.toggle_cov.metric_mode.empty()) {
        stream << "metricMode=\"" << escape_xml_chars(base.toggle_cov.metric_mode) << "\" ";
      }
      stream << "weight=\"" << base.toggle_cov.weight << "\">\n";
      for (const auto& obj : base.toggle_cov.objects) {
        if (!obj || obj->bits.empty()) continue;
        stream << "<toggleObject name=\"" << escape_xml_chars(obj->name) << "\" "
               << "key=\"" << get_unique_key() << "\" ";
        if (!obj->type_name.empty()) {
          stream << "type=\"" << escape_xml_chars(obj->type_name) << "\" ";
        }
        if (!obj->port_direction.empty()) {
          stream << "portDirection=\"" << escape_xml_chars(obj->port_direction) << "\" ";
        }
        stream << "excluded=\"" << (obj->excluded ? "true" : "false") << "\">\n";
        if (obj->is_vector && obj->bit_width > 0) {
          stream << "<dimension left=\"" << (obj->bit_width - 1) << "\" right=\"0\" downto=\"true\"/>\n";
        }
        stream << "<id file=\"" << (obj->file_id > 0 ? obj->file_id : 1U)
               << "\" line=\"" << (obj->line > 0 ? obj->line : 1U)
               << "\" inlineCount=\"1\"/>\n";
        for (const auto& bit : obj->bits) {
          if (bit.toggles.empty()) continue;
          stream << "<toggleBit name=\"" << escape_xml_chars(bit.name) << "\" "
                 << "key=\"" << get_unique_key() << "\" "
                 << "excluded=\"" << (bit.excluded ? "true" : "false") << "\">\n";
          if (bit.has_index) {
            stream << "<index>" << bit.index << "</index>\n";
          }
          for (const auto& tog : bit.toggles) {
            stream << "<toggle from=\"" << escape_xml_chars(tog.from) << "\" "
                   << "to=\"" << escape_xml_chars(tog.to) << "\">\n"
                   << "<bin coverageCountGoal=\"" << tog.goal << "\" "
                   << "excluded=\"" << (tog.excluded ? "true" : "false") << "\">"
                   << "<contents coverageCount=\"" << tog.count << "\"/>"
                   << "</bin>\n"
                   << "</toggle>\n";
          }
          stream << "</toggleBit>\n";
        }
        stream << "</toggleObject>\n";
      }
      stream << "</toggleCoverage>\n";
    }

    // Optional assertionCoverage (UCIS.xsd sequence: after <toggleCoverage>, before <covergroupCoverage>)
    if (!base.assertion_cov.assertions.empty()) {
      stream << "<assertionCoverage weight=\"" << base.assertion_cov.weight << "\">\n";
      for (const auto& asrt : base.assertion_cov.assertions) {
        if (!asrt) continue;
        stream << "<assertion name=\"" << escape_xml_chars(asrt->name) << "\" "
               << "assertionKind=\"" << escape_xml_chars(asrt->assertion_kind.empty() ? std::string("assert") : asrt->assertion_kind) << "\" ";
        if (!asrt->name_component.empty()) {
          stream << "nameComponent=\"" << escape_xml_chars(asrt->name_component) << "\" ";
        }
        if (!asrt->type_component.empty()) {
          stream << "typeComponent=\"" << escape_xml_chars(asrt->type_component) << "\" ";
        }
        stream << "excluded=\"" << (asrt->excluded ? "true" : "false") << "\" "
               << "weight=\"" << asrt->weight << "\">\n";
        auto emit_bin = [&](const char* tag, bool emit, uint64_t cnt) {
          if (emit) {
            stream << "<" << tag << "><contents coverageCount=\"" << cnt << "\"/></" << tag << ">\n";
          }
        };
        emit_bin("coverBin", asrt->emit_cover_bin, asrt->cover_count);
        emit_bin("passBin", asrt->emit_pass_bin, asrt->pass_count);
        emit_bin("failBin", asrt->emit_fail_bin, asrt->fail_count);
        emit_bin("vacuousBin", asrt->emit_vacuous_bin, asrt->vacuous_count);
        emit_bin("disabledBin", asrt->emit_disabled_bin, asrt->disabled_count);
        emit_bin("attemptBin", asrt->emit_attempt_bin, asrt->attempt_count);
        emit_bin("activeBin", asrt->emit_active_bin, asrt->active_count);
        emit_bin("peakActiveBin", asrt->emit_peak_active_bin, asrt->peak_active_count);
        if (!asrt->expr_str.empty()) {
          stream << "<userAttr key=\"fc4sc.expr\" type=\"str\" len=\"" << asrt->expr_str.size() << "\">"
                 << escape_xml_chars(asrt->expr_str) << "</userAttr>\n";
        }
        stream << "</assertion>\n";
      }
      stream << "</assertionCoverage>\n";
    }

    // Each type is between an instanceCoverages tag
    for (auto &type_it : base.cvgs)
    {
      bool instance = false;
      for (size_t i = 0; i < type_it.second.size(); ++i)
      {
        if(type_it.second[i]->enable && has_coverpoints(*type_it.second[i])) {
          instance = true;
	  break;
        }
      }

      if(instance) {
        stream << "<covergroupCoverage ";
        stream << "weight=\"" << type_it.second.front()->type_data->type_option.weight << "\" ";
        stream << ">\n";

        // With strict_sv_options, the instances without per_instance are
        // written as one instance which holds the type coverage, first
        std::vector<fc4sc::cvg_base_data_model*> type_insts;
        for (auto cvg : type_it.second)
          if (cvg->enable && has_coverpoints(*cvg) && fc4sc::strict_sv_options(cvg->type_data) && !cvg->option.per_instance)
            type_insts.push_back(cvg);
        if (!type_insts.empty())
          write_type_instance(type_insts);

        // Print each instance
        for (auto cvg : type_it.second)
          if (cvg->enable && has_coverpoints(*cvg) && std::find(type_insts.begin(), type_insts.end(), cvg) == type_insts.end())
            write_cg_instance(*cvg);

        const unsigned int tgoal = type_it.second.front()->type_data->type_option.goal;
        if (tgoal != 100) {
          const std::string gstr = std::to_string(tgoal);
          stream << "<userAttr key=\"fc4sc.type_goal\" type=\"int\" len=\"" << gstr.size() << "\">"
                 << gstr << "</userAttr>\n";
        }

        stream << "</covergroupCoverage>\n";
      }
    }

    auto pct_encode_attr_field = [](const std::string& s) {
      std::string out;
      out.reserve(s.size());
      static const char hex[] = "0123456789ABCDEF";
      for (size_t k = 0; k < s.size(); ++k) {
        const unsigned char c = static_cast<unsigned char>(s[k]);
        if (c == '%' || c == '|' || c == '\n' || c == '\r') {
          out.push_back('%');
          out.push_back(hex[(c >> 4) & 0xF]);
          out.push_back(hex[c & 0xF]);
        } else {
          out.push_back(static_cast<char>(c));
        }
      }
      return out;
    };

    for (auto &type_it : base.cvgs)
    {
      // Print each disabled instance unambiguously as an fc4sc.disabled_cg userAttr
      for (size_t i = 0; i < type_it.second.size(); ++i)
      {
        if(!type_it.second[i]->enable) {
          const fc4sc::cvg_base_data_model* dcg = type_it.second[i];
          const std::string payload = "v1|" +
              pct_encode_attr_field(dcg->type_data->type_name) + "|" +
              pct_encode_attr_field(dcg->type_data->scp_type_name) + "|" +
              pct_encode_attr_field(dcg->name) + "|" +
              std::to_string(dcg->type_data->type_option.weight) + "|" +
              std::to_string(dcg->option.weight) + "|" +
              std::to_string(dcg->type_data->type_option.goal) + "|" +
              (dcg->type_data->type_option.merge_instances ? "1" : "0");
          stream << "<userAttr ";
          stream << "key=\"fc4sc.disabled_cg\" ";
          stream << "type=\"str\" ";
          stream << "len=\"" << payload.size() << "\"";
          stream << ">";
          stream << escape_xml_chars(payload);
          stream << "</userAttr>\n";
        }
      }
    }

    stream << "</instanceCoverages>\n";

    for(auto scope_inst_it : base.child_scp_insts)
    {
      scope_inst_it.second->accept_visitor(*this);
    } 
  }

  /*! Writes a covergroup instance */
  void write_cg_instance(fc4sc::cvg_base_data_model& cvg)
  {
    stream << "<cgInstance ";
    stream << "name=\"" << escape_xml_chars(cvg.name) << "\" \n";

    stream << "key=\"" << get_unique_key() << "\" \n";
    stream << "excluded=\""
           << "false"
           << "\" \n";
    stream << ">\n";

    cvg.accept_visitor(*this);

    stream << "\n";
    stream << "</cgInstance>\n";
  }

  /*! A string as it is read back from the file, once escaped */
  std::string as_written(const std::string& in)
  {
    return fc4sc::ucis_db::xml_parser("<a v=\"" + escape_xml_chars(in) + "\"/>", "").parse()->get("v");
  }

  /*!
   * \brief Writes the instances of a covergroup type which has the
   * strict_sv_options type option and which do not have the per_instance
   * option as one covergroup instance which holds the type coverage. Its
   * name is empty, so that it is told apart from every instance, which has
   * a name, and its alias is the name of the type.
   *
   *   As in SystemVerilog (IEEE 1800-2017 19.7), the data of an instance
   * without per_instance is not saved on its own, but it contributes to the
   * type coverage. The UCIS XML schema has no element for the coverage of a
   * covergroup type, which readers compute from the covergroup instances of
   * the type, so the type coverage is written as an instance whose bins
   * are the union of the bins of these instances, matched by name, with
   * their counts added up, as for type_option.merge_instances (19.11.3).
   * Its options are the type options: the weight, goal and comment of the
   * covergroup type_option and of the type_option of each coverpoint and
   * cross. Its other options are those of the first instance.
   * \param insts The instances, at least one
   */
  void write_type_instance(const std::vector<fc4sc::cvg_base_data_model*>& insts)
  {
    using fc4sc::ucis_db::xml_node;
    std::unique_ptr<xml_node> record;
    fc4sc::ucis_db::merger instances;
    for (auto cvg : insts)
    {
      std::ostringstream text;
      xml_printer printer(text);
      printer.write_cg_instance(*cvg);
      auto node = fc4sc::ucis_db::xml_parser(text.str(), "covergroup " + cvg->name).parse();
      if (!record)
        record = std::move(node);
      else
        instances.merge_instance_bins(*record, *node);
    }

    const fc4sc::cvg_metadata& type = *insts.front()->type_data;
    record->set("name", "");
    record->set("alias", as_written(type.type_name));
    xml_node* options = record->element("options");
    if (options != nullptr) {
      options->set("weight", std::to_string(type.type_option.weight));
      options->set("goal", std::to_string(type.type_option.goal));
      options->set("comment", as_written(type.type_option.comment));
    }
    xml_node* id = record->element("cgId");
    xml_node* inst_id = (id != nullptr) ? id->element("cginstSourceId") : nullptr;
    if (inst_id != nullptr) {
      inst_id->set("file", std::to_string(type.file_id));
      inst_id->set("line", std::to_string(type.line));
    }

    // the type options of the coverpoints and crosses, by their names as
    // they are written
    std::map<std::string, cvp_type_option> type_options;
    for (auto &opt : type.cvp_type_options)
      type_options[as_written(opt.first)] = opt.second;
    for (auto &child : record->children)
    {
      if (child->name != "coverpoint" && child->name != "cross")
        continue;
      xml_node* cvp_options = child->element("options");
      if (cvp_options == nullptr)
        continue;
      const cvp_type_option opt = type_options[child->get("name")];
      cvp_options->set("weight", std::to_string(opt.weight));
      cvp_options->set("goal", std::to_string(opt.goal));
      cvp_options->set("comment", as_written(opt.comment));
    }

    uint64_t key = static_cast<uint64_t>(gkey);
    fc4sc::ucis_db::merger::renumber_keys(*record, key);
    gkey = static_cast<int>(key);
    fc4sc::ucis_db::write_xml(*record, stream);
  }

  void visit(fc4sc::cvg_base_data_model& base)
  {
    base.sync_options();
    auto& inst = base.option;
    stream << "<options ";
    stream << "weight=\"" << inst.weight << "\" ";
    stream << "goal=\"" << inst.goal << "\" ";
    stream << "comment=\"" << escape_xml_chars(inst.comment) << "\" ";
    stream << "at_least=\"" << inst.at_least << "\" ";
    stream << "auto_bin_max=\"" << inst.auto_bin_max << "\" ";
    stream << "detect_overlap=\"" << inst.detect_overlap << "\" ";
    stream << "cross_num_print_missing=\"" << inst.cross_num_print_missing << "\" ";
    if(inst.per_instance) {
    stream << "per_instance=\"true\" ";
    }
    else {
      stream << "per_instance=\"false\" ";
    }
    if(base.type_data->type_option.merge_instances) {
      stream << "merge_instances=\"true\" ";
    }
    else {
      stream << "merge_instances=\"false\" ";
    }
    stream << "/>\n";

    stream << "<cgId cgName=\"" << escape_xml_chars(base.type_data->type_name) << "\" ";
    stream << "moduleName=\""
           <<  escape_xml_chars(base.type_data->scp_type_name)
           << "\">\n";

    stream << "<cginstSourceId file=\""
           << base.inst_file_id
           << "\" line=\""
           << base.inst_line
           << "\" inlineCount=\"1\"/>\n";
    stream << "<cgSourceId file=\"" << base.type_data->file_id << "\" "
           << "line=\"" 
           << base.type_data->line
           << "\""
           << " inlineCount=\"1\"/>\n";
    stream << "</cgId>\n";
          
    // UCIS CGINSTANCE requires all <coverpoint> elements before any <cross>.
    for (auto cvp : base.cvps)
      if (dynamic_cast<fc4sc::coverpoint_base_data_model*>(cvp) != nullptr)
        cvp->accept_visitor(*this);
    for (auto cvp : base.cvps)
      if (dynamic_cast<fc4sc::cross_base_data_model*>(cvp) != nullptr)
        cvp->accept_visitor(*this);


    stream << "\n";

  }

  void visit(fc4sc::coverpoint_base_data_model& base)
  {
    base.sync_options_from_parent();
    stream << "<coverpoint ";
    stream << "name=\"" << escape_xml_chars(base.name) << "\" ";
    stream << "key=\""
         << get_unique_key()
         << "\" ";
    stream << "exprString=\"" << escape_xml_chars(base.get_sample_expression_str()) << "\"";
    stream << ">\n";

    auto& inst = base.option;
    stream << "<options ";
    stream << "weight=\"" << inst.weight << "\" ";
    stream << "goal=\"" << inst.goal << "\" ";
    stream << "comment=\"" << escape_xml_chars(inst.comment) << "\" ";
    stream << "at_least=\"" << inst.at_least << "\" ";
    stream << "auto_bin_max=\"" << inst.auto_bin_max << "\" ";
    stream << "detect_overlap=\"" << inst.detect_overlap << "\" ";
    stream << "/>\n";

    coverpoint_bins_written_ = 0;
    for (auto bin : base.bins_data)
      bin->accept_visitor(*this);
    for (auto bin : base.illegal_bins_data)
      bin->accept_visitor(*this);
    for (auto bin : base.ignore_bins_data)
      bin->accept_visitor(*this);
    for (auto bin : base.default_bins_data)
      bin->accept_visitor(*this);

    // UCIS COVERPOINT requires at least one coverpointBin.
    if (coverpoint_bins_written_ == 0)
    {
      write_ignore_bin("",
                       std::vector<std::tuple<std::string, std::string, uint64_t>>(
                           1, std::make_tuple(std::string("0"), std::string("0"), uint64_t(0))));
    }

    stream << "</coverpoint>\n\n";
  }

  void visit(fc4sc::cross_base_data_model& base)
  {
    base.sync_options_from_parent();
    stream << "<cross ";
    stream << "name=\"" << escape_xml_chars(base.name) << "\" ";
    stream << "key=\""
           << get_unique_key()
           << "\" ";
    stream << ">\n";
    
    auto& inst = base.option;
    stream << "<options ";
    stream << "weight=\"" << inst.weight << "\" ";
    stream << "goal=\"" << inst.goal << "\" ";
    stream << "comment=\"" << escape_xml_chars(inst.comment) << "\" ";
    stream << "at_least=\"" << inst.at_least << "\" ";
    stream << "cross_num_print_missing=\"" << inst.cross_num_print_missing << "\" ";
    stream << "/>\n";


    for (auto &cvp : base.cross_cvps)
    {
      stream << "<crossExpr>" << escape_xml_chars(cvp->name) << "</crossExpr> \n";
    }

    // Write the bins of the cross which were hit, and the missing bins
    // (never hit) up to option.cross_num_print_missing, as in SystemVerilog.
    // A reader of the file only knows the bins found in it, so the coverage
    // it computes is too high when missing bins are left out. Define
    // FC4SC_SAVE_ALL_CROSS_BINS to write every missing bin.
    const auto& cross_bins = base.get_cross_bins();
    size_t bins_written = 0;

    auto write_bin = [&](const std::vector<size_t>& indices, uint64_t count)
    {
      bins_written++;
      // The name is made of the bin indices, as in <0,1>. Readers of UCIS
      // files tell the bins of a cross apart by their names, and add up
      // the counts of bins with the same name when files are merged.
      stream << "<crossBin \n";
      stream << "name=\"&lt;";
      for (size_t i = 0; i < indices.size(); i++)
        stream << (i == 0 ? "" : ",") << indices[i];
      stream << "&gt;\"  \n";
      stream << "key=\"" << get_unique_key() << "\" \n";
      //Cannot specify type attribute b/c URG bug
      //stream << "type=\""
      //       << "default"
      //       << "\" \n";

      stream << "> \n";

      for (auto& index : indices)
        stream << "<index>" << index << "</index>\n";

      stream << "<contents \n";
      stream << "coverageCount=\"" << count << "\"> \n";
      stream << "</contents> \n";

      stream << "</crossBin> \n";
    };

    // number of bins of each crossed coverpoint
    std::vector<size_t> sizes;
    bool has_bins = !base.cross_cvps.empty();

    for (auto &cvp : base.cross_cvps)
    {
      sizes.push_back(cvp->size());
      has_bins = has_bins && (cvp->size() != 0);
    }

    // The bins are written in the order of their indices, the last
    // coverpoint counting fastest, which is also the order of cross_bins.
    std::vector<size_t> indices(sizes.size(), 0);

#ifdef FC4SC_SAVE_ALL_CROSS_BINS
    const bool all_missing = true;
    const uint64_t missing_limit = 0;
#else
    const bool all_missing = false;
    // The schema requires at least one crossBin: when no bin was hit, the
    // first missing bin is written even if no missing bins are requested.
    uint64_t missing_limit = inst.cross_num_print_missing;
    if (cross_bins.empty() && missing_limit == 0)
      missing_limit = 1;
#endif

    // Visit the combinations while missing bins are still to be written
    uint64_t missing_written = 0;

    while (has_bins && (all_missing || missing_written < missing_limit))
    {
      auto hit = cross_bins.find(indices);

      if (hit != cross_bins.end())
        write_bin(indices, hit->second);
      else if (!base.has_custom_auto_bins() || base.is_auto_bin(indices))
      {
        // a product of bins in a user defined bin is not a bin
        write_bin(indices, 0);
        missing_written++;
      }

      // advance to the next combination, the last coverpoint counts fastest
      size_t pos = indices.size();
      while (pos > 0 && ++indices[pos - 1] == sizes[pos - 1])
      {
        indices[pos - 1] = 0;
        pos--;
      }
      has_bins = (pos != 0);
    }

    // then only the remaining bins which were hit
    if (has_bins)
    {
      for (auto it = cross_bins.lower_bound(indices); it != cross_bins.end(); it++)
        write_bin(it->first, it->second);
    }

    // The user defined bins, by name. They have the index -1, for no bin,
    // for every crossed coverpoint, as they have no single product of bins.
    // The ignore and illegal bins, and a user defined bin without products,
    // are not counted in the coverage, so they are not written: readers of
    // UCIS files may not look at the type of a crossBin, and would count
    // them.
    for (auto &bin : base.user_bins)
    {
      if (bin.bin_type == fc4sc::bin_t::default_bin_) {
        if (bin.size == 0 && bin.hits == 0)
          continue;
        bins_written++;
        stream << "<crossBin \n";
        stream << "name=\"" << escape_xml_chars(bin.name) << "\"  \n";
        stream << "key=\"" << get_unique_key() << "\" \n";
        stream << "type=\"ignore\" \n";
        stream << "> \n";

        for (size_t i = 0; i < sizes.size(); i++)
          stream << "<index>" << -1 << "</index>\n";

        stream << "<contents \n";
        stream << "coverageCount=\"" << bin.hits << "\"> \n";
        stream << "</contents> \n";

        stream << "</crossBin> \n";
        continue;
      }
      if (bin.bin_type != fc4sc::bin_t::default_ || bin.size == 0)
        continue;
      bins_written++;
      stream << "<crossBin \n";
      stream << "name=\"" << escape_xml_chars(bin.name) << "\"  \n";
      stream << "key=\"" << get_unique_key() << "\" \n";
      stream << "> \n";

      for (size_t i = 0; i < sizes.size(); i++)
        stream << "<index>" << -1 << "</index>\n";

      stream << "<contents \n";
      stream << "coverageCount=\"" << bin.hits << "\"> \n";
      stream << "</contents> \n";

      stream << "</crossBin> \n";
    }

    // A cross has no bins when one of its coverpoints has none, but the
    // schema requires at least one crossBin. Write a single ignore bin
    // instead, with the index -1, for no bin, for every crossed coverpoint.
    if (bins_written == 0)
    {
      stream << "<crossBin \n";
      stream << "name=\"&lt;";
      for (size_t i = 0; i < sizes.size(); i++)
        stream << (i == 0 ? "" : ",") << -1;
      stream << "&gt;\"  \n";
      stream << "key=\"" << get_unique_key() << "\" \n";
      stream << "type=\"ignore\" \n";
      stream << "> \n";

      for (size_t i = 0; i < sizes.size(); i++)
        stream << "<index>" << -1 << "</index>\n";

      stream << "<contents \n";
      stream << "coverageCount=\"" << 0 << "\"> \n";
      stream << "</contents> \n";

      stream << "</crossBin> \n";
    }


      stream << "</cross>\n"; 
  }

  /*!
   * \brief Writes a default bin. The type "default" of UCIS is the type of
   * the regular bins, which are counted in the coverage, so a default bin is
   * written as an ignore bin, which is not. A default bin array has a bin
   * for each value hit. A bin without values is not written, as a bin needs
   * at least one range. A bin array without hits has a bin for its first
   * value, with no hits, as a coverpoint needs at least one bin.
   */
  void write_default_bin(fc4sc::bin_base_data_model& base)
  {
    auto ranges = base.get_default_ranges();
    if (base.is_value_array()) {
      if (ranges.empty())
        ranges = base.get_unhit_array_range();
      // the values may not fit in an int, so the names use the values
      // written in decimal, which are unique
      for (auto &range : ranges)
        write_ignore_bin(base.get_name() + "[" + std::get<0>(range) + "]",
                         std::vector<std::tuple<std::string, std::string, uint64_t>>(1, range));
    }
    else if (!ranges.empty()) {
      write_ignore_bin(base.get_name(), ranges);
    }
  }

  /*!
   * \brief Writes an ignore bin of a coverpoint
   * \param name Name of the bin
   * \param ranges Its ranges, from and to in decimal, with their hit counts
   */
  void write_ignore_bin(const std::string& name,
                        const std::vector<std::tuple<std::string, std::string, uint64_t>>& ranges)
  {
    coverpoint_bins_written_++;
    stream << "<coverpointBin name=\"" << escape_xml_chars(name) << "\" \n";
    stream << "key=\"" << get_unique_key() << "\" \n";
    stream << "type=\"ignore\" >\n";
    for (auto &range : ranges)
    {
      stream << "<range \n"
             << "from=\"" << std::get<0>(range) << "\" \n"
             << "to =\"" << std::get<1>(range) << "\"\n"
             << ">\n";
      stream << "<contents "
             << "coverageCount=\"" << std::get<2>(range) << "\">";
      stream << "</contents>\n";
      stream << "</range>\n\n";
    }
    stream << "</coverpointBin>\n";
  }

  void visit(fc4sc::bin_base_data_model& base)
  {
    if (base.get_bin_type() == fc4sc::bin_t::default_bin_) {
      write_default_bin(base);
      return;
    }

    coverpoint_bins_written_++;
    stream << "<coverpointBin name=\"" << escape_xml_chars(base.get_name()) << "\" \n";
 
    stream << "key=\"" << get_unique_key() << "\" \n";

    std::string ucis_bin_type;
    switch(base.get_bin_type())
    {
      case fc4sc::bin_t::default_:
        ucis_bin_type = "default";
	break;
      case fc4sc::bin_t::illegal_:
        ucis_bin_type = "illegal";
	break;
      case fc4sc::bin_t::ignore_:
      case fc4sc::bin_t::default_bin_:
        ucis_bin_type = "ignore";
	break;
    }

    stream << "type=\""
           << ucis_bin_type 
           << "\" "
           << ">\n";

    // the values in full, as the schema type of from and to, xsd:integer,
    // has no limit
    auto bin_intervals = base.get_intervals_decimal();
    auto interval_hits = base.get_interval_hits();

    // Print each range. Coverpoint writes the header (name etc.)
    for (size_t i = 0; i < bin_intervals.size(); ++i)
    {
      stream << "<range \n"
             << "from=\"" << bin_intervals[i].first << "\" \n"
   	     << "to =\"" << bin_intervals[i].second << "\"\n"
    	     << ">\n";

      // Print hits for each range
      stream << "<contents "
             << "coverageCount=\"" << interval_hits[i] << "\">";
      stream << "</contents>\n";
      stream << "</range>\n\n";
    }

    // Print each sequence of a transition bin
    auto bin_sequences = base.get_sequences_decimal();
    for (size_t i = 0; i < bin_sequences.size(); ++i)
    {
      stream << "<sequence>\n";
      stream << "<contents "
             << "coverageCount=\"" << interval_hits[i] << "\">";
      stream << "</contents>\n";
      for (auto &value : bin_sequences[i])
        stream << "<seqValue>" << value << "</seqValue>\n";
      stream << "</sequence>\n\n";
    }

    for (const auto& attr : base.get_user_attrs())
    {
      stream << "<userAttr key=\"" << escape_xml_chars(attr.key)
             << "\" type=\"" << escape_xml_chars(attr.type) << "\">"
             << escape_xml_chars(attr.value) << "</userAttr>\n";
    }

    stream << "</coverpointBin>\n";

  }

  /*!
   * \brief Prints data to the given file name
   * \param name Where to print. When it is empty, the name set by
   * fc4sc::global::set_coverage_db_name() is used, and nothing is printed if
   * there is none
   */
  static void coverage_save(const std::string &name = "", fc4sc::global* cntxt = fc4sc::global::getter(), const fc4sc_format how = fc4sc_format::ucis_xml)
  {
    const std::string file_name = name.empty() ? fc4sc::global::get_coverage_db_name(cntxt) : name;
    if (file_name.empty()) {
      std::cerr << "FC4SC " << __FUNCTION__ << ": Error! Function was passed "
	  "empty string as the file name\n";
      std::cerr << "COVERAGE DB WAS NOT BE SAVED!" << std::endl;
      return;
    }

    std::ofstream file(file_name);
    if (!file) {
      std::cerr << "FC4SC " << __FUNCTION__ << ": Error! Could not open file ["
        << file_name << "] for writing!" << std::endl;
      std::cerr << "COVERAGE DB WAS NOT BE SAVED!" << std::endl;
      return;
    }
    xml_printer printer(file, file_name);
    switch(how) {
      case fc4sc_format::ucis_xml:
        printer.print_data_xml(cntxt);
        break;
      default :
	break;
    }
  }

  /*!
   * \brief Prints data to the given std::stream object.
   * \param stream object where to print.
   */
  static void coverage_save(std::ofstream& stream, fc4sc::global* cntxt = fc4sc::global::getter(), const fc4sc_format how = fc4sc_format::ucis_xml)
  {
    xml_printer printer(stream);
    switch(how) {
      case fc4sc_format::ucis_xml:
        printer.print_data_xml(cntxt);
        break;
      default :
	break;
    }
  }

  /*!
   *  \brief Returns the length of the UTF-8 encoded character which starts
   *  at a byte of a string, or 0 if the bytes there are not a character
   *  which XML 1.0 allows, encoded as UTF-8 allows
   *
   *  \param in The string
   *  \param idx Index of the first byte, which is not an ASCII character
   */
  static size_t utf8_char_length(const std::string &in, std::string::size_type idx) {
    const unsigned char lead = static_cast<unsigned char>(in[idx]);
    size_t length;
    // the range of the second byte, which excludes the overlong encodings,
    // the surrogates and the values above U+10FFFF
    unsigned char low = 0x80, high = 0xBF;
    if (lead >= 0xC2 && lead <= 0xDF) length = 2;
    else if (lead >= 0xE0 && lead <= 0xEF) {
      length = 3;
      if (lead == 0xE0) low = 0xA0;
      if (lead == 0xED) high = 0x9F;
    }
    else if (lead >= 0xF0 && lead <= 0xF4) {
      length = 4;
      if (lead == 0xF0) low = 0x90;
      if (lead == 0xF4) high = 0x8F;
    }
    else return 0;
    if (in.length() - idx < length) return 0;
    for (size_t k = 1; k < length; ++k) {
      const unsigned char c = static_cast<unsigned char>(in[idx + k]);
      if (c < (k == 1 ? low : 0x80) || c > (k == 1 ? high : 0xBF)) return 0;
    }
    // U+FFFE and U+FFFF are not allowed in XML 1.0
    if (lead == 0xEF && static_cast<unsigned char>(in[idx + 1]) == 0xBF &&
        static_cast<unsigned char>(in[idx + 2]) >= 0xBE) return 0;
    return length;
  }

  /*!
   *  \brief Function which returns a string where all XML special characters are escaped,
   *  and the control characters which XML 1.0 does not allow are replaced with '?'.
   *  The file is encoded as UTF-8: each byte which is not part of a UTF-8 encoded
   *  character, as the bytes of a Latin-1 string, is also replaced with '?'.
   *
   *  \param in The input string
   */
  std::string escape_xml_chars(const std::string &in) {
    std::string out;

    for (std::string::size_type idx = 0; idx < in.length(); idx++) {
      switch(in[idx]) {
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '&': out += "&amp;"; break;
        case '\"': out += "&quot;"; break;
        case '\'': out += "&apos;"; break;
        // white space is written as references, so that it is kept in
        // attribute values
        case '\t': out += "&#9;"; break;
        case '\n': out += "&#10;"; break;
        case '\r': out += "&#13;"; break;
        default: {
          const unsigned char c = static_cast<unsigned char>(in[idx]);
          // the other control characters are not allowed in XML 1.0
          if (c < 0x20)
            out += '?';
          else if (c < 0x80)
            out += in[idx];
          else {
            const size_t length = utf8_char_length(in, idx);
            if (length == 0)
              out += '?';
            else {
              out.append(in, idx, length);
              idx += length - 1;
            }
          }
        }
      }
    }
    return out;
  }

};

#endif
