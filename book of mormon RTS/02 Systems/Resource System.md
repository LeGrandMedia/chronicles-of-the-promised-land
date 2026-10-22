# Resource System

Spendable global stockpiles: **Food, Timber, Precious Things** (legacy key Wealth), plus Jaredite **Cement**. Religious stats are separate/nonspendable; [[Favor of the Lord]]. No coin denominations or ore-to-gold processing.

## Gathering, income and delivery

- Laborers gather finite berries, wild game, trees and ore; farms/vineyards renew. Hill of Ore needs a natural deposit and improves gathering. Exhausted nodes disappear; finish remaining cargo without paying twice. Rates, reserves, carry/work-slot limits: [[Data and Assets]]. Travel affects net income.
- Workers carry one gathered resource type per trip. Keep fractional output, repair payment and stolen shares internally; whole-unit spending/display. Payment fails atomically if insufficient.
- Gathering becomes spendable on completed valid drop-off delivery. Own HQ/Storehouse accepts ordinary types. Cargo death creates one visible contested 30-second crate; capacity-limited pickups never duplicate it. An expiring capacity buff preserves surplus cargo but pauses loading.
- Sheep and Jaredite beasts produce passive Food/other resources as listed in [[Data and Assets]]. House of Laban Winery generates 0.5 Drunkenness/s and no Food; Age-III Winery Abundance adds 3 Food/s, while Age-III Fermented Reserves raises Winery Drunkenness by 20%. Ten normal sheep per flock; enemy destruction scatters sheep to the attacker. Sheep need a surviving home flock unless scattered.
- **Galavant** replaces distance-based market trading: chosen waypoints take traders among own laborers, yielding Precious Things per nearby worker visit with shared per-worker cooldown. Markets pass routes to new traders. Idle traders count as idle workers; overlaps cannot farm one visit.
- Show workforce counts, gross/diverted/net income and selected passive rates. Aggregate each trickle resource until ten before notifying.

## Faction economy

| Faction | Exception |
| --- | --- |
| Nephites | Expensive/slower recruitment; faster-gathering Ammonites; genuine Food deliveries can award Favor |
| Lamanites | Cheaper/faster recruitment; Anger improves gathering/training |
| Gadianton Robbers | Conspiring Civilians gather at 35% of ordinary rate; no own farms/productive flocks or traders. They gather Timber/Precious Things, hunt/forage and take foreign farm/flock Food. Main income: [[Gadianton Plunder and Stealth]] |
| Coriantumr Jaredites | Precious Things gathering bonus; [[Jaredite Cement Construction]] |
| Shiz Jaredites | Shared Jaredite beast/Cement economy |
| House of Laban | Precious Things bonus; Vineyards, a Winery that generates Drunkenness, Age-III Food research, traders and Concubine laborer auras |

Gathering combines base rate, faction/unit factors and additive applicable research; permanent age bonuses/documented auras apply independently. Bonuses never turn non-gatherers into workers. Lobby starting reserves, speed and natural-reserve fruitfulness: [[Game Overview]]. Keep HQ recovery viable, especially Curelom recruitment.

Gadianton workers retain the requested 35% gathering rate, including when gathering Timber, Precious Things or eligible foreign farms/flocks. The v0.8 rate passed native validation; see [[Validation Report]].
