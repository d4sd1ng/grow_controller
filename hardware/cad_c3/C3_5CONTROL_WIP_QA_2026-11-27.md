# C3 five-control stage extension — quality assurance snapshot

**2026-11-27 · NOT FOR PCB ORDER**

### Files and scope
- Revised draft: `c3_6motor_5control_switches_WIP_NOT_FOR_ORDER.kicad_pcb`
- 65 footprints; five newly placed low-voltage control output stages alongside the six previously modeled motor stages
- The additional stages have preliminary GPIO/pad-net assignments and **incomplete tracks**
- The preliminary B.Cu ground zone was created but has **not been stored as a validated filled copper polygon**

### Latest verified result
- KiCad 9 strict DRC workflow: https://github.com/d4sd1ng/homepage/actions/runs/46031882023
- Result: **FAIL** — 19 reported DRC violations and 12 unconnected items
- Main categories: crossing tracks, shorts between different nets, inadequate drill/track clearance, incomplete via connections, solder-mask bridging
- A prior candidate's passing DRC report **does not apply** to this WIP revision

### Release state
No Gerbers from this revision are approved for fabrication; no validated BOM, enclosure dimensions, or complete electrical design review exists for the combined board. The PR remains draft and the automated release check remains false.
