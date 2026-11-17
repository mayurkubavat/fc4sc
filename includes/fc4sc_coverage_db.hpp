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
 \file fc4sc_coverage_db.hpp
 \brief Loading and merging of the UCIS XML files written by FC4SC

 The SystemVerilog $load_coverage_db (IEEE 1800-2017 19.9) is
 \link fc4sc::load_coverage_db \endlink, which adds the hit counts of a
 file to the coverage model, and \link fc4sc::merge_coverage_db \endlink
 merges several files into one, as the files of runs with different seeds.
 The file name of $set_coverage_db_name is set with
 fc4sc::global::set_coverage_db_name(), and xml_printer::coverage_save()
 writes to it when it is given no file name.

 The files are read by a small XML reader, which reads the XML written by
 FC4SC: elements, attributes, text, character and entity references,
 comments, processing instructions and CDATA sections. A DOCTYPE is not
 supported. The reader throws std::runtime_error, with the file name and
 the line, for a file which is not well formed.
 */

#ifndef FC4SC_COVERAGE_DB_HPP
#define FC4SC_COVERAGE_DB_HPP

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "fc4sc_base.hpp"
#include "fc4sc_master.hpp"

namespace fc4sc
{
namespace ucis_db
{

/*!
 * \brief An element of an XML document
 */
class xml_node
{
public:

  /*! Name of the element */
  std::string name;

  /*! Attributes, in the order of the file */
  std::vector<std::pair<std::string, std::string>> attributes;

  /*! Child elements, in the order of the file */
  std::vector<std::unique_ptr<xml_node>> children;

  /*! Text of the element, without the text of its children */
  std::string text;

  /*! Line of the element in its file */
  size_t line = 0;

  /*! Returns the value of an attribute, or nullptr */
  const std::string* attribute(const std::string& key) const
  {
    for (auto &attr : attributes)
      if (attr.first == key)
        return &attr.second;
    return nullptr;
  }

  /*! Returns the value of an attribute, or an empty string */
  std::string get(const std::string& key) const
  {
    const std::string* value = attribute(key);
    return value ? *value : std::string();
  }

  /*! Sets the value of an attribute */
  void set(const std::string& key, const std::string& value)
  {
    for (auto &attr : attributes)
      if (attr.first == key) {
        attr.second = value;
        return;
      }
    attributes.push_back(std::make_pair(key, value));
  }

  /*! Removes an attribute */
  void erase(const std::string& key)
  {
    for (auto it = attributes.begin(); it != attributes.end(); ++it)
      if (it->first == key) {
        attributes.erase(it);
        return;
      }
  }

  /*! Returns the child elements with a name */
  std::vector<xml_node*> elements(const std::string& child_name) const
  {
    std::vector<xml_node*> found;
    for (auto &child : children)
      if (child->name == child_name)
        found.push_back(child.get());
    return found;
  }

  /*! Returns the first child element with a name, or nullptr */
  xml_node* element(const std::string& child_name) const
  {
    for (auto &child : children)
      if (child->name == child_name)
        return child.get();
    return nullptr;
  }

  /*! Returns a deep copy of this element */
  std::unique_ptr<xml_node> clone() const
  {
    std::unique_ptr<xml_node> copy(new xml_node);
    copy->name = name;
    copy->attributes = attributes;
    copy->text = text;
    copy->line = line;
    for (auto &child : children)
      copy->children.push_back(child->clone());
    return copy;
  }
};

/*!
 * \brief Reads an XML document
 */
class xml_parser
{
public:

  /*!
   * \param input The document
   * \param source Name of the document, for the error messages
   */
  xml_parser(const std::string& input, const std::string& source) : in(input), src(source) {}

  /*!
   * \brief Reads the document
   * \returns Its root element
   */
  std::unique_ptr<xml_node> parse()
  {
    pos = 0;
    line = 1;
    // a byte order mark
    if (in.compare(0, 3, "\xEF\xBB\xBF") == 0) pos = 3;
    skip_misc(true);
    if (pos >= in.size() || in[pos] != '<')
      error("expected the root element");
    std::unique_ptr<xml_node> root = parse_element(0);
    skip_misc(false);
    if (pos != in.size())
      error("unexpected content after the root element");
    return root;
  }

private:

  const std::string& in;
  std::string src;
  size_t pos = 0;
  size_t line = 1;

  /*! Elements nested deeper are refused, so that the recursion is bounded */
  static const int max_depth = 256;

  void error(const std::string& message) const
  {
    throw std::runtime_error(src + ":" + std::to_string(line) + ": " + message);
  }

  bool starts(const char* s) const
  {
    return in.compare(pos, std::strlen(s), s) == 0;
  }

  void advance(size_t n)
  {
    for (size_t i = 0; i < n && pos < in.size(); ++i, ++pos)
      if (in[pos] == '\n') line++;
  }

  static bool is_space(char c)
  {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
  }

  void skip_space()
  {
    while (pos < in.size() && is_space(in[pos])) advance(1);
  }

  /*! Skips past the end marker, which must be found */
  void skip_past(const char* end, const char* what)
  {
    size_t found = in.find(end, pos);
    if (found == std::string::npos)
      error(std::string("unterminated ") + what);
    advance(found + std::strlen(end) - pos);
  }

  /*! Skips white space, comments and processing instructions */
  void skip_misc(bool prolog)
  {
    while (true)
    {
      skip_space();
      if (starts("<?"))
        skip_past("?>", "processing instruction");
      else if (starts("<!--"))
        skip_past("-->", "comment");
      else if (prolog && starts("<!DOCTYPE"))
        error("DOCTYPE is not supported");
      else
        return;
    }
  }

  static bool is_name_char(char c)
  {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == ':' ||
           c == '-' || c == '.' || (static_cast<unsigned char>(c) >= 0x80);
  }

  std::string parse_name()
  {
    size_t start = pos;
    if (pos < in.size() && (std::isdigit(static_cast<unsigned char>(in[pos])) || in[pos] == '-' || in[pos] == '.'))
      error("expected a name");
    while (pos < in.size() && is_name_char(in[pos])) advance(1);
    if (start == pos)
      error("expected a name");
    return in.substr(start, pos - start);
  }

  void expect(char c)
  {
    if (pos >= in.size() || in[pos] != c)
      error(std::string("expected '") + c + "'");
    advance(1);
  }

  static void append_utf8(std::string& out, unsigned long cp)
  {
    if (cp < 0x80) {
      out += static_cast<char>(cp);
    }
    else if (cp < 0x800) {
      out += static_cast<char>(0xC0 | (cp >> 6));
      out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    else if (cp < 0x10000) {
      out += static_cast<char>(0xE0 | (cp >> 12));
      out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    else {
      out += static_cast<char>(0xF0 | (cp >> 18));
      out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
      out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      out += static_cast<char>(0x80 | (cp & 0x3F));
    }
  }

  /*! Reads a character or entity reference, at '&' */
  void parse_reference(std::string& out)
  {
    size_t end = in.find(';', pos);
    if (end == std::string::npos || end - pos > 12)
      error("bad reference");
    const std::string ref = in.substr(pos + 1, end - pos - 1);
    if (ref == "lt") out += '<';
    else if (ref == "gt") out += '>';
    else if (ref == "amp") out += '&';
    else if (ref == "quot") out += '"';
    else if (ref == "apos") out += '\'';
    else if (ref.size() > 1 && ref[0] == '#') {
      const bool hex = (ref[1] == 'x');
      const std::string digits = ref.substr(hex ? 2 : 1);
      if (digits.empty() ||
          digits.find_first_not_of(hex ? "0123456789abcdefABCDEF" : "0123456789") != std::string::npos)
        error("bad character reference &" + ref + ";");
      const unsigned long cp = std::strtoul(digits.c_str(), nullptr, hex ? 16 : 10);
      if (cp == 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
        error("bad character reference &" + ref + ";");
      append_utf8(out, cp);
    }
    else {
      error("unknown entity &" + ref + ";");
    }
    advance(end + 1 - pos);
  }

  std::unique_ptr<xml_node> parse_element(int depth)
  {
    if (depth > max_depth)
      error("elements are nested too deeply");
    std::unique_ptr<xml_node> node(new xml_node);
    node->line = line;
    expect('<');
    node->name = parse_name();

    // attributes
    while (true)
    {
      const size_t before = pos;
      skip_space();
      if (pos >= in.size())
        error("unterminated element <" + node->name + ">");
      if (starts("/>")) {
        advance(2);
        return node;
      }
      if (in[pos] == '>') {
        advance(1);
        break;
      }
      if (pos == before)
        error("expected white space before an attribute");
      std::string key = parse_name();
      skip_space();
      expect('=');
      skip_space();
      if (pos >= in.size() || (in[pos] != '"' && in[pos] != '\''))
        error("expected a quoted value for attribute " + key);
      const char quote = in[pos];
      advance(1);
      std::string value;
      while (true)
      {
        if (pos >= in.size())
          error("unterminated value of attribute " + key);
        const char c = in[pos];
        if (c == quote) {
          advance(1);
          break;
        }
        if (c == '<')
          error("'<' in the value of attribute " + key);
        if (c == '&')
          parse_reference(value);
        else {
          value += c;
          advance(1);
        }
      }
      if (node->attribute(key) != nullptr)
        error("duplicate attribute " + key);
      node->attributes.push_back(std::make_pair(key, value));
    }

    // content
    while (true)
    {
      if (pos >= in.size())
        error("unterminated element <" + node->name + ">");
      if (starts("</")) {
        advance(2);
        if (parse_name() != node->name)
          error("mismatched end tag of element <" + node->name + ">");
        skip_space();
        expect('>');
        return node;
      }
      if (starts("<!--")) {
        skip_past("-->", "comment");
      }
      else if (starts("<![CDATA[")) {
        advance(9);
        size_t end = in.find("]]>", pos);
        if (end == std::string::npos)
          error("unterminated CDATA section");
        node->text += in.substr(pos, end - pos);
        advance(end + 3 - pos);
      }
      else if (starts("<?")) {
        skip_past("?>", "processing instruction");
      }
      else if (in[pos] == '<') {
        node->children.push_back(parse_element(depth + 1));
      }
      else if (in[pos] == '&') {
        parse_reference(node->text);
      }
      else {
        node->text += in[pos];
        advance(1);
      }
    }
  }
};

/*!
 * \brief Reads an XML file
 * \returns Its root element
 *
 * Throws std::runtime_error if the file cannot be read or is not well formed.
 */
inline std::unique_ptr<xml_node> read_xml_file(const std::string& file_name)
{
  std::ifstream file(file_name, std::ios::in | std::ios::binary);
  if (!file)
    throw std::runtime_error("Could not open file [" + file_name + "] for reading");
  std::stringstream content;
  content << file.rdbuf();
  return xml_parser(content.str(), file_name).parse();
}

/*! Escapes the XML special characters and the control characters */
inline std::string escape(const std::string& in)
{
  std::string out;
  for (char c : in)
  {
    switch (c)
    {
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '&': out += "&amp;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&apos;"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20)
          out += "&#" + std::to_string(static_cast<int>(c)) + ";";
        else
          out += c;
    }
  }
  return out;
}

/*! Writes an element and its children */
inline void write_xml(const xml_node& node, std::ostream& out)
{
  out << "<" << node.name;
  for (auto &attr : node.attributes)
    out << " " << attr.first << "=\"" << escape(attr.second) << "\"";
  if (node.children.empty()) {
    if (node.text.empty())
      out << "/>\n";
    else
      out << ">" << escape(node.text) << "</" << node.name << ">\n";
    return;
  }
  out << ">\n";
  for (auto &child : node.children)
    write_xml(*child, out);
  out << "</" << node.name << ">\n";
}

/*!
 * \brief Writes an XML document
 * \param root Its root element
 * \param file_name Where to write
 *
 * Throws std::runtime_error if the file cannot be written.
 */
inline void write_xml_file(const xml_node& root, const std::string& file_name)
{
  std::ofstream file(file_name);
  if (!file)
    throw std::runtime_error("Could not open file [" + file_name + "] for writing");
  file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  write_xml(root, file);
  if (!file)
    throw std::runtime_error("Could not write file [" + file_name + "]");
}

/*!
 * \brief Reads a non-negative integer, as a coverage count
 * \param text The integer, with optional white space around it
 * \param value Returns the integer
 * \returns false if the text is not an integer which fits in 64 bits
 */
inline bool parse_uint(const std::string& text, uint64_t& value)
{
  size_t first = text.find_first_not_of(" \t\r\n");
  size_t last = text.find_last_not_of(" \t\r\n");
  if (first == std::string::npos) return false;
  std::string digits = text.substr(first, last - first + 1);
  if (digits[0] == '+') digits = digits.substr(1);
  if (digits.empty() || digits.find_first_not_of("0123456789") != std::string::npos)
    return false;
  errno = 0;
  unsigned long long v = std::strtoull(digits.c_str(), nullptr, 10);
  if (errno == ERANGE || v > std::numeric_limits<uint64_t>::max()) return false;
  value = static_cast<uint64_t>(v);
  return true;
}

/*!
 * \brief Reads an integer, as a value of a sequence or an index of a cross bin
 * \returns false if the text is not an integer which fits in a long long
 */
inline bool parse_int(const std::string& text, long long& value)
{
  size_t first = text.find_first_not_of(" \t\r\n");
  size_t last = text.find_last_not_of(" \t\r\n");
  if (first == std::string::npos) return false;
  const std::string digits = text.substr(first, last - first + 1);
  const size_t start = (digits[0] == '-' || digits[0] == '+') ? 1 : 0;
  if (start == digits.size() || digits.find_first_not_of("0123456789", start) != std::string::npos)
    return false;
  errno = 0;
  value = std::strtoll(digits.c_str(), nullptr, 10);
  return errno != ERANGE;
}

/*! Adds without overflow: the sum stops at the largest count */
inline uint64_t add_saturated(uint64_t a, uint64_t b)
{
  return (a > std::numeric_limits<uint64_t>::max() - b) ? std::numeric_limits<uint64_t>::max() : a + b;
}

/*!
 * \brief Returns the coverage count of a range, sequence or cross bin,
 * from its contents element
 *
 * Throws std::runtime_error if there is no valid count.
 */
inline uint64_t coverage_count(const xml_node& node)
{
  const xml_node* contents = node.element("contents");
  uint64_t count = 0;
  if (contents == nullptr || !parse_uint(contents->get("coverageCount"), count))
    throw std::runtime_error("Element <" + node.name + "> of line " + std::to_string(node.line) +
                             " has no valid coverage count");
  return count;
}

/*! Adds a coverage count to the contents of a range, sequence or cross bin */
inline void add_coverage_count(xml_node& node, uint64_t count)
{
  xml_node* contents = node.element("contents");
  contents->set("coverageCount", std::to_string(add_saturated(coverage_count(node), count)));
}

/*! The integers of the child elements with a name, as the seqValue of a sequence */
inline std::vector<long long> int_list(const xml_node& node, const std::string& child_name)
{
  std::vector<long long> values;
  for (auto child : node.elements(child_name))
  {
    long long v = 0;
    if (!parse_int(child->text, v))
      throw std::runtime_error("Element <" + child_name + "> of line " + std::to_string(child->line) +
                               " is not an integer");
    values.push_back(v);
  }
  return values;
}

/*!
 * \brief Inserts an element among the children of another, after the
 * children which come before it in the UCIS schema
 */
inline void insert_child(xml_node& parent, std::unique_ptr<xml_node> child)
{
  // the order of the elements in their parents, in the UCIS schema
  static const char* order[] = {
    "sourceFiles", "historyNodes", "instanceCoverages", "designParameter", "id",
    "covergroupCoverage", "cgInstance", "options", "cgId", "cgParms", "coverpoint",
    "cross", "crossExpr", "crossBin", "coverpointBin", "range", "sequence", "index",
    "contents", "seqValue"
  };
  const size_t count = sizeof(order) / sizeof(order[0]);
  auto rank = [&](const std::string& name) {
    for (size_t i = 0; i < count; ++i)
      if (name == order[i]) return i;
    // userAttr and unknown elements come last
    return count;
  };
  const size_t child_rank = rank(child->name);
  size_t at = parent.children.size();
  while (at > 0 && rank(parent.children[at - 1]->name) > child_rank) at--;
  parent.children.insert(parent.children.begin() + at, std::move(child));
}

/*!
 * \brief Returns the path of each scope instance (instanceCoverages) of a
 * document: the names and types of its parents and itself, as
 * "/top(top_type)/child(child_type)"
 */
inline std::map<xml_node*, std::string> instance_paths(const xml_node& root)
{
  std::map<std::string, xml_node*> by_id;
  for (auto inst : root.elements("instanceCoverages"))
    by_id[inst->get("instanceId")] = inst;
  std::map<xml_node*, std::string> paths;
  for (auto inst : root.elements("instanceCoverages"))
  {
    std::string path;
    const xml_node* node = inst;
    // a loop of parents is cut after as many steps as there are instances
    for (size_t steps = 0; node != nullptr && steps <= by_id.size(); ++steps)
    {
      path = "/" + node->get("name") + "(" + node->get("moduleName") + ")" + path;
      const std::string* parent = node->attribute("parentInstanceId");
      auto it = (parent != nullptr) ? by_id.find(*parent) : by_id.end();
      node = (it != by_id.end() && it->second != node) ? it->second : nullptr;
    }
    paths[inst] = path;
  }
  return paths;
}

/*!
 * \brief The children of an element by a key, each key with its children
 * in the order of the file, so that the n-th child with a key of a file is
 * merged with the n-th child with that key of the other
 */
class keyed_children
{
public:
  std::map<std::string, std::vector<xml_node*>> by_key;
  std::map<std::string, size_t> used;

  template <typename F>
  keyed_children(const xml_node& parent, const std::string& child_name, F key)
  {
    for (auto child : parent.elements(child_name))
      by_key[key(*child)].push_back(child);
  }

  /*! Returns the next child with the key which was not matched yet, or nullptr */
  xml_node* match(const std::string& key)
  {
    auto it = by_key.find(key);
    if (it == by_key.end()) return nullptr;
    size_t& n = used[key];
    if (n >= it->second.size()) return nullptr;
    return it->second[n++];
  }

  void add(const std::string& key, xml_node* node)
  {
    by_key[key].push_back(node);
    used[key] = by_key[key].size();
  }
};

/*!
 * \brief Merges the UCIS XML document written by FC4SC into another
 *
 *   The scope instances (instanceCoverages) are matched by their name, their
 * type (moduleName) and their parent, the covergroup instances by their type
 * (cgName) and their name, coverpoints and crosses by their name, the bins of
 * a coverpoint by their name and type, their ranges by their limits and
 * their sequences by their values, and the bins of a cross by their name.
 * The coverage counts of matching elements are added, and the elements of
 * the other document which match none are added to this one. The options
 * of the elements of this document are kept. The source files are matched
 * by their names, and the history nodes of the other document are added.
 */
class merger
{
public:

  /*!
   * \brief Merges a document into another
   * \param into The document which receives the counts
   * \param from The document whose counts are added
   *
   * Throws std::runtime_error if a document is not a UCIS document or has
   * an invalid coverage count.
   */
  void merge(xml_node& into, const xml_node& from)
  {
    if (into.name != "UCIS" || from.name != "UCIS")
      throw std::runtime_error("The root element of a UCIS XML file must be <UCIS>");
    file_ids.clear();
    instance_ids.clear();
    merge_source_files(into, from);
    merge_history_nodes(into, from);
    merge_instances(into, from);
  }

  /*!
   * \brief Numbers the key attributes of all the elements from 1, as they
   * are written by FC4SC, so that they stay unique in a merged document
   */
  static void renumber_keys(xml_node& node, uint64_t& next)
  {
    // the key of a userAttr is its name
    if (node.name != "userAttr" && node.attribute("key") != nullptr)
      node.set("key", std::to_string(next++));
    for (auto &child : node.children)
      renumber_keys(*child, next);
  }

private:

  /*! The ids of the source files and instances of from, in into */
  std::map<std::string, std::string> file_ids;
  std::map<std::string, std::string> instance_ids;

  static uint64_t max_id(const xml_node& parent, const std::string& child_name,
                         const std::string& attr)
  {
    uint64_t max = 0;
    for (auto child : parent.elements(child_name))
    {
      uint64_t id = 0;
      if (parse_uint(child->get(attr), id) && id > max) max = id;
    }
    return max;
  }

  void merge_source_files(xml_node& into, const xml_node& from)
  {
    std::map<std::string, std::string> ids;
    for (auto file : into.elements("sourceFiles"))
      ids[file->get("fileName")] = file->get("id");
    uint64_t next = max_id(into, "sourceFiles", "id") + 1;
    for (auto file : from.elements("sourceFiles"))
    {
      const std::string name = file->get("fileName");
      auto it = ids.find(name);
      if (it == ids.end()) {
        std::unique_ptr<xml_node> copy = file->clone();
        copy->set("id", std::to_string(next++));
        it = ids.insert(std::make_pair(name, copy->get("id"))).first;
        insert_child(into, std::move(copy));
      }
      file_ids[file->get("id")] = it->second;
    }
  }

  void merge_history_nodes(xml_node& into, const xml_node& from)
  {
    uint64_t next = max_id(into, "historyNodes", "historyNodeId") + 1;
    for (auto node : from.elements("historyNodes"))
    {
      std::unique_ptr<xml_node> copy = node->clone();
      copy->set("historyNodeId", std::to_string(next++));
      copy->erase("parentId");
      insert_child(into, std::move(copy));
    }
  }

  /*! Gives the file ids of an element of from, and of its children, their values in into */
  void map_file_ids(xml_node& node)
  {
    if (node.name == "id" || node.name == "cginstSourceId" || node.name == "cgSourceId") {
      auto it = file_ids.find(node.get("file"));
      if (it != file_ids.end()) node.set("file", it->second);
    }
    for (auto &child : node.children)
      map_file_ids(*child);
  }

  void merge_instances(xml_node& into, const xml_node& from)
  {
    std::map<std::string, xml_node*> into_by_path;
    for (auto &p : instance_paths(into))
      into_by_path[p.second] = p.first;
    uint64_t next = max_id(into, "instanceCoverages", "instanceId") + 1;

    // the parents first, as they have shorter paths
    auto from_paths = instance_paths(from);
    std::vector<std::pair<size_t, xml_node*>> order;
    for (auto inst : from.elements("instanceCoverages"))
      order.push_back(std::make_pair(std::count(from_paths[inst].begin(), from_paths[inst].end(), '/'), inst));
    std::stable_sort(order.begin(), order.end(),
      [](const std::pair<size_t, xml_node*>& a, const std::pair<size_t, xml_node*>& b) {
        return a.first < b.first;
      });

    for (auto &entry : order)
    {
      const xml_node& inst = *entry.second;
      const std::string& path = from_paths[entry.second];
      auto it = into_by_path.find(path);
      if (it != into_by_path.end()) {
        instance_ids[inst.get("instanceId")] = it->second->get("instanceId");
        merge_instance(*it->second, inst);
        continue;
      }
      std::unique_ptr<xml_node> copy = inst.clone();
      map_file_ids(*copy);
      copy->set("instanceId", std::to_string(next++));
      instance_ids[inst.get("instanceId")] = copy->get("instanceId");
      const std::string* parent = inst.attribute("parentInstanceId");
      if (parent != nullptr) {
        auto id = instance_ids.find(*parent);
        if (id != instance_ids.end())
          copy->set("parentInstanceId", id->second);
        else
          copy->erase("parentInstanceId");
      }
      into_by_path[path] = copy.get();
      insert_child(into, std::move(copy));
    }
  }

  /*! The covergroup type of a covergroupCoverage or of a cgInstance */
  static std::string cg_name(const xml_node& node)
  {
    const xml_node* inst = (node.name == "cgInstance") ? &node : node.element("cgInstance");
    const xml_node* id = (inst != nullptr) ? inst->element("cgId") : nullptr;
    return (id != nullptr) ? id->get("cgName") : std::string();
  }

  void merge_instance(xml_node& into, const xml_node& from)
  {
    keyed_children groups(into, "covergroupCoverage", cg_name);
    for (auto group : from.elements("covergroupCoverage"))
    {
      xml_node* match = groups.match(cg_name(*group));
      if (match != nullptr) {
        merge_covergroup(*match, *group);
        continue;
      }
      std::unique_ptr<xml_node> copy = group->clone();
      map_file_ids(*copy);
      groups.add(cg_name(*group), copy.get());
      insert_child(into, std::move(copy));
    }
    // the covergroups which were disabled, by their type name
    auto attr_key = [](const xml_node& n) { return n.get("key") + "\n" + n.text; };
    keyed_children attrs(into, "userAttr", attr_key);
    for (auto attr : from.elements("userAttr"))
      if (attrs.match(attr_key(*attr)) == nullptr)
        insert_child(into, attr->clone());
  }

  void merge_covergroup(xml_node& into, const xml_node& from)
  {
    auto key = [](const xml_node& n) { return cg_name(n) + "\n" + n.get("name"); };
    keyed_children insts(into, "cgInstance", key);
    for (auto inst : from.elements("cgInstance"))
    {
      xml_node* match = insts.match(key(*inst));
      if (match != nullptr) {
        merge_cg_instance(*match, *inst);
        continue;
      }
      std::unique_ptr<xml_node> copy = inst->clone();
      map_file_ids(*copy);
      insts.add(key(*inst), copy.get());
      insert_child(into, std::move(copy));
    }
  }

  static std::string name_key(const xml_node& node)
  {
    return node.get("name");
  }

  /*! Merges the children with a name, matched by a key, with a function */
  template <typename K, typename M>
  void merge_children(xml_node& into, const xml_node& from, const std::string& child_name,
                      K key, M merge_child)
  {
    keyed_children children(into, child_name, key);
    for (auto child : from.elements(child_name))
    {
      xml_node* match = children.match(key(*child));
      if (match != nullptr) {
        merge_child(*match, *child);
        continue;
      }
      std::unique_ptr<xml_node> copy = child->clone();
      children.add(key(*child), copy.get());
      insert_child(into, std::move(copy));
    }
  }

  static void add_counts(xml_node& into, const xml_node& from)
  {
    add_coverage_count(into, coverage_count(from));
  }

  void merge_cg_instance(xml_node& into, const xml_node& from)
  {
    merge_children(into, from, "coverpoint", name_key, [this](xml_node& a, const xml_node& b) {
      merge_coverpoint(a, b);
    });
    merge_children(into, from, "cross", name_key, [](xml_node& a, const xml_node& b) {
      // the bins of a cross are known by their names, as <0,1>
      merge_children_counts(a, b, "crossBin", name_key);
    });
  }

  template <typename K>
  static void merge_children_counts(xml_node& into, const xml_node& from,
                                    const std::string& child_name, K key)
  {
    keyed_children children(into, child_name, key);
    for (auto child : from.elements(child_name))
    {
      xml_node* match = children.match(key(*child));
      if (match != nullptr) {
        add_counts(*match, *child);
        continue;
      }
      coverage_count(*child);
      std::unique_ptr<xml_node> copy = child->clone();
      children.add(key(*child), copy.get());
      insert_child(into, std::move(copy));
    }
  }

  void merge_coverpoint(xml_node& into, const xml_node& from)
  {
    auto bin_key = [](const xml_node& n) { return n.get("type") + "\n" + n.get("name"); };
    merge_children(into, from, "coverpointBin", bin_key, [](xml_node& a, const xml_node& b) {
      auto range_key = [](const xml_node& n) { return n.get("from") + ":" + n.get("to"); };
      merge_children_counts(a, b, "range", range_key);
      auto sequence_key = [](const xml_node& n) {
        std::string key;
        for (auto v : int_list(n, "seqValue"))
          key += std::to_string(v) + ",";
        return key;
      };
      merge_children_counts(a, b, "sequence", sequence_key);
    });
  }
};

/*!
 * \brief Adds the hit counts of a UCIS XML file to the coverage model
 *
 * See \link fc4sc::load_coverage_db \endlink.
 */
class loader
{
public:

  /*! Number of ranges, sequences and cross bins whose counts were added */
  uint64_t loaded = 0;

  /*! Number of elements of the file which match nothing in the model */
  uint64_t not_found = 0;

  void load(const xml_node& root, global* cntxt)
  {
    if (root.name != "UCIS")
      throw std::runtime_error("The root element of a UCIS XML file must be <UCIS>");

    // the scopes of the model, by the path of names and types written in the file
    std::map<std::string, scp_base_data_model*> scopes;
    for (auto scp : global::get_top_scopes(cntxt))
      add_scope(scopes, scp, "");

    for (auto &entry : instance_paths(root))
    {
      const xml_node* inst = entry.first;
      const std::string& path = entry.second;
      auto scp = scopes.find(path);
      if (scp == scopes.end()) {
        missing("scope " + path);
        continue;
      }
      for (auto group : inst->elements("covergroupCoverage"))
        for (auto cg : group->elements("cgInstance"))
          load_cg_instance(*cg, *scp->second, path);
    }
  }

private:

  void missing(const std::string& what)
  {
    not_found++;
    std::cerr << "FC4SC load_coverage_db: Warning! " << what
              << " of the file is not in the coverage model\n";
  }

  static void add_scope(std::map<std::string, scp_base_data_model*>& scopes,
                        scp_base_data_model* scp, const std::string& parent_path)
  {
    const std::string path = parent_path + "/" + scp->name + "(" + scp->type_data->type_name + ")";
    scopes[path] = scp;
    for (auto &child : scp->child_scp_insts)
      add_scope(scopes, child.second, path);
  }

  void load_cg_instance(const xml_node& cg, scp_base_data_model& scp, const std::string& path)
  {
    const xml_node* id = cg.element("cgId");
    const std::string type = (id != nullptr) ? id->get("cgName") : std::string();
    const std::string scp_type = (id != nullptr) ? id->get("moduleName") : std::string();
    const std::string name = cg.get("name");
    cvg_base_data_model* cvg = nullptr;
    for (auto &type_it : scp.cvgs)
      for (auto data : type_it.second)
        if (data->name == name && data->type_data->type_name == type &&
            data->type_data->scp_type_name == scp_type)
          cvg = data;
    if (cvg == nullptr) {
      missing("covergroup " + path + "/" + name + " of type " + type);
      return;
    }
    for (auto cvp : cg.elements("coverpoint"))
    {
      coverpoint_base_data_model* data = nullptr;
      for (auto c : cvg->cvps)
        if (c->name == cvp->get("name") && dynamic_cast<coverpoint_base_data_model*>(c) != nullptr)
          data = static_cast<coverpoint_base_data_model*>(c);
      if (data == nullptr)
        missing("coverpoint " + path + "/" + name + "/" + cvp->get("name"));
      else
        load_coverpoint(*cvp, *data, path + "/" + name + "/" + cvp->get("name"));
    }
    for (auto crs : cg.elements("cross"))
    {
      cross_base_data_model* data = nullptr;
      for (auto c : cvg->cvps)
        if (c->name == crs->get("name") && dynamic_cast<cross_base_data_model*>(c) != nullptr)
          data = static_cast<cross_base_data_model*>(c);
      if (data == nullptr)
        missing("cross " + path + "/" + name + "/" + crs->get("name"));
      else
        load_cross(*crs, *data, path + "/" + name + "/" + crs->get("name"));
    }
  }

  /*! Adds the counts of the ranges and sequences of a bin to the data of a bin */
  void load_bin(const xml_node& bin, bin_base_data_model& data, const std::string& path)
  {
    auto& hits = data.get_interval_hits();
    std::vector<std::pair<std::string, std::string>> ranges;
    if (data.get_bin_type() == bin_t::default_bin_) {
      for (auto &range : data.get_default_ranges())
        ranges.push_back(std::make_pair(std::get<0>(range), std::get<1>(range)));
    }
    else {
      for (auto &intv : data.get_intervals_to_int())
        ranges.push_back(std::make_pair(std::to_string(intv.first), std::to_string(intv.second)));
    }
    std::vector<bool> used(ranges.size(), false);
    for (auto range : bin.elements("range"))
    {
      const std::string from = range->get("from"), to = range->get("to");
      size_t i = 0;
      while (i < ranges.size() && (used[i] || ranges[i].first != from || ranges[i].second != to)) ++i;
      if (i == ranges.size() || i >= hits.size()) {
        missing("range " + from + ":" + to + " of bin " + path);
        continue;
      }
      used[i] = true;
      hits[i] = add_saturated(hits[i], coverage_count(*range));
      loaded++;
    }

    auto sequences = data.get_sequences_to_int();
    std::vector<bool> seq_used(sequences.size(), false);
    for (auto seq : bin.elements("sequence"))
    {
      auto values = int_list(*seq, "seqValue");
      size_t i = 0;
      auto same = [&](const std::vector<int>& sequence) {
        if (sequence.size() != values.size()) return false;
        for (size_t k = 0; k < values.size(); ++k)
          if (values[k] != sequence[k]) return false;
        return true;
      };
      while (i < sequences.size() && (seq_used[i] || !same(sequences[i]))) ++i;
      if (i == sequences.size() || i >= hits.size()) {
        missing("sequence of bin " + path);
        continue;
      }
      seq_used[i] = true;
      hits[i] = add_saturated(hits[i], coverage_count(*seq));
      loaded++;
    }
  }

  void load_coverpoint(const xml_node& cvp, coverpoint_base_data_model& data, const std::string& path)
  {
    // the bins of the model with each name, matched in the order of the file
    std::map<std::string, std::vector<bin_base_data_model*>> regular, illegal, ignore, defaults;
    for (auto b : data.bins_data) regular[b->get_name()].push_back(b);
    for (auto b : data.illegal_bins_data) illegal[b->get_name()].push_back(b);
    for (auto b : data.ignore_bins_data) ignore[b->get_name()].push_back(b);
    for (auto b : data.default_bins_data) defaults[b->get_name()].push_back(b);
    std::map<std::string, size_t> used;

    for (auto bin : cvp.elements("coverpointBin"))
    {
      const std::string type = bin->get("type");
      const std::string name = bin->get("name");
      const std::string bin_path = path + "/" + name;
      bin_base_data_model* data_bin = nullptr;
      auto take = [&](std::map<std::string, std::vector<bin_base_data_model*>>& bins) {
        auto it = bins.find(name);
        size_t& n = used[type + "\n" + name];
        if (it != bins.end() && n < it->second.size())
          data_bin = it->second[n++];
      };
      if (type == "default")
        take(regular);
      else if (type == "illegal")
        take(illegal);
      else if (type == "ignore") {
        take(ignore);
        // a default bin is written as an ignore bin
        if (data_bin == nullptr) take(defaults);
        // and a default bin array as a bin for each value hit, as others[5]
        if (data_bin == nullptr && !name.empty() && name.back() == ']') {
          const size_t open = name.rfind('[');
          auto it = (open == std::string::npos) ? defaults.end() : defaults.find(name.substr(0, open));
          if (it != defaults.end() && it->second.size() == 1 && it->second[0]->is_value_array()) {
            const std::string value = name.substr(open + 1, name.size() - open - 2);
            uint64_t count = 0;
            for (auto range : bin->elements("range"))
              count = add_saturated(count, coverage_count(*range));
            if (it->second[0]->add_value_hits(value, count))
              loaded++;
            else
              missing("value " + value + " of default bin " + bin_path);
            continue;
          }
        }
      }
      if (data_bin == nullptr) {
        missing("bin " + bin_path);
        continue;
      }
      load_bin(*bin, *data_bin, bin_path);
    }
  }

  void load_cross(const xml_node& crs, cross_base_data_model& data, const std::string& path)
  {
    for (auto bin : crs.elements("crossBin"))
    {
      if (bin->get("type") == "ignore") continue;
      const std::string name = bin->get("name");
      auto indices = int_list(*bin, "index");
      bool product = !indices.empty();
      for (auto i : indices)
        product = product && (i >= 0);
      const uint64_t count = coverage_count(*bin);
      if (product) {
        std::vector<size_t> key;
        bool valid = (indices.size() == data.cross_cvps.size());
        for (size_t i = 0; valid && i < indices.size(); ++i)
        {
          valid = static_cast<uint64_t>(indices[i]) < data.cross_cvps[i]->size();
          key.push_back(static_cast<size_t>(indices[i]));
        }
        if (!valid || (!data.user_bins.empty() && !data.is_auto_bin(key))) {
          missing("bin " + name + " of cross " + path);
          continue;
        }
        data.add_cross_bin_hits(key, count);
        loaded++;
        continue;
      }
      // a user defined bin, by its name
      bool found = false;
      for (auto &user : data.user_bins)
        if (!found && user.name == name && user.bin_type == bin_t::default_) {
          user.hits = add_saturated(user.hits, count);
          found = true;
        }
      if (found)
        loaded++;
      else
        missing("bin " + name + " of cross " + path);
    }
  }
};

} // namespace ucis_db

/*!
 * \brief Adds the hit counts of a UCIS XML file written by FC4SC to the
 * coverage model, as the SystemVerilog $load_coverage_db
 * \param file_name The file
 * \param cntxt The context of the coverage model
 * \returns The number of ranges, sequences and cross bins of the file whose
 * hit counts were added
 *
 *   The scopes are matched by their names and types, the covergroup
 * instances by their types and names, coverpoints and crosses by their
 * names, the bins of a coverpoint by their names and types, their ranges by
 * their limits and their sequences by their values, the automatically
 * created bins of a cross by their indices and its user defined bins by
 * their names. The elements of the file which match nothing in the model
 * are reported as warnings and left out. The misses of the coverpoints and
 * crosses are not in the file, so they are not loaded.
 *
 *   Throws std::runtime_error if the file cannot be read, is not well
 * formed or has an invalid coverage count; the model is then left with the
 * counts added before the error.
 */
inline uint64_t load_coverage_db(const std::string& file_name, global* cntxt = global::getter())
{
  auto root = ucis_db::read_xml_file(file_name);
  ucis_db::loader loader;
  loader.load(*root, cntxt);
  return loader.loaded;
}

/*!
 * \brief Merges UCIS XML files written by FC4SC into one, as the files of
 * runs with different seeds, see \link fc4sc::ucis_db::merger \endlink
 * \param inputs The files to merge, at least one
 * \param output The merged file, which may be one of the inputs
 *
 * Throws std::runtime_error if a file cannot be read or written, is not
 * well formed or has an invalid coverage count.
 */
inline void merge_coverage_db(const std::vector<std::string>& inputs, const std::string& output)
{
  if (inputs.empty())
    throw std::runtime_error("merge_coverage_db needs at least one input file");
  auto root = ucis_db::read_xml_file(inputs[0]);
  ucis_db::merger merger;
  for (size_t i = 1; i < inputs.size(); ++i)
    merger.merge(*root, *ucis_db::read_xml_file(inputs[i]));
  uint64_t key = 1;
  ucis_db::merger::renumber_keys(*root, key);
  ucis_db::write_xml_file(*root, output);
}

} // namespace fc4sc

#endif /* FC4SC_COVERAGE_DB_HPP */
