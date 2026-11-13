# Automap items

The automap helps the player find loot left behind. Show current floor and alcove items on explored squares; the existing Reveal map cheat also exposes items on unexplored squares. Use the same square ownership rules as the map picture.

## Approaches

Enabling the minion map item option is smaller, but skips missing graphics, overlaps piles, and includes missiles and explosions. The agreed approach uses a consistent marker, a count, and a clickable item list.

## Behavior

- A yellow item marker appears on every square with loose items, including containers. Piles show a digit, or `+` for ten or more; inspection shows the exact count.
- Preserve the party frame and distinguish pressure pads from item markers.
- A short left click opens the square's item list. Dragging still scrolls the map. List pages expose every item, grouping matching names with quantities.
- The list has mouse controls for previous/next pages and return to the map. Escape/right click return to the map; Tab/M close the automap.
- Contents of containers, champion inventories, creature possessions, missiles, and effects are excluded.
- Read current dungeon state on redraw. No item history, new saved state, or save format change is needed.
- Closing the map or loading a game clears inspection and scrolling.

Implementation uses the existing C11 UI, offscreen rendering, game font, and rows screen. Keep unrelated local changes and never restart the user's running game.
