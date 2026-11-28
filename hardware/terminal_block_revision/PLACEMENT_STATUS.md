# Placement candidates — NOT FOR ORDER

Both candidate .kicad_pcb files replace the original external connector footprints with the specified fixed screw terminal blocks. All original contacts mapped exactly once, including original unconnected contacts. Pin labels on silkscreen identify block and pin; mapping CSV retains original contact and net. All tracks and vias deliberately removed from these separate placement-only files; full routing required. Original layouts/CAM untouched. C3 remains an SMD baseline; through-hole redesign still pending. No KiCad DRC or physical fit verification performed. Candidate footprint geometry and chosen placement/orientation not approved by user.

```json
[
  {
    "board": "sensor",
    "source": "4f922912ca55c9bfb255d58473fffe02ec8843e3",
    "mapped": 38,
    "connected": 38,
    "free": 0,
    "oldConnectors": 12,
    "newConnectors": 4,
    "removedRoutingItems": 1577,
    "footprints": 23,
    "affectedNets": 19,
    "placementChecks": "rectangular/circular courtyard bounds, original board outline and sensor USB corridor: PASS (approximate candidate geometry)"
  },
  {
    "board": "c3",
    "source": "c80686158a4673c9c63ff8fc010b6f9c93359b61",
    "mapped": 25,
    "connected": 24,
    "free": 1,
    "oldConnectors": 12,
    "newConnectors": 3,
    "removedRoutingItems": 437,
    "footprints": 45,
    "affectedNets": 14,
    "placementChecks": "rectangular/circular courtyard bounds, original board outline and sensor USB corridor: PASS (approximate candidate geometry)"
  }
]
```
