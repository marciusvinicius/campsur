#pragma once

#include "components.h"
#include "serialization.h"
#include <string>

namespace criogenio {

/**
 * Editor-authoring tag: assigns an entity to a named "object layer" so the
 * editor can group, show/hide, and lock entities together (parity with tile
 * layers in the Layers panel). Defaults to "Default" for any entity that
 * doesn't carry the component explicitly.
 *
 * The runtime ignores this component; it has no gameplay effect. It IS
 * serialized so authoring intent persists through level save/load.
 */
struct LayerMembership : public Component {
  std::string layerName = "Default";

  std::string TypeName() const override { return "LayerMembership"; }

  SerializedComponent Serialize() const override {
    SerializedComponent o;
    o.type = TypeName();
    o.fields["layerName"] = layerName;
    return o;
  }

  void Deserialize(const SerializedComponent &data) override {
    if (auto it = data.fields.find("layerName"); it != data.fields.end())
      layerName = GetString(it->second);
  }
};

/**
 * Transient editor-only tag: when present, render systems skip the entity.
 * Applied/cleared by the editor whenever the user toggles object-layer
 * visibility. NOT persisted (Serialize/Deserialize are no-ops); the editor
 * recomputes membership on each toggle from its `hiddenObjectLayers` set.
 */
struct EditorHidden : public Component {
  std::string TypeName() const override { return "EditorHidden"; }
  SerializedComponent Serialize() const override {
    SerializedComponent o;
    o.type = TypeName();
    return o;
  }
  void Deserialize(const SerializedComponent &) override {}
};

} // namespace criogenio
