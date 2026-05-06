#include "player_anim.h"
#include "subterra_player_json_io.h"

#include <cstdio>

namespace subterra {

criogenio::AssetId LoadPlayerAnimationDatabaseFromJson(const std::string &jsonPath,
                                                       PlayerAnimConfig *outCfg) {
  int fw = 64, fh = 64;
  std::string err;
  criogenio::AssetId id =
      criogenio::LoadSubterraGuildAnimationJson(jsonPath, &fw, &fh, &err);
  if (outCfg) {
    outCfg->frameW = fw;
    outCfg->frameH = fh;
  }
  if (id == criogenio::INVALID_ASSET_ID && !err.empty())
    std::fprintf(stderr, "[player.json] %s\n", err.c_str());
  return id;
}

} // namespace subterra
