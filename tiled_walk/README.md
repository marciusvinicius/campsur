# Tiled Walk – Multiplayer

A simple 2–4+ player game: everyone walks on a tiled map. Uses the engine’s ENet multiplayer (server-authoritative snapshots, client input).

## Run

1. **Start the server** (one terminal). The server window is also a player: use WASD to move (blue circle).
   ```bash
   ./bin/Debug/camp_sur_cpp_tiled_walk --server [port]
   ```
   Default port: 7777.

2. **Start clients** (one or more terminals):
   ```bash
   ./bin/Debug/camp_sur_cpp_tiled_walk --client 127.0.0.1 [port]
   ```

3. **Controls:** WASD or Arrow keys to move. Your character appears when you press a key; other players appear as they move. Colors follow **connection order**: server = blue (first), first client = red, second = green, yellow, magenta, orange.

## Build

From the repo root:
```bash
premake5 gmake2
make config=debug_x64
```

Binary: `bin/Debug/camp_sur_cpp_tiled_walk`
