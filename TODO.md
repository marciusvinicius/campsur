# TODO

### Editor
- [x] Fix Camera controll
- [x] Make editor integrate with ImGui
  - [x] Select entity
    - [x] Make selected entity visual better
    - [x] Move entity
    - [x] Add component to selected entity
    - [ ] Add component on selected entity 
  - [ ] Make play on editor works
    - [ ] Play and Stop should go into scene in the MIDDLE
- [ ] Making possible to serialize scene on SaveScene
- [ ] Working on tileset map editor

### Engine
- [ ] Move all raylib reference to engine
- [ ] Abstract raylib on engine (possibility to change the motor later)
- [ ] Add 3D engine backend
- [ ] World Graph (persist and load json for now)
- [ ] Organize better the arquitecture of World and systems
  - [ ] Add a system manager
  - [ ] Add a component manager
  - [ ] World hold the managers and system comunicate with then

### Game
- [ ] Simplest game end to end
  - [x] Movment
    - [x] Movement Working as system
  - [x] Controll
  - [ ] Events
  - [ ] Collision
- [ ] Network
