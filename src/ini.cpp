

#include "tinywebserver/ini.h"

#include <sstream>
#include <stdexcept>

#include "tinywebserver/utils/sv.h"

INI::Section::operator std::string() const {
  std::stringstream ss;
  ss << "[" << name_ << "]\n";
  for (auto &[key, val] : *this) ss << key << "=" << val << "\n";
  return ss.str();
}

INI INI::parse(std::string_view content, CommentType type) {
  INI obj;
  INI::KeyValues *p_sec;

  for (std::string_view line; !content.empty();) {
    getline(content, line, '\n');
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
  for (const auto &[name, sec] : data_) {
    ss << '[' << name << "]\n";
    for (const auto &[key, value] : sec) ss << key << '=' << value << '\n';
    ss << '\n';
  }
  return ss.str();
}

bool INI::add(const std::string &section_name) {
  if (section_name.empty()) return false;
  if (auto it = data_.find(section_name); it != data_.end()) return false;
  data_[section_name] = {};
  return true;
}

bool INI::remove(const std::string &section_name) {
  auto it = data_.find(section_name);
  if (it == data_.end()) return false;
  data_.erase(it);
  return true;
}

void INI::update(const std::string &section_name,
                 const INI::KeyValues &keyvalues) {
  if (auto it = data_.find(section_name); it == data_.end())
    it->second = keyvalues;
  else {
    auto &sec = it->second;
    for (const auto &[key, value] : keyvalues) sec.emplace(key, value);
  }
}

void INI::update(const Section &section) { update(section.name(), section); }

bool INI::rename(const std::string &section_name,
                 const std::string &new_section_name) {
  auto it = data_.find(section_name);
  if (it == data_.end()) return false;

  auto nh = data_.extract(it);
  nh.key() = new_section_name;
  data_.insert(std::move(nh));
  return true;
}

void INI::set(const std::string &section_name, const std::string &key,
              const std::string &value) {
  if (data_.find(section_name) == data_.end())
    // create according section
    data_[section_name] = {};
  data_[section_name][key] = value;
}

bool INI::remove(const std::string &section_name, const std::string &key) {
  auto it_sec = data_.find(section_name);
  if (it_sec == data_.end()) return false;
  auto &sec = it_sec->second;
  auto it_val = sec.find(key);
  if (it_val == sec.end()) return false;
  sec.erase(it_val);
  return true;
}

bool INI::rename(const std::string &section_name, const std::string &key,
                 const std::string &new_key) {
  auto it_sec = data_.find(section_name);
  if (it_sec == data_.end()) return false;
  auto &sec = it_sec->second;
  auto it_val = sec.find(key);
  if (it_val == sec.end()) return false;
  // sec.erase(it_val);
  auto nh = sec.extract(it_val);
  nh.key() = new_key;
  sec.insert(std::move(nh));
  return true;
}

bool INI::has(const std::string &section_name, const std::string &key) const {
  if (auto it_sec = data_.find(section_name); it_sec == data_.end())
    return false;
  else
    return (it_sec->second).count(key);
}

std::string INI::get(const std::string &section_name, const std::string &key,
                     const std::string &default_value) const {
  if (auto it_sec = data_.find(section_name); it_sec == data_.end())
    return default_value;
  else {
    auto &sec = it_sec->second;
    if (auto it_val = sec.find(key); it_val == sec.end())
      return default_value;
    else
      return it_val->second;
  }
}