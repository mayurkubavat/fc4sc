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

#include "fc4sc_base.hpp"

typedef enum fc4sc_format {
  ucis_xml = 1
} fc4sc_format;

class xml_printer : public fc4sc::covVisitorBase {
    
  /*! key generation for UCIS XML */
  int gkey = 1;

  /*! output stream for xml file */
  std::ofstream& stream;

public:

  xml_printer(std::ofstream& out_stream) : stream(out_stream) {}

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
    stream << " writtenBy=\""
           << getenv("USER")
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

    for (auto &fname_it : fc4sc::global::get_file_id_to_name_table(cntxt))
    {
      stream << "<sourceFiles ";
      stream << " fileName=\""
	     << fname_it.second
	     << "\"";
      stream << " id=\""
             << fname_it.first
             << "\" ";
      stream << "/>\n";
    }

    //TODO: Needed information but not filled
    stream << "<historyNodes ";
    stream << "historyNodeId=\"" << 200 << "\" \n";
    stream << "logicalName=\""
           << "string"
           << "\" \n";
    stream << "physicalName=\""
           << "string"
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
      
    stream << "userName=\""
           << getenv("USER")
           << "\" \n";
    /*
    stream << "cost=\""
           << "1000.00"
           << "\" \n";
    */
    stream << "toolCategory=\""
           << "string"
           << "\" \n";
    stream << "ucisVersion=\""
           << "1.0"
           << "\" \n";
    stream << "vendorId=\""
           << "string"
           << "\" \n";
    stream << "vendorTool=\""
           << "string"
           << "\" \n";
    stream << "vendorToolVersion=\""
           << "string"
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
           << base.name
           << "\" \n";
    stream << "moduleName=\""
           << base.type_data->type_name
    	   << "\" \n";
    stream << "key=\"" << get_unique_key() << "\" \n";
    stream << "instanceId=\"" << base.instance_id << "\" \n";
    if(base.parent_scp != nullptr) {
      stream << "parentInstanceId=\"" << base.parent_scp->instance_id << "\" \n";
    }
    stream << ">\n";

    stream << "<id ";
    stream << "file=\"" << base.type_data->file_id << "\" ";
    stream << "line=\"" << base.type_data->line << "\" ";
    stream << "inlineCount=\""
           << "1"
           << "\" ";
    stream << "/>\n";

    // Each type is between an instanceCoverages tag
    for (auto &type_it : base.cvgs)
    {
      bool instance = false;
      for (size_t i = 0; i < type_it.second.size(); ++i)
      {
        if(type_it.second[i]->enable) {
          instance = true;
	  break;
        }
      }

      if(instance) {
        stream << "<covergroupCoverage ";
        stream << "weight=\"" << type_it.second.front()->type_data->type_option.weight << "\" ";
        stream << ">\n";
      

      // Print each instance
      for (size_t i = 0; i < type_it.second.size(); ++i)
      {
        if(type_it.second[i]->enable) {
          stream << "<cgInstance ";
          stream << "name=\"" << type_it.second[i]->name << "\" \n";

          stream << "key=\"" << get_unique_key() << "\" \n";
          stream << "excluded=\""
                 << "false"
                 << "\" \n";
          stream << ">\n";

          type_it.second[i]->accept_visitor(*this);

          stream << "\n";
          stream << "</cgInstance>\n";
        }
      }


        stream << "</covergroupCoverage>\n";
      }
    }

    for (auto &type_it : base.cvgs)
    {
      // Print each instance
      for (size_t i = 0; i < type_it.second.size(); ++i)
      {
        if(!type_it.second[i]->enable) {
          stream << "<userAttr ";
  	  stream << "key=\"" << type_it.second[i]->type_data->type_name << "\" \n";
          stream << "type=\"str\"\n";
	  stream << "len=\"" << type_it.second[i]->type_data->type_name.size() << "\"\n";
	  stream << ">\n";
	  stream << type_it.second[i]->type_data->type_name;
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

  void visit(fc4sc::cvg_base_data_model& base)
  {
    auto& inst = base.option;
    stream << "<options ";
    stream << "weight=\"" << inst.weight << "\" ";
    stream << "goal=\"" << inst.goal << "\" ";
    stream << "comment=\"" << inst.comment << "\" ";
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
           <<  base.type_data->scp_type_name
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
          
    // Print coverpoints
    for (auto cvp : base.cvps)
      cvp->accept_visitor(*this);


    stream << "\n";

  }

  void visit(fc4sc::coverpoint_base_data_model& base)
  {
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
    stream << "comment=\"" << inst.comment << "\" ";
    stream << "at_least=\"" << inst.at_least << "\" ";
    stream << "auto_bin_max=\"" << inst.auto_bin_max << "\" ";
    stream << "detect_overlap=\"" << inst.detect_overlap << "\" ";
    stream << "/>\n";

    for (auto bin : base.bins_data)
      bin->accept_visitor(*this);
    for (auto bin : base.illegal_bins_data)
      bin->accept_visitor(*this);
    for (auto bin : base.ignore_bins_data)
      bin->accept_visitor(*this);

    stream << "</coverpoint>\n\n";
  }

  void visit(fc4sc::cross_base_data_model& base)
  {
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
    stream << "comment=\"" << inst.comment << "\" ";
    stream << "at_least=\"" << inst.at_least << "\" ";
    stream << "cross_num_print_missing=\"" << inst.cross_num_print_missing << "\" ";
    stream << "/>\n";


    for (auto &cvp : base.cross_cvps)
    {
      stream << "<crossExpr>" << cvp->name << "</crossExpr> \n";
    }

    // Write the bins of the cross which were hit, and the missing bins
    // (never hit) up to option.cross_num_print_missing, as in SystemVerilog.
    // A reader of the file only knows the bins found in it, so the coverage
    // it computes is too high when missing bins are left out. Define
    // FC4SC_SAVE_ALL_CROSS_BINS to write every missing bin.
    const auto& cross_bins = base.get_cross_bins();

    auto write_bin = [&](const std::vector<size_t>& indices, uint64_t count)
    {
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

    // A cross has no bins when one of its coverpoints has none, but the
    // schema requires at least one crossBin. Write a single ignore bin
    // instead, with the index -1, for no bin, for every crossed coverpoint.
    if (!has_bins)
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
      else
      {
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

      stream << "</cross>\n"; 
  }

  void visit(fc4sc::bin_base_data_model& base)
  {
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
        ucis_bin_type = "ignore";
	break;
    }

    stream << "type=\""
           << ucis_bin_type 
           << "\" "
           << ">\n";

    auto bin_intervals = base.get_intervals_to_int();
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
    auto bin_sequences = base.get_sequences_to_int();
    for (size_t i = 0; i < bin_sequences.size(); ++i)
    {
      stream << "<sequence>\n";
      stream << "<contents "
             << "coverageCount=\"" << interval_hits[i] << "\">";
      stream << "</contents>\n";
      for (auto value : bin_sequences[i])
        stream << "<seqValue>" << value << "</seqValue>\n";
      stream << "</sequence>\n\n";
    }

    stream << "</coverpointBin>\n";

  }

  /*!
   * \brief Prints data to the given file name
   * \param file_name Where to print. Returns if empty
   */
  static void coverage_save(const std::string &file_name = "", fc4sc::global* cntxt = fc4sc::global::getter(), const fc4sc_format how = fc4sc_format::ucis_xml)
  {
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
    coverage_save(file, cntxt, how);
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
   *  \brief Function which returns a string where all XML special characters are escaped.
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
        default: out += in[idx];
      }
    }
    return out;
  }

};

#endif
