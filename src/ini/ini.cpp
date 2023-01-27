#include "./ini.hpp"

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>

INI::Section::operator std::string() const {
  std::stringstream ss;
  ss << "[" << name_ << "]\n";
  for (auto &[key, val] : *this) ss << key << "=" << val << "\n";
  return ss.str();
}

INI INI::parse(std::ifstream &f, CommentType type) {
  INI obj;
  Section *p_sec = nullptr;
  for (std::string line_buf; !f.eof();) {
    std::getline(f, line_buf);
    auto line = std::string_view(line_buf.c_str());
    line = trim(line);
    if (line.empty() || is_comment_line(line, type)) {
      continue;
    } else if (line[0] == '[') {
      // section name
      if (line.back() != ']') throw "There is no ] in section name";
      auto sec_name = std::string(line.begin() + 1, line.end() - 1);
      // if(obj.has(sec_name))
      //   throw "Duplicated section name.";
      obj.add(sec_name);
      p_sec = &obj.data_[sec_name];
    } else {
      if (auto pos = line.find_first_of('='); pos == std::string_view::npos)
        throw "There is no = in the key-value pair";
      else if (p_sec == nullptr)
        throw "The key-value pair has no section name.";
      else {
        auto name = std::string(rtrim(line.substr(0, pos))),
             value = std::string(ltrim(line.substr(pos + 1)));
        p_sec->emplace(std::move(name), std::move(value));
      }
    }
  }
  return obj;
}

INI::operator std::string() const {
  std::stringstream ss;
  for (auto &[_, sec] : data_) ss << std::string(sec) << '\n';
  return ss.str();
}

bool INI::add(const std::string &section_name) {
  if (auto p_sec = get_section(section_name); p_sec != nullptr) return false;
  data_.emplace(section_name, Section(section_name));
  return true;
}

bool INI::remove(const std::string &section_name) {
  auto iter_sec = data_.find(section_name);
  if (iter_sec == data_.end()) return false;
  data_.erase(iter_sec);
  return true;
}

void INI::update(const Section &section) {
  const auto name = section.name();
  if (auto p_sec = get_section(name); p_sec == nullptr)
    data_.emplace(name, section);
  else
    for (auto &[key, val] : section) p_sec->emplace(key, val);
}

bool INI::rename(const std::string &section_name) {
  auto iter_sec = data_.find(section_name);
  if (iter_sec == data_.end()) return false;
  // rename the section name of the map
  (iter_sec->second).set_name(section_name);
  auto nh = data_.extract(iter_sec);  // C++ 17
  nh.key() = section_name;
  data_.insert(std::move(nh));
  return true;
}

void INI::set(const std::string &section_name, const std::string &key,
              const std::string &value) {
  if (data_.find(section_name) == data_.end())
    // create according section
    data_.emplace(section_name, Section(section_name));
  data_[section_name][key] = value;
}

bool INI::remove(const std::string &section_name, const std::string &key) {
  auto iter_sec = data_.find(section_name);
  if (iter_sec == data_.end()) return false;
  auto &sec = iter_sec->second;
  auto iter_val = sec.find(key);
  if (iter_val == sec.end()) return false;
  sec.erase(iter_val);
  return true;
}

bool INI::has(const std::string &section_name, const std::string &key) const {
  if (auto p_sec = get_section(section_name); p_sec == nullptr)
    return false;
  else
    return p_sec->count(key);
}

std::string INI::get(const std::string &section_name, const std::string &key,
                     const std::string &default_value) const {
  if (auto p_sec = get_section(section_name); p_sec == nullptr)
    return default_value;
  else if (auto iter_val = p_sec->find(key); iter_val == p_sec->end())
    return default_value;
  else
    return iter_val->second;
}