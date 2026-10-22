# Voice Lines

## Shared direction and implementation

Original, scripturally grounded intent for the current game (v0.8), including lines not yet recorded. Keep each line verbatim. Lines are original paraphrases, fictional dialogue, or comic allusions; none is asserted as a verbatim scripture quotation. Use basis_type paraphrase, comic_allusion, or nonverbal. A reference identifies the source scene, not proof of a unit ability. Preserve each event ID and Scripture reference as metadata; do not present references as quote attributions.

Use clear, restrained, lightly archaic but intelligible speech; let the situation carry affectionate Book of Mormon humor. No Relief Society, ministering, wards, modern offices, current church-leader memes, imitation of real religious leaders, stereotyped or racial accents, or skin-color jokes. Record original performances; no copyrighted song recording is required. Animals use nonverbal cues only—do not invent human scriptural dialogue for them.

Trigger select on selection, move on an accepted movement order, and attack on an accepted attack order or documented noncombatant response. Attack requests to support or noncombat units get their authored refusal/support line without damage or a fake attack animation. Resource orders use the matching resource event even if the worker then walks; forced context barks share the global cooldown. Do not queue missed barks. Only the local player hears ordinary order barks. Choose one speaker per command, prioritizing hero then highest population, with deterministic entity-ID tie-break; apply 2s per-speaker and 0.75s global cooldowns. A repeat-selection line may play after three clicks within 5s.

Every event needs a stable unit ID, event ID, text/cue, Scripture reference, basis type, audio asset ID, and subtitle. Provide text fallback, subtitles, and independent voice volume. Do not infer absent abilities from dialogue. Scripture references should be audited through [[Source Ledger]]. The excluded “anxiously engaged” line is outside the Book of Mormon source constraint; profanity status remains UI text, not voiced scripture. Historical/reserved lines do not add characters to current faction rosters.

## Character event lines

### Amalickiah

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | The kingdom is within reach. | Alma 47:35 |
| move | Bring them down from the mountain. | Alma 47:10-13 |
| attack | Their captain will not save them. | Alma 47:18-19 |

### Ammon

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | Need a hand? | Alma 17:37-39 |
| move | Back to the flocks. | Alma 17:31-32 |
| attack | You will not scatter them. | Alma 17:35 |
| select_repeat | I have several. | Alma 17:37-39 |

### Ammonite Covenant Keeper

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | Our swords remain buried. | Alma 24:17 |
| move | We will labor with our hands. | Alma 24:18 |
| attack | We will not take up arms. | Alma 24:6 |
| gather_food | Gather grain for our people. | Mosiah 9:9 |
| gather_timber | Timber for the work ahead. | Helaman 3:9-10 |
| gather_wealth | Precious ore lies beneath. | Ether 10:23 |
| return_cargo | Bring in the provisions. | 3 Nephi 4:4 |

### Bowman

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | Bow and arrows ready. | Alma 43:20 |
| move | I will take my position. | Alma 49:20 |
| attack | Loose the arrows! | Alma 49:20 |

### Captain Moroni

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | Remember our liberty. | Alma 46:12 |
| move | Strengthen the weaker places. | Alma 48:9 |
| attack | In memory of our God! | Alma 46:12 |

### Chief Captain

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | The army awaits command. | Alma 43:16 |
| move | Keep your ranks together. | Alma 43:31-33 |
| attack | Stand for your people! | Alma 43:48 |

### Cimeter Bearer

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | The cimeter is drawn. | Alma 43:20 |
| move | Around their flank. | Alma 43:22 |
| attack | Fight like dragons! | Alma 43:44 |

### Coriantumr

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | I yet remain. | Ether 15:29-32 |
| move | Gather those who are left. | Ether 15:27-29 |
| attack | This war ends here. | Ether 15:29-31 |
| last_survivor | Last man standing. | Ether 15:29-32 |

### Corrupt Judge

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | The judgment is in our hands. | Helaman 6:39 |
| move | To the seat of power. | Helaman 6:39 |
| attack | Let others bear the sword. | 3 Nephi 6:27-30 |
| plunder_food | Their provisions are unguarded. | 3 Nephi 4:4-5 |
| plunder_timber | Their deserted lands are ours to strip. | 3 Nephi 4:1 |
| plunder_wealth | Their riches will serve our design. | Helaman 6:17,38-39 |

### Cumelom

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | [Low chuff; no speech] | Ether 9:19 |
| move | [Heavy steps and grunt] | Ether 9:19 |
| attack | [Warning bellow] | Ether 9:19 |

### Curelom

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | [Deep resonant call; no speech] | Ether 9:19 |
| move | [Two short grunts] | Ether 9:19 |
| attack | [Defensive roar] | Ether 9:19 |

### Elephant

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | [Soft trumpet; no speech] | Ether 9:19 |
| move | [Low rumble] | Ether 9:19 |
| attack | [Sharp trumpet] | Ether 9:19 |

### Gadianton Robber

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | I loooooove precious things | User-authored original line; not scripture |
| move | Through the secret places. | 3 Nephi 4:1 |
| attack | Take their provisions! | 3 Nephi 4:4-5 |
| plunder_food | Their provisions are unguarded. | 3 Nephi 4:4-5 |
| plunder_timber | Their deserted lands are ours to strip. | 3 Nephi 4:1 |
| plunder_wealth | Their riches will serve our design. | Helaman 6:17,38-39 |

### Gadianton

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | Our design remains secret. | Helaman 2:4-5 |
| move | To the wilderness. | Helaman 2:11 |
| attack | Power shall be ours. | Helaman 2:5 |
| revealed | This meeting was supposed to be confidential. | Helaman 2:4-5 |

### Giddianhi

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | Yield your lands. | 3 Nephi 3:6-7 |
| move | We will come down upon them. | 3 Nephi 4:1 |
| attack | Their provisions will sustain us. | 3 Nephi 4:5 |

### Hagoth's Vessel

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | The vessel is ready. | Alma 63:5 |
| move | Toward the land northward. | Alma 63:6 |
| attack | We carry people, not war. | Alma 63:6 |

### Helaman

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | They are firm and undaunted. | Alma 57:20 |
| move | We march with exactness. | Alma 57:21 |
| attack | Remember what your mothers taught. | Alma 56:47 |

### Industrious Laborer

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | My hands will not be idle. | 2 Nephi 5:17 |
| move | I will go and do. | 1 Nephi 3:7 |
| attack | I will defend my household. | Alma 43:47 |
| gather_food | Gather grain for our people. | Mosiah 9:9 |
| gather_timber | Timber for the work ahead. | Helaman 3:9-10 |
| gather_wealth | Precious ore lies beneath. | Ether 10:23 |
| return_cargo | Bring in the provisions. | 3 Nephi 4:4 |
| many_orders | All of these commandments? | 1 Nephi 3:7 |

### Jaredite Barge

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | Still tight. | Ether 2:17 |
| move | Across the great deep. | Ether 6:5-8 |
| attack | Keep the passengers sheltered. | Ether 6:4-7 |

### Keeper of Flocks

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | The flocks are gathered. | Alma 17:25 |
| move | To the waters with the flocks. | Alma 17:26 |
| attack | You will not scatter them again. | Alma 17:33-35 |
| gather_food | Gather grain for our people. | Mosiah 9:9 |
| gather_timber | Timber for the work ahead. | Helaman 3:9-10 |
| gather_wealth | Precious ore lies beneath. | Ether 10:23 |
| return_cargo | Bring in the provisions. | 3 Nephi 4:4 |

### King's Chariot

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | The king’s chariot is prepared. | Alma 18:9 |
| move | Make ready the journey. | Alma 18:12 |
| attack | I bear the king; I do not charge. | Alma 18:9-12 |

### Kishkumen

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | No one has discovered me. | Helaman 1:12 |
| move | The servant will show the way. | Helaman 2:6-7 |
| attack | The chief judge is within reach. | Helaman 2:8 |

### Laban

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | My fifty will answer. | 1 Nephi 3:31 |
| move | To my treasury. | 1 Nephi 4:20 |
| attack | My sword is drawn. | 1 Nephi 4:9 |

### Lamoni

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | My servants have spoken of strength. | Alma 18:2-3 |
| move | Prepare my chariot. | Alma 18:9 |
| attack | Defend the king’s household. | Alma 17:25; Alma 18:9 |

### Man of Moroni

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | My breastplate is fastened. | Alma 43:19 |
| move | We will hold the entrance. | Alma 49:20 |
| attack | For a better cause. | Alma 43:45 |

### Man of the Sword

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | My sword is ready. | Alma 43:18 |
| move | To defend our lands. | Alma 43:26 |
| attack | For our homes! | Alma 43:45 |

### One of the Fifty

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | One of the fifty, at your command. | 1 Nephi 3:31 |
| move | For the house of Laban. | 1 Nephi 4:20 |
| attack | You shall not pass our guard. | 1 Nephi 3:25 |

### Rameumptom Priest

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | There is room for one. | Alma 31:13 |
| move | Make way for the holy stand. | Alma 31:21 |
| attack | I will pronounce our superiority. | Alma 31:16-18 |
| select_repeat | I’m considerably more chosen than you. | Alma 31:16-18 |
| occupy | One at a time, please. | Alma 31:13 |

### Samuel the Lamanite

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | I have words for this people. | Helaman 13:4-5 |
| move | I will stand upon the wall. | Helaman 13:4 |
| attack | Hear my words; I bear no bow. | Helaman 16:2 |

### Secret Murderer

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | The oath is remembered. | Helaman 2:3 |
| move | No one must know my path. | Helaman 2:6-9 |
| attack | The judgment seat will be empty. | Helaman 2:3-5 |
| plunder_food | Their provisions are unguarded. | 3 Nephi 4:4-5 |
| plunder_timber | Their deserted lands are ours to strip. | 3 Nephi 4:1 |
| plunder_wealth | Their riches will serve our design. | Helaman 6:17,38-39 |

### Shiz

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | Who can stand before us? | Ether 14:18 |
| move | Sweep through the land. | Ether 14:17-18 |
| attack | I will have Coriantumr. | Ether 14:24 |
| last_strike | Not quite dead. | Ether 15:31 |

### Slinger of Stones

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | A stone is enough. | Alma 17:36 |
| move | Give me room to cast. | Alma 17:36 |
| attack | Let the stones answer. | Alma 17:36 |

### Stripling Warrior

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | Our mothers knew. | Alma 56:48 |
| move | We will obey every command. | Alma 57:21 |
| attack | We do not doubt. | Alma 56:47 |

### Teacher of the Word

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | I am prepared to teach. | Alma 17:2-3 |
| move | To those who will hear. | Alma 17:11 |
| attack | Let their hearts be changed. | Alma 23:5-7 |

### Teancum

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | Let their captain sleep. | Alma 51:33 |
| move | Quietly, into their camp. | Alma 51:33 |
| attack | One captain. One chance. | Alma 51:34 |

### Tiller of the Earth

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | The seed is ready. | Mosiah 9:9 |
| move | To the fields of our inheritance. | Mosiah 9:6-9 |
| attack | Our fields shall not be taken. | Mosiah 9:14-18 |
| gather_food | Gather grain for our people. | Mosiah 9:9 |
| gather_timber | Timber for the work ahead. | Helaman 3:9-10 |
| gather_wealth | Precious ore lies beneath. | Ether 10:23 |
| return_cargo | Bring in the provisions. | 3 Nephi 4:4 |

### Trader in Precious Things

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | Precious things for trade. | Mosiah 24:7 |
| move | Another city, another exchange. | Mosiah 24:7 |
| attack | Trade is my errand, not bloodshed. | Mosiah 24:7 |

### Wilderness Spy

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | I have watched their camp. | Alma 43:28 |
| move | Into the wilderness. | Alma 43:23 |
| attack | They have found us. | Alma 43:28 |

### Worker in Ore

| Event ID | Line / cue | Scriptural basis |
| --- | --- | --- |
| select | There is ore in this hill. | Ether 10:23 |
| move | Where shall we dig? | Ether 10:23 |
| attack | These hands can wield more than tools. | Mosiah 9:16 |
| gather_food | Gather grain for our people. | Mosiah 9:9 |
| gather_timber | Timber for the work ahead. | Helaman 3:9-10 |
| gather_wealth | Precious ore lies beneath. | Ether 10:23 |
| return_cargo | Bring in the provisions. | 3 Nephi 4:4 |

## Retained contextual lines

| Event | Line | Basis | Scope |
| --- | --- | --- | --- |
| worker_many_orders | All of these commandments? | 1 Nephi 3:7 | Active Laborer |
| laman_lemuel_move | Another wilderness? | 1 Nephi 16:20 | Reserved campaign characters |
| nephi_build_ship | Not after the manner of men. | 1 Nephi 18:2 | Reserved campaign Nephi |
| barge_select | Still tight. | Ether 2:17 | Active Jaredite Barge |
| rameumptom_occupy | One at a time, please. | Alma 31:13 | Legacy priest occupancy |
| zoramite_repeat | I’m considerably more chosen than you. | Alma 31:16-18 | Active priest |
| ammon_select | Need a hand? | Alma 17:37-39 | Legacy Ammon; pun |
| ammon_repeat | I have several. | Alma 17:37-39 | Legacy Ammon; pun |
| gadianton_revealed | This meeting was supposed to be confidential. | Helaman 2:4-5 | Active Gadianton; comedic paraphrase |
| tent_select | And my father dwelt in a tent. | 1 Nephi 2:15 | Future Lehi tent cosmetic narrator |
| research_complete | For a wise purpose. | 1 Nephi 9:5 | UI narrator, not victory |
| loading | And it came to pass… | 1 Nephi 1:4 | UI narrator |
| pause | And thus we see… | Alma 24:19 | UI narrator |
| moroni_defend | A better cause. | Alma 43:45 | Active infantry |
| captain_attack_buff | Fight like dragons! | Alma 43:44 | Lamanite captain upgrade |
| stripling_hold | Firm and undaunted. | Alma 57:20 | Active Stripling |
| stripling_order | Every word of command. | Alma 57:21 | Active Stripling |
| helaman_buff | Their mothers knew. | Alma 56:48 | Legacy Helaman |
| moroni_banner | In memory of our God. | Alma 46:12 | Active Moroni |
| liahona_hint | Look to the Liahona. | Alma 37:38-41 | UI narrator |
| barge_complete | Tight like unto a dish. | Ether 2:17 | Active Barge |
| gadianton_network | Secret combination. | Helaman 6:38 | Active Gadianton |
| coriantumr_last | Last man standing. | Ether 15:29-32 | Active hero; paraphrase |
| shiz_last | Not quite dead. | Ether 15:31 | Active hero; joke |

## Reserved non-roster character lines

These are reserved scripts, not recruitable units or balance entities.

| Character | Select | Move | Attack response | Basis |
| --- | --- | --- | --- | --- |
| Nephi | I will go and do. | The Lord has prepared a way. | I will defend my people. | 1 Nephi 3:7; 2 Nephi 5:14 |
| Laman and Lemuel | Why must we follow? | Another wilderness? | We will contend with him. | 1 Nephi 16:20; 1 Nephi 7:16 |
| Brother of Jared | I have prepared these stones. | To the mount. | I seek light, not battle. | Ether 3:1-4; noncombatant response is invented |
| Three Nephites | There is work yet to do. | We go among the people. | We will minister, not strike. | 3 Nephi 28:9,18,23; noncombatant interpretation |

## Coverage check

Active roster coverage requires select, move, and attack events; workers require Food, Timber, and Precious Things events; all three beast types use distinct nonverbal cues. Attack refusals must match simulation behavior.
