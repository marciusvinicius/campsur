#pragma once

#include <string>
#include <string_view>

namespace subterra {

/** Strips // comments outside JSON string literals (reference data files use JSON5-style //). */
inline std::string SubterraStripJsonLineComments(std::string_view in) {
  std::string out;
  out.reserve(in.size());
  bool in_str = false;
  bool esc = false;
  for (size_t i = 0; i < in.size(); ++i) {
    char c = in[i];
    if (esc) {
      out += c;
      esc = false;
      continue;
    }
    if (in_str) {
      if (c == '\\')
        esc = true;
      else if (c == '"')
        in_str = false;
      out += c;
      continue;
    }
    if (c == '"') {
      in_str = true;
      out += c;
      continue;
    }
    if (c == '/' && i + 1 < in.size() && in[i + 1] == '/') {
      while (i < in.size() && in[i] != '\n')
        ++i;
      continue;
    }
    out += c;
  }
  return out;
}

} // namespace subterra
