# Gadianton Plunder and Stealth

[[Gadianton Robbers]] depend on raids, infiltration and stolen deliveries. Income needs real enemy stock/output; idle/revealed buildings and raw damage grant nothing. Later requested yield upgrades explicitly permit bonus cargo beyond the victim debit, superseding strict no-multiplier intent.

## Raids and deliveries

**Band of Kishkumen** (`gadianton_robber`) is a fast raider trained at Encampments or Storehouses. Set a raid route from a Gadianton Storehouse by choosing an enemy economic site; new raiders inherit the route and return to that Storehouse. Raiders steal available laborer cargo before dropoff, and plunder economic sites. Attack Move is a unit command; structures do not receive it. Show cargo and notify victims.

After three-second setup, steal available Food/Timber/Precious Things into limited cargo. No stock means no stock theft. On production streams retain single interception: suppress 25% gross, divert 30%, leave 45% before explicit stolen-yield bonuses. One effective channel per stream; no farm-plus-drop-off double charge. Full cargo pauses loading; unused post-suppression output stays with victim. Giddianhi suppression alone produces no cargo.

Movement, attacks, damage, range/LOS loss or target death interrupts a raid. Lawyers reveal from stealth when Bribe begins and stay exposed through their conversion channel. Gadianton HQs and Encampments are concealed after five seconds without hostile damage; taking damage reveals them for five seconds. Towers and spies detect concealed structures/units normally. Cargo is spendable at friendly HQ/Storehouse/Encampment; death drops a contested crate under [[Resource System]]. Stolen cargo never awards Favor delivery credit.

## Lasciviousness and infiltration

Generation/decay: [[Favor of the Lord]]. Research may be purchased below its activation threshold:

| Current stat | Researched effect |
| --- | --- |
| Above 30 | Plunder Share/Hoard multiplies stolen yield; Storehouse research adds raid yield |
| Above 50 | Network of Secrets shares enemy vision |
| Above 70 | Compromised Storehouses: stealth camper near enemy HQ/Storehouse diverts half of deposits |
| At 100 | Diverts entire compromised deposits |

Split one real delivery; multiple campers never multiply its debit. **Signs and Symbols** has this exact description: “Enemy laborers will mistakenly drop resources off at your storehouses, if they are the closest dropoff point.” It makes enemy workers choose the nearest Gadianton Storehouse, crediting robbers. Lawyer conversion retains deceptive former-owner appearance; lost command reveals infiltration and the former owner can manually delete the infiltrated entity. Conversion immunity remains.

Secret Combination Lodges train Lawyers and stop generating once scouted. Judgment Seat/anti-infiltration research is retired; towers detect stealth. Hideouts hold five population of Gadianton combat units.

## Shared visibility rules

Unexplored, explored memory and live LOS are distinct. Terrain/structures block sight; towers see over low walls, never opaque high terrain. Remember last-scouted buildings, trees, ore and herd animals without updating state or showing live enemies; revisit to refresh/remove memory. Lobby can disable fog. Liahona failure changes no real vision. Foundations may be placed outside LOS, subject to terrain, overlap and cost checks.

Stealth hides bodies, not attack effects; owners see reduced opacity. Detection requires LOS: spies 6m, towers 8m. Damage dealt/received reveals for five seconds. Network of Secrets is an explicit later exception to normal enemy-vision secrecy.

Runtime stock raids debit global reserves at eligible economic sites; this remains an implementation interpretation. Production interception conserves the victim debit before explicit yield bonuses. v0.8 structure concealment and laborer-cargo interception passed native and multiplayer validation; see [[Validation Report]].
