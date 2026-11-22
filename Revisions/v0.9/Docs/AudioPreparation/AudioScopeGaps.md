# Audio scope gaps — A185 preparation

The routine production ledger contains 153 rows, including nine nonverbal builder-tier cues. Each row has an empty audio binding, missing license/release references and an explicit unrecorded state. Source route is a proposed original performance, not a completed performance. `ContextualExtras.json` separately preserves 24 master-sheet contextual rows, including three dormant campaign/cosmetic rows. Several contextual rows repeat routine lines; deduplicate before paying for takes. The future non-roster character table remains in the canonical master sheet, outside required coverage.

| Category | Prepared scope | Missing acceptance |
| --- | --- | --- |
| Routine speech | 144 spoken rows in RecordingScript.md | Performer/source selection, approved name pronunciations, paraphrase/allusion classification, recording/release, edit and runtime binding |
| Creature cues | Nine Cumelom/Curelom/Elephant event cues | Actual nonverbal recordings, source clearance and runtime behavior |
| Contextual extras | 24 rows with original scopes and references | Resolve overlaps and approve intended triggers; dormant rows remain excluded |
| UI | Master-sheet narrator research_complete/loading/pause/liahona_hint texts retained in extras | Working menus/feedback event list, nonverbal interface sounds, narration decision and actual sources |
| Music | No canonical track list; no music acquired | A193 music brief, original/licensed small set, loops/mix/source records |
| World effects | A189/A193 require creature, ship/chariot, ambience, combat and building sounds | Approved cue inventory tied to actual gameplay/VFX; provenance and hidden-information checks |

No invented list is presented as a complete game sound inventory. Complete this ledger after gameplay and presentation expose the required cues. Link every final sound to AssetLedger.csv, actor permission or original source, editable master, export/hash and runtime asset ID. Keep invoices and personal performer details private.

Run `python Tools/prepare_sol_records.py` from the workspace to regenerate extraction outputs. The script overwrites preparation files; keep real takes, approvals and license records in a separately reviewed production ledger when A185 starts. Do not insert production edits into these generated files and rerun without preserving them.
