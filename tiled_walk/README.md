# Tiled Walk – Multiplayer

A simple 2–4+ player game: everyone walks on a tiled map. Uses the engine’s ENet multiplayer (server-authoritative snapshots, client input).

## Run

1. **Start the server** (one terminal):
   ```bash
   ./bin/Debug/camp_sur_cpp_tiled_walk --server [port]
   ```
   Default port: 7777.

2. **Start clients** (one or more terminals):
   ```bash
   ./bin/Debug/camp_sur_cpp_tiled_walk --client 127.0.0.1 [port]
   ```

3. **Controls (client):** WASD or Arrow keys to move. Your character (blue circle) appears when you press a key; other players appear as they move.

## Build

From the repo root:
```bash
premake5 gmake2
make config=debug_x64
```

Binary: `bin/Debug/camp_sur_cpp_tiled_walk`
