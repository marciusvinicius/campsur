## Addons as items mods

- Addons are attaches for items that can be used to modify the item's behavior or appearance.
- Addons are stored in the `addons/items_addons.json`.
- Each item can have multiple addons.
- Addons are applied to the item when the item is used.
- Addons can be two categories:
    - Modifiers: modify the item's behavior or/and appearance.
    - Addons can have different types of modifiers:
        - Add a new behavior that don't need action from the player to be activated. (Like enemy detection on a range of the item)
        - Upgrades: Upgrade the item's behavior or appearance. (Like a new mode for the gun so now its silent when shooting.)
        - Change the type of item like a metal detector to a scanner that can scan for items.
- Items prefabs can have a list of possible addons compatibles
- Player can find new addons and attach to the item that support it
- Each item can have 0 to N addons activated, defined by each item
- The only item that addons are aded automaticly is the energy_torch, the addons are controlled by the game itself not by the player.
