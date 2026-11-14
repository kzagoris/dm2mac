# Automap wheel zoom

Implement the agreed 1x/2x/3x nearest-neighbor zoom, anchored under the pointer.
Keep the readout and item list at their existing size. Ignore wheel input outside
an open map viewport. Reopening the map resets zoom to 1x.

1. Add failing focused tests for wheel routing, zoom limits, cursor-anchored item
   selection, scaled markers, dragging, and ignoring the wheel on the item list.
2. In automap.c, scale native tile rendering and share the zoomed coordinate
   transform between rendering and selection. Expose zoom and cell size queries.
3. Route SDL wheel events through platform/main/input to the advanced automap;
   cancel an active drag on zoom and use scaled cell sizes for subsequent drags.
4. Document wheel controls, run focused regression checks, build Windows, then
   launch a separate game instance and exercise wheel input, clicks and dragging.
