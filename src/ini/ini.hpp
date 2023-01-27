#ifndef INI_H_
#define INI_H_

#include <string>
#include <string_view>
#include <unordered_map>

#include "../utils/sv.hpp"

class INI {
 public:
  class Section : public std::unordered_map<std::string, std::string> {
   protected:
    std::string name_;

   public:
    Section() = default;

    Section(const std::string &name) : name_(name) {}

    std::string name() const { return name_; }

    void set_name(const std::string &name) { name_ = name; }

    operator std::string() const;
  };

  enum class CommentType {
    SEMICOLON = ';',    // char ';'
    NUMBER_SIGN = '#',  // char '#'
    POSSIBLE,           // char ';' or char '#'
  };

  INI() = default;
  ~INI() = default;

  /**
   * @brief Parse INI from std::ifstream
   */
  static INI parse(std::ifstream &f, CommentType type = CommentType::POSSIBLE);

  /**
   * @brief Parse INI from std::string_view
   */
  static INI parse(std::string_view content,
                   CommentType type = CommentType::POSSIBLE);

  /**
   * @brief Convert INI object into std::string
   */
  operator std::string() const;

  /**
   * @brief Update the INI object by another INI object.
   */
  void update(const INI &ini) {
    for (const auto &[name, sec] : ini.data_) update(sec);
  }

  /**
   * @brief Add a empty section.
   * @return Return false if the section exists.
   */
  bool add(const std::string &section_name);

  /**
   * @brief Remove the section
   * @return Return false if the section doesn't exist.
   */
  bool remove(const std::string &section_name);

  /**
   * @brief Update the INI object by a specify Section object.
   */
  void update(const Section &section);

  /**
   * @brief Rename the section.
   * @return Return false if the section doesn't exist.
   */
  bool rename(const std::string &section_name);

  /**
   * @brief Determine whether there is a corresponding section.
   */
  bool has(const std::string &section_name) const {
    return data_.find(section_name) == data_.end();
  }

  /**
   * @brief Set the value
   * @note The section or key will be created automatically if they don't exist.
   */
  void set(const std::string &section_name, const std::string &key,
           const std::string &value);

  /**
   * @brief Remove the value
   * @return Return false if the section or the key doesn't exist.
   */
  bool remove(const std::string &section_name, const std::string &key);

  /**
   * @brief Determine whether there is a key.
   */
  bool has(const std::string &section_name, const std::string &key) const;

  /**
   * @brief Get the value.
   */
  std::string get(const std::string &section_name, const std::string &key,
                  const std::string &default_value = "") const;

 protected:
  /**
   * @brief Determine whether is is a comment line.
   */
  static bool is_comment_line(std::string_view str, CommentType type) {
    if (char c = (char)type; type == CommentType::POSSIBLE)
      return str[0] == (char)CommentType::SEMICOLON ||
             str[0] == (char)CommentType::NUMBER_SIGN;
    else
      return str[0] == c;
  }

  /**
   * @brief Get the section object.
   */
  const Section *get_section(const std::string section_name) const {
    auto iter_sec = data_.find(section_name);
    if (iter_sec == data_.end()) return nullptr;
    return &(iter_sec->second);
  }

  /**
   * @brief Get the section object.
   */
  Section *get_section(const std::string section_name) {
    auto iter_sec = data_.find(section_name);
    if (iter_sec == data_.end()) return nullptr;
    return &(iter_sec->second);
  }

  std::unordered_map<std::string, Section> data_;
};

#endif