# Engine Improvement Suggestions

Suggestions based on a review of the Criogenio engine. Ordered by impact vs effort; you can tackle them incrementally.

---

## 1. **Remove global `netToEntity` (network)**

**Current:** `extern std::unordered_map<NetEntityId, Entity> netToEntity` is shared and written by both `ReplicationServer` and `ReplicationClient`.

**Issues:** Hard to test, unclear ownership, and listen-server (server + client in one process) would share one map.

**Improvement:**  
- Keep the map inside `ReplicationClient` (and optionally inside `ReplicationServer` if needed for debugging).  
- Pass the client’s map (or a lookup interface) only where needed; remove the global.

---

## 2. **Scoped ECS / multiple worlds**

**Current:** `Registry::instance()`, `EntityManager::instance()` are global singletons. One process = one world.

**Issues:** No way to run two worlds (e.g. main game + preview), and unit tests can’t isolate ECS state.

**Improvement:**  
- Option A: Store `Registry` (and optionally `EntityManager`) inside `World` and pass `World&` (or `Registry&`) into systems.  
- Option B: Keep globals but add `Registry::reset()` / `World::clear()` and document that tests must reset state.  
- Long term: “World” owns the ECS; engine and games refer to a world instance, not global registry.

---

## 3. **ECS view performance and allocations**

**Current:** `Query::build()` allocates a `std::vector<EntityId>` every call and iterates all alive entities + signature check.

**Issues:**  
- `GetEntitiesWith<T,U>()` allocates every frame per system.  
- View is O(alive entities) per query, no archetype-style grouping.

**Improvements:**  
- **Short term:** Reuse a thread-local or system-local `std::vector<EntityId>` and clear/fill in place (e.g. `view<T,U>(output)`), or return a non-owning span if you can.  
- **Medium term:** Cache views per (T, U, …) and invalidate on add/remove component; or introduce archetypes and iterate only matching archetypes.

---

## 4. **System execution order**

**Current:** Systems run in registration order for both `Update()` and `Render()`; order is implicit.

**Improvement:**  
- Add optional priority or named phases (e.g. `Movement`, `Collision`, `Render`).  
- Or at least document and enforce a single canonical order (e.g. in `Engine` or `World`: “movement systems first, then collision, then render”).

---

## 5. **Renderer and World ownership in Engine**

**Current:** `Renderer* renderer`, `World* world` with manual delete in destructor.

**Improvement:** Use `std::unique_ptr<Renderer>` and `std::unique_ptr<World>` so ownership is explicit and exception-safe.

---

## 6. **Abstract Raylib (render + input)**

**Current:** `Renderer` and `Input` are thin wrappers; `#include "raylib.h"` appears in engine and world headers.

**Improvement:**  
- Introduce a small `IRenderer` / `IInput` (or backend interface) in the engine; implement with Raylib in a single place.  
- Keep raylib includes in `.cpp` and one backend header so the rest of the engine doesn’t depend on raylib.  
- Aligns with your TODO: “Abstract raylib on engine (possibility to change the motor later)”.

---

## 7. **Error handling and loading**

**Current:** Many load paths assume success (e.g. textures, assets); failures can be unclear.

**Improvement:**  
- Prefer `std::optional` or `expected<T, Error>` for load APIs.  
- Log failures (e.g. `TraceLog`) and document behavior when load fails (fallback texture, skip entity, or abort).  
- Ensures editor and games don’t silently use invalid resources.

---

## 8. **Network: delta / dirty snapshots**

**Current:** Full snapshot every frame for all replicated entities.

**Improvement:**  
- Add a dirty flag or “last sent state” per entity; only serialize and send when transform (or other replicated data) changed.  
- Reduces bandwidth when many entities are idle; you can still send full snapshots periodically for reliability.

---

## 9. **Component vs ECS storage**

**Current:** Components inherit a `Component` base with `Serialize`/`Deserialize`/`TypeName`; ECS stores them by type in the registry.

**Improvement:**  
- Keep “serialization” on a small set of types (or a trait) and use the registry as pure data (no need for every component to inherit `Component` for ECS).  
- Simplifies adding simple, data-only structs as components and keeps serialization opt-in.

---

## 10. **Camera as a component**

**Current:** Camera is global on `World` (`maincamera`, `AttachCamera2D`).

**Improvement:**  
- Add a `Camera2D` component (or tag + data component).  
- One entity with the camera component is “active”; systems that need the camera take it from that entity.  
- Enables multiple cameras, camera per scene, or camera following an entity without special-case code in `World`.

---

## 11. **Event system usage**

**Current:** `EventBus` exists; usage in core loop and systems is limited.

**Improvement:**  
- Use events for “entity spawned”, “entity destroyed”, “collision”, “input consumed” so gameplay and UI can react without direct coupling.  
- Reduces the need for systems to know about each other and makes debugging and extensions easier.

---

## 12. **Testing**

**Current:** No engine-level tests visible; regressions (e.g. replication, ECS) are caught manually.

**Improvement:**  
- Unit tests for: ECS (create/destroy entity, add/remove component, view correctness), serialization round-trip, snapshot parse/build.  
- Small integration test: start server, connect client, send input, check snapshot contains expected state.  
- Even a few tests will help when refactoring (e.g. netToEntity, ECS scope).

---

## Quick wins (low effort)

| Change | Benefit |
|--------|--------|
| `unique_ptr` for `Renderer` and `World` in `Engine` | Clear ownership, exception safety |
| Document system execution order in `ARCHITECTURE.md` | Fewer subtle bugs, easier onboarding |
| Move `netToEntity` into `ReplicationClient` (and out of global) | Clearer ownership, no global state |
| Reuse buffer in ECS view (or return span) | Fewer allocations per frame |

## Medium effort, high impact

| Change | Benefit |
|--------|--------|
| Raylib behind interfaces | Swap backend later, easier testing |
| Optional/error returns for asset loading | Predictable failure handling |
| System phases or priorities | Deterministic, understandable order |
| Camera as component | More flexible scenes and cameras |

## Larger refactors

| Change | Benefit |
|--------|--------|
| World/Registry scoped to `World` (no global ECS) | Multiple worlds, tests, cleaner design |
| Delta/dirty replication | Lower bandwidth, scalability |
| Archetype or cached views | Better cache use and iteration performance |

If you say which area you want to tackle first (e.g. “network”, “ECS”, “rendering”), the next step can be a concrete patch or design for that part.
