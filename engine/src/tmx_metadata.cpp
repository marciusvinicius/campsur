#include "tmx_metadata.h"

namespace criogenio {

static const TmxProperty *findProp(const std::vector<TmxProperty> &props,
                                   const char *name) {
  for (const auto &p : props) {
    if (p.name == name)
      return &p;
  }
  return nullptr;
}

int TmxGetPropertyInt(const std::vector<TmxProperty> &props, const char *name,
                      int defaultValue) {
  const TmxProperty *p = findProp(props, name);
  if (!p || p->value.empty())
    return defaultValue;
  try {
    return std::stoi(p->value);
  } catch (...) {
    return defaultValue;
  }
}

bool TmxGetPropertyBool(const std::vector<TmxProperty> &props, const char *name,
                        bool defaultValue) {
  const TmxProperty *p = findProp(props, name);
  if (!p)
    return defaultValue;
  const std::string &v = p->value;
  if (v == "true" || v == "1")
    return true;
  if (v == "false" || v == "0")
    return false;
  return defaultValue;
}

const char *TmxGetPropertyString(const std::vector<TmxProperty> &props,
                                 const char *name, const char *defaultValue) {
  const TmxProperty *p = findProp(props, name);
  if (!p)
    return defaultValue;
  return p->value.c_str();
}

} // namespace criogenio
