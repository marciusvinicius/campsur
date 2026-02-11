# Criogenio Engine Architecture Documentation

## Overview

The Criogenio Engine is a 2D game engine built in C++ using an Entity Component System (ECS) architecture. It provides a modular, data-driven approach to game development with support for animations, terrain rendering, serialization, asset management, and **server-authoritative multiplayer** over ENet.

## Core Architecture

### Engine Class

The `Engine` class is the main entry point and orchestrates all subsystems:

```
Engine
├── Renderer (handles window and rendering)
├── World (ECS container and game state)
├── EventBus (event system)
├── AssetManager (resource loading)
└── Network (optional)
    ├── INetworkTransport (e.g. ENetTransport)
    ├── ReplicationServer (when server)
    └── ReplicationClient (when client)
```

**Key Responsibilities:**
- Initializes subsystems (Renderer, World, AssetManager)
- Runs the main game loop (with optional network step)
- Manages component registration
- Provides access to subsystems and network APIs (`StartServer`, `ConnectToServer`, `SendInputAsClient`, `SetServerPlayerInput`)

**Lifecycle:**
1. Constructor: Initializes renderer, world, and registers texture loader
2. Optional: `StartServer(port)` or `ConnectToServer(host, port)` to enter network mode
3. `Run()`: Main game loop (see [Main Loop Order](#main-loop-order))
4. Destructor: Cleans up resources in proper order (replication client/server → transport → animations → world → assets → renderer)

**Network Modes:** `NetworkMode::Off`, `NetworkMode::Server`, `NetworkMode::Client`. The server can act as a player (local input applied via `SetServerPlayerInput`).

### World Class

The `World` class is the central ECS container that manages entities, components, and systems:

**Features:**
- Entity creation and destruction
- Component management (add, get, remove, query)
- System registration and execution
- Terrain management
- Serialization/Deserialization
- Camera management

**Key Methods:**
- `CreateEntity()`: Creates a new entity
- `AddComponent<T>()`: Adds a component to an entity
- `GetComponent<T>()`: Retrieves a component from an entity
- `GetEntitiesWith<T>()`: Queries entities by component type
- `Serialize()`: Converts world state to serializable format
- `Deserialize()`: Restores world state from serialized data

## Entity Component System (ECS)

### Architecture Pattern

The engine uses a **Sparse Set** data structure for efficient component storage, providing:
- O(1) component access
- Cache-friendly memory layout
- Efficient iteration over components

### Core ECS Components

#### 1. EntityManager
- Manages entity lifecycle (create, destroy, track alive entities)
- Maintains entity signatures (which components each entity has)
- Singleton pattern

#### 2. ComponentTypeRegistry
- Assigns unique type IDs to component types
- Template-based type registration
- Singleton pattern

#### 3. SparseSet<T>
- Core data structure for component storage
- **Sparse array**: Maps entity ID → index in dense array
- **Dense array**: Actual component data (cache-friendly)
- **Entities array**: Tracks which entities have this component

**Benefits:**
- Fast component lookup: O(1)
- Memory efficient: Only stores components that exist
- Cache-friendly: Dense array allows efficient iteration

#### 4. Registry
- Main ECS container
- Manages component storage for all types
- Provides query interface
- Singleton pattern

**Key Operations:**
- `create_entity()`: Creates new entity
- `add_component<T>()`: Adds component to entity
- `get_component<T>()`: Retrieves component
- `view<T, U, ...>()`: Queries entities with specific components

### Component System

#### Component Base Class

All components inherit from `Component` base class:

```cpp
class Component {
  virtual std::string TypeName() const = 0;
  virtual SerializedComponent Serialize() const = 0;
  virtual void Deserialize(const SerializedComponent &data) = 0;
};
```

#### Built-in Components

1. **Transform**
   - Position (x, y)
   - Rotation
   - Scale (scale_x, scale_y)

2. **AnimatedSprite**
   - References animation by AssetId
   - Current clip name
   - Frame timer and index
   - Runtime state only (no owned assets)

3. **AnimationState**
   - Current animation state (IDLE, WALKING, ATTACKING)
   - Previous state
   - Facing direction (UP, DOWN, LEFT, RIGHT)

4. **Controller**
   - Movement speed
   - Direction

5. **AIController**
   - Movement speed
   - Direction
   - Brain state (FRIENDLY, HOSTILE, etc.)
   - Target entity ID

6. **Name**
   - Entity name string

7. **NetReplicated**
   - Marks an entity for network replication (server-authoritative snapshots).

8. **ReplicatedNetId**
   - Network entity ID (e.g. for connection-order coloring). Server assigns; client receives in snapshots.

9. **Controller** (network usage)
   - `velocity` (Vector2) driven by `PlayerInput` on server (client input messages) or `SetServerPlayerInput` for the server’s local player.

## System Architecture

### System Interface

All systems implement `ISystem`:

```cpp
class ISystem {
  virtual void Update(float dt) = 0;
  virtual void Render(Renderer &renderer) = 0;
};
```

### Core Systems

#### 1. MovementSystem
- Processes entities with `Controller` component
- Reads keyboard input (arrow keys)
- Updates `Transform` position
- Updates `AnimationState` based on movement

#### 2. AIMovementSystem
- Processes entities with `AIController` component
- Implements AI movement logic (arrive behavior)
- Updates position toward target entity
- Updates animation state

#### 3. AnimationSystem
- Processes entities with `AnimatedSprite` component
- Builds clip keys from `AnimationState`
- Updates frame timers
- Manages animation playback

#### 4. RenderSystem
- Processes entities with `AnimatedSprite` and `Transform`
- Looks up animation definitions from `AnimationDatabase`
- Loads textures via `AssetManager`
- Renders sprites using `Renderer`

### System Execution Order

Systems run in **registration order** (order in which they are added to the world):

1. **Update Phase**: All systems' `Update(dt)` called
2. **Render Phase**: All systems' `Render(renderer)` called

There is no explicit priority or phases; document or enforce order in game code if dependencies matter (e.g. movement before collision before render).

---

## Main Loop Order

The engine run loop is ordered so that **server snapshots contain current-frame positions** after movement:

1. **OnFrame(dt)** – Game override: e.g. read input, call `SendInputAsClient(input)` (client) or `SetServerPlayerInput(input)` (server).
2. **world->Update(dt)** – All systems' `Update(dt)` run (e.g. `VelocityMovementSystem` updates transforms).
3. **Network step**
   - **Server:** `transport->Update()` (receive client input), `replicationServer->Update()` (apply input, build snapshot, send to all connections).
   - **Client:** `transport->Update()`, poll messages, `ApplySnapshot` for each snapshot, `UpdateInterpolation(dt)`.
4. **Render** – `BeginFrame()`, `world->Render(renderer)` (terrain + all systems' `Render()`), `OnGUI()`, `EndFrame()`.

Replicated entities on the client have no `Controller`; their position comes only from snapshots.

## Asset Management

### AssetManager

Thread-safe singleton for resource loading and caching:

**Features:**
- Type-safe resource loading via templates
- Resource caching (prevents duplicate loads)
- Custom loader registration
- Hot-reload support (callbacks)

**Usage:**
```cpp
// Register loader
AssetManager::instance().registerLoader<TextureResource>(loader);

// Load resource (cached)
auto texture = AssetManager::instance().load<TextureResource>(path);
```

### AnimationDatabase

Manages animation definitions and clips:

**Features:**
- Animation creation with texture paths
- Clip management (frames, frame speed)
- Reference counting for usage tracking
- Animation cloning

**Structure:**
- `AnimationDef`: Contains texture path and clips
- `AnimationClip`: Contains frames and frame speed
- `AnimationFrame`: Rectangle defining sprite frame

**AssetId System:**
- Each animation has a unique `AssetId`
- `AnimatedSprite` components reference animations by ID
- Allows multiple entities to share the same animation

### Resource Types

#### TextureResource
- Wraps Raylib's `Texture2D`
- Stores texture path
- Managed by `AssetManager`

## Rendering System

### Renderer Class

Abstraction layer over Raylib rendering:

**Features:**
- Window management
- Frame begin/end
- Texture drawing (DrawTexture, DrawTexturePro, DrawTextureRec)
- Rectangle drawing
- Text rendering

**Usage:**
- Systems call `Renderer` methods to draw
- Handles 2D camera transformations
- Manages rendering context

### Rendering Pipeline

1. `BeginFrame()`: Clear screen, begin 2D mode
2. Systems render (terrain, entities, etc.)
3. GUI rendering (if in editor)
4. `EndFrame()`: Swap buffers

### Input

The engine provides an `Input` abstraction (wrapping Raylib) for key/mouse state: `IsKeyDown`, `IsKeyPressed`, `IsMouseDown`, `GetMousePosition`. Games use this or Raylib directly for input.

---

## Network / Multiplayer

Multiplayer is **server-authoritative**: the server runs the simulation and sends state snapshots; clients send input and apply snapshots.

### Transport

- **INetworkTransport**: Interface for start server, connect, send, poll messages, get connection IDs.
- **ENetTransport**: ENet-based implementation (UDP, reliable/unreliable channels). Used for `camp_sur_cpp_tiled_walk` and other networked projects.

### Replication Server

- **ReplicationServer**: Uses a single `World` and `INetworkTransport`.
- **Server player**: Creates one local player entity (net id `0`) and applies `SetServerPlayerInput` each frame.
- **Client players**: For each connection in `GetConnectionIds()`, creates an entity with `NetReplicated`, `Transform`, `Controller`, `ReplicatedNetId(conn)`.
- **Input**: Receives `MsgType::Input` messages, applies `PlayerInput` to the corresponding entity’s `Controller`.
- **Snapshot**: Each frame builds a snapshot of all entities with `NetReplicated` + `Transform` (server player first with net id `0`, then others). Serializes transform (x, y, rotation, scale) and sends to every connection.

### Replication Client

- **ReplicationClient**: Applies received snapshots to the local `World`.
- For each snapshot entity: create or find entity by `NetEntityId`, set `NetReplicated`, `Transform`, `ReplicatedNetId(id)`; overwrite transform from snapshot data.
- **netToEntity**: Global map `NetEntityId → Entity` used by client (and written by server for consistency); see ENGINE_IMPROVEMENTS.md for moving this into the client.

### Message Types

- **Snapshot**: Full state tick + list of entities (id, component mask, serialized transform).
- **Input**: `PlayerInput` (move_x, move_y) from client to server.

### Game Responsibilities

- **Client**: Each frame, gather input and call `SendInputAsClient(input)`.
- **Server**: Each frame, gather local input and call `SetServerPlayerInput(input)` if the server is a player.
- **Rendering**: Query `NetReplicated` + `Transform` (and optionally `ReplicatedNetId` for color) so both server and clients draw all replicated players.

## Serialization System

### Architecture

Three-layer serialization system:

1. **Component Layer**: Components implement `Serialize()`/`Deserialize()`
2. **World Layer**: `World::Serialize()` collects all entities and components
3. **JSON Layer**: Converts to/from JSON format

### Serialization Flow

```
World State
    ↓
SerializedWorld (intermediate format)
    ↓
JSON (file format)
```

### SerializedWorld Structure

```cpp
struct SerializedWorld {
  std::vector<SerializedEntity> entities;
  SerializedTerrain2D terrain;
  std::vector<SerializedAnimation> animations;
};
```

### Key Features

- **Component Serialization**: Each component type handles its own serialization
- **Animation Serialization**: Animation definitions saved separately from entities
- **ID Remapping**: Animation IDs remapped on deserialize (old → new)
- **Texture Loading**: Textures loaded during deserialization via AssetManager

### Deserialization Process

1. Clear existing world state
2. Restore animation definitions (creates new IDs, loads textures)
3. Build animation ID mapping (old ID → new ID)
4. Restore terrain (loads tileset texture)
5. Restore entities and components
6. Map animation IDs in `AnimatedSprite` components

## Event System

### EventBus

Pub/sub event system:

**Features:**
- Type-based event subscription
- Multiple subscribers per event type
- Type-safe event emission

**Event Types:**
- `EntityCreated`
- `EntityDestroyed`
- `KeyPressed`
- `CollisionEnter`
- `CollisionExit`
- `Custom`

**Usage:**
```cpp
// Subscribe
eventBus.Subscribe(EventType::EntityCreated, callback);

// Emit
eventBus.Emit(EntityCreatedEvent(...));
```

## Terrain System

### Terrain2D

2D tile-based terrain system:

**Features:**
- Multiple layers
- Tileset support (texture atlas)
- Tile manipulation (set, fill, clear)
- Layer management (add, remove, resize)

**Structure:**
- `Tileset`: Texture atlas, tile size, grid dimensions
- `TileLayer`: Width, height, tile IDs array

**Rendering:**
- Renders tiles from texture atlas
- Supports multiple layers with transparency

## File Structure

```
engine/
├── include/
│   ├── engine.h              # Main engine class, network mode, Run loop
│   ├── world.h               # World/ECS container
│   ├── ecs_core.h            # Core ECS types (SparseSet, EntityManager)
│   ├── ecs_registry.h        # Registry implementation
│   ├── components.h          # Component definitions (incl. NetReplicated, ReplicatedNetId)
│   ├── animated_component.h  # Animation-related components
│   ├── systems.h             # System interface (ISystem)
│   ├── core_systems.h        # Built-in systems (Movement, AI, Animation, Render)
│   ├── render.h              # Renderer abstraction (Raylib)
│   ├── input.h               # Input abstraction (keys, mouse)
│   ├── asset_manager.h       # Asset loading system
│   ├── animation_database.h  # Animation management
│   ├── event.h               # Event system (EventBus)
│   ├── serialization.h       # Serialization types
│   ├── json_serialization.h  # JSON conversion
│   ├── terrain.h             # Terrain system
│   ├── resources.h           # Resource types
│   └── network/
│       ├── transport.h       # INetworkTransport interface
│       ├── enet_transport.h  # ENet implementation
│       ├── net_messages.h    # MsgType, PlayerInput, snapshot masks
│       ├── net_entity.h      # NetEntityId
│       ├── snapshot.h        # Snapshot, SnapshotEntity
│       ├── replication_server.h
│       ├── replication_client.h
│       └── serialization.h    # NetWriter, NetReader
└── src/
    ├── engine.cpp
    ├── world.cpp
    ├── core_systems.cpp
    ├── render.cpp
    ├── input.cpp
    ├── asset_manager.cpp
    ├── animation_database.cpp
    ├── json_serialization.cpp
    ├── terrain.cpp
    └── network/
        ├── enet_transport.cpp
        ├── replication_server.cpp
        └── replication_client.cpp
```

## Design Patterns

### Singleton Pattern
- `EntityManager`
- `ComponentTypeRegistry`
- `Registry`
- `AssetManager`
- `AnimationDatabase`

### Component Pattern
- All game objects are entities with components
- Components are data-only (no logic)
- Logic in systems

### Observer Pattern
- `EventBus` for event subscription

### Factory Pattern
- `ComponentFactory` for runtime component creation

### Server-Authoritative Replication
- Server owns simulation; clients send input and receive snapshots. `ReplicationServer` builds snapshots from `World`; `ReplicationClient` applies them to local `World`.

## Data Flow

### Entity Creation
```
CreateEntity() → EntityManager → Registry → Add Components
```

### Component Access
```
GetComponent<T>() → Registry → ComponentTypeRegistry → SparseSet → Component
```

### System Update
```
Update(dt) → Query Entities → Get Components → Process Logic
```

### Rendering
```
Render() → Query Entities → Get Components → Load Assets → Draw
```

### Multiplayer (server)
```
Receive Input → Apply to Controller → World->Update() (movement) → Build Snapshot → Send to connections
```

### Multiplayer (client)
```
Send Input → Receive Snapshot → ApplySnapshot (create/update entities, set Transform) → Render
```

## Best Practices

1. **Components**: Keep components as pure data, no logic
2. **Systems**: Put logic in systems, not components
3. **Queries**: Use `GetEntitiesWith<T>()` for efficient entity queries
4. **Assets**: Always load via `AssetManager` for caching
5. **Serialization**: Ensure all components implement `Serialize()`/`Deserialize()`
6. **Animation IDs**: Use `AssetId` references, not direct texture paths
7. **Network rendering**: Query `NetReplicated` + `Transform` (and `ReplicatedNetId` for color) so server and clients draw all replicated players

## Extension Points

### Adding New Components
1. Inherit from `Component`
2. Implement `TypeName()`, `Serialize()`, `Deserialize()`
3. Register in `Engine::RegisterCoreComponents()`

### Adding New Systems
1. Inherit from `ISystem`
2. Implement `Update()` and `Render()`
3. Add to world via `World::AddSystem<T>()`

### Adding New Resource Types
1. Create resource class inheriting from `Resource`
2. Register loader in `Engine` constructor
3. Use `AssetManager::load<T>()` to load

## Dependencies

- **Raylib**: Rendering, window management, input
- **ENet**: UDP networking (used by `ENetTransport` for multiplayer)
- **nlohmann/json**: JSON serialization
- **Standard Library**: STL containers, threading, memory management

## Related Documentation

- **ENGINE_IMPROVEMENTS.md**: Suggested improvements (global state, ECS scope, system order, network, testing, etc.).
- **TODO.md**: Project and editor task list.
