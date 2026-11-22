# v0.5 Content and Balance Specification

This document records the intended content values for v0.5. The catalog is authoritative for unit, building, and upgrade definitions. The values are implemented and covered by the native simulation and interface checks; see Validation-v0.5.md.

## Storehouse research

Each faction receives one Age I, one Age II, and one Age III economic research item. All 18 are faction-only, use `storehouse_of_provisions` as producer, and carry one decimal `economy_bonuses` value. Costs are `[Food, Timber, Precious Things]`.

| Faction | Age I | Age II | Age III |
|---|---|---|---|
| Nephites | Food gathering +20% | Timber gathering +20% | Precious Things gathering +20% |
| Lamanites | Food gathering +20% | Carry capacity +25% | Timber gathering +20% |
| Gadianton | Food gathering +20% | Carry capacity +25% | Raid yield +20% |
| Coriantumr Jaredites | Beast income +20% | Build speed +20% | Carry capacity +25% |
| Shiz Jaredites | Beast income +20% | Build speed +20% | Carry capacity +25% |
| House of Laban | Food gathering +20% | Trader income +25% | Precious Things gathering +20% |

Research costs and times rise by age: Age I `[100, 50, 0]` / 25 seconds; Age II `[180, 90, 30]` / 40 seconds; Age III `[250, 150, 100]` / 55 seconds.

## Laban and concubines

The Laban hero no longer grants the general military age-choice bonus. Laban’s age-choice perk grants Drunken Concubines +10% Seduction channel speed globally. Within 8m of Laban, allied Drunken Concubines gain +1 armor, +25% Seduction channel speed, and their aura radius increases from 5m to 7m.

## Jaredite beast population and flock limits

Curelom, Cumelom, and Elephant have population cost 0. Each Gathering of Flocks allows at most 10 living and queued beasts combined across types. Each Seat of the People has a separate bootstrap limit of 10 living and queued beasts. The starting Curelom counts against its Seat.

## Lamanite Temple upgrades

The stable Temple of the Land ID displays as Rameumptom for Lamanites. Order of the Nehors is an Age II research produced by the Temple of the Land, with no prerequisites. By the Pride of Their Hearts is Age III, produced by the Temple of the Land, and requires Order of the Nehors. Both remain in the Lamanite upgrade whitelist.

Both Temple research IDs are explicitly included in the Lamanite faction whitelist. This omission in v0.4 was the reason the upgrades did not appear. The viewport tests click both research cards and verify the resulting queues.

## Map and food access

Groves contain two trees instead of three and starting groves nine instead of twelve, with wider spacing. Large beasts route using their own collision clearance. Two finite Wild Game sources spawn near each starting position; workers hunt and gather them by right-clicking. Conspiring Civilians can gather wild food and food from other players' completed farms/flocks, including allies, but cannot build their own farms/flocks or gather trees/ore. Depleted nodes and linked ore hills are removed.
