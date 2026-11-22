# Starting positions in v0.9

2026-11-22. Recommended model: GPT-6.2 Sol. Implementation and verification: Codex parent agent.

The six starting laborers now face their own settlement's resources. The starting Jaredite Curelom uses the same local orientation behind the Seat. This applies to all 15 maps. No starting stock, unit count, unit statistic or guaranteed resource supply changes.

Previously, all four settlements placed workers in one fixed world-axis formation. Calculated mean distance to the nearest berry bush was 9.56, 12.45, 14.90 and 5.70 metres by slot. The first runtime check after rotating the workers exposed another difference: an offset tree from a regular grove intruded into one original map's starting area. The grove centre was outside the clearing, but the tree was inside it. Final tree positions now respect a 22 m opening clearance; each settlement retains its nine deliberate starting trees. New-map groves remain mirrored as whole groups.

The actual spawned worlds now give all four slots mean nearest-source distances of **5.834 m to berries, 15.021 m to timber, and 8.938 m to ore**. The fixture compares every corresponding worker, not just the averages, against all neutral resources in the map. It also checks clear worker and beast spawn positions and samples each worker's first berry approach against real collision. These supplement the existing land, resource, dock and ship checks.

The final executable `CC8D75CDE64C6AF89877698515480D7853C672A63E667A015C41BDBF051042B1` passed all **4,234** map assertions. [Report](Evidence/v0.9/maps/opening-raids-final/maps-1600x900/v09-maps.json) · [Executable and exit](Evidence/v0.9/maps/opening-raids-final/maps-1600x900/run.json). The preceding attempt had three timber-distance failures, all on the original maps; the report is retained. [Initial failure](Evidence/v0.9/maps/opening-raid-diagnosis/maps-1600x900/v09-maps.json)

This establishes equal initial geometric access, not equal whole-match economies. Resource work-slot angles, route-grid rounding, later producer exit positions, faction rates and player orders can still affect delivery times. A future opening replay should measure identical gather orders at all four positions before making a broader fairness claim.

Historical balance probes can add `-RTSV09BotLegacyOpening` together with `-RTSV09BotBalance` to retain the earlier formation and grove placement. The command has no effect in ordinary games. The report records `opening_formation`; the PowerShell balance wrapper exposes `-LegacyOpening`. Compare policy arms using the same opening and executable.
