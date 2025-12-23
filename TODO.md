# TODO

## Editor

[ ] Making possible to serialize scene on SaveScene
  [ ] Serialize world
      [√] serialize normal components
      [√] serialize animations state
      [ ] serialize terrain
[√] Fix Camera controll
[√] Make editor integrate with ImGui
  [√] Select entity
  [√] Make selected entity visual better
  [√] Move entity
  [√] Add component to selected entity
  [ ] Make play on editor works
    [ ] Play and Stop should go into scene in the MIDDLE
[ ] Working on tileset map editor
[ ] Working on animation tool
[ ] Move all UI to a separeted file

## Engine

[ ] Move all raylib reference to engine
[ ] Abstract raylib on engine (possibility to change the motor later)
[ ] Add 3D engine backend
[ ] World Graph (persist and load json for now)
[ ] Organize better the arquitecture of World and systems
[ ] Add a system manager
[ ] Add a component manager
[ ] World hold the managers and system comunicate with then

## Game

[ ] Simplest game end to end
  [√] Movment
  [√] Movement Working as system
  [√] Controll
  [ ] Events
  [ ] Collision
  [ ] Network
