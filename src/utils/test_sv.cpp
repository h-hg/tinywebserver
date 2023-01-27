#include <iostream>

#include "./sv.hpp"

using namespace std;
int main() {
  const std::regex pieces_regex(R"(\s*(.+)\s*=\s*(.+)\s*)");
  const std::string_view fname = " key1 key2  =   value1  value2  .txt   ";
  cout << fname << endl;
  svmatch pieces_match;
  if (std::regex_match(fname.begin(), fname.end(), pieces_match,
                       pieces_regex)) {
    std::cout << fname << '\n';
    for (size_t i = 0; i < pieces_match.size(); ++i) {
      svsub_match sub_match = pieces_match[i];
      std::string_view piece = sub_match.str();
      auto str = std::string(piece.begin(), piece.end());
      std::cout << "  submatch " << i << ": " << str << '\n';
    }
  }
  cout << "<" << ltrim("   123123   ") << ">" << endl;
  cout << "<" << rtrim("   123123   ") << ">" << endl;
  return 0;
}