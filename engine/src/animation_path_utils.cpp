#include "animation_path_utils.h"

#include <filesystem>
#include <string>

namespace criogenio {
namespace fs = std::filesystem;

namespace {

void StripLeadingDotSlash(std::string &s) {
  while (s.size() >= 2 && s[0] == '.' && (s[1] == '/' || s[1] == '\\')) {
    s.erase(0, 2);
  }
}

} // namespace

std::string ResolveAnimationTexturePathRelToJson(const std::string &jsonPath,
                                                 std::string texRel) {
  if (texRel.empty())
    return texRel;
  StripLeadingDotSlash(texRel);

  fs::path rel(texRel);
  std::error_code ec;
  if (rel.is_absolute()) {
    if (fs::is_regular_file(rel, ec))
      return rel.lexically_normal().string();
    return rel.lexically_normal().string();
  }

  if (fs::is_regular_file(texRel, ec))
    return fs::path(texRel).lexically_normal().string();

  const fs::path jsonP = fs::path(jsonPath).lexically_normal();
  const fs::path animDir = jsonP.parent_path();
  const fs::path dataDir = animDir.parent_path();
  const fs::path packRoot = dataDir.parent_path();
  const fs::path repoRoot = packRoot.parent_path();

  const fs::path cwd = fs::current_path(ec);
  const fs::path roots[] = {cwd, animDir, dataDir, packRoot, repoRoot};
  for (const auto &root : roots) {
    if (root.empty())
      continue;
    fs::path cand = (root / texRel).lexically_normal();
    if (fs::is_regular_file(cand, ec))
      return cand.string();
  }

  const fs::path fallback = (packRoot.empty() ? cwd : packRoot) / texRel;
  return fallback.lexically_normal().string();
}

} // namespace criogenio
