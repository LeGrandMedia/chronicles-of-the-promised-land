"""Wire generated PNGs into the v0.6 data manifest without modifying image pixels."""
from pathlib import Path
import argparse, json, hashlib, html, struct
ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "Game/Content/Data"
ART = DATA / "Art"
p = argparse.ArgumentParser()
p.add_argument("--allow-missing", action="store_true")
args = p.parse_args()
inventory = json.loads((ROOT / "Docs/ArtInventory.json").read_text(encoding="utf-8"))
catalog = json.loads((DATA / "catalog.json").read_text(encoding="utf-8"))
defs = {d["id"]: d for group in ("units","buildings","upgrades") for d in catalog[group]}
building_ids = {d["id"] for d in catalog["buildings"]}
sprites, icons, cards = {}, {}, []
building_sizes = {
 "seat_of_the_people":132, "temple_of_the_land":126, "secret_combination_lodge":104,
 "fields_of_grain":106, "gathering_of_flocks":112, "storehouse_of_provisions":106,
 "place_of_trade":116, "encampment_of_war":112, "house_of_curious_workmanship":108,
 "tower_of_watchmen":92, "works_of_defense":90, "cement_wall":82, "cement_tower":94,
 "winery":108, "place_of_many_waters":128, "tents_and_habitations":94, "hill_of_ore":86
}
large_units = {"elephant":82,"curelom":76,"cumelom":76,"kings_chariot":88,
 "hagoths_vessel":108,"jaredite_barge":100,"sheep":38,"laman_and_lemuel":62,
 "jaredite_warrior":58,"jaredite_shield_bearer":56,"jaredite_archer":54}
def add_sprite(key, base, path, name, group, faction=""):
 d = defs.get(base, {})
 building = base in building_ids
 resource = group == "Resources"
 size = building_sizes[base] if building else large_units.get(base, 58 if d.get("kind")=="hero" else 48)
 if resource: size = {"tree":60,"ore":52,"berries":48,"crate":36,"wild_game":46}[base]
 pivot = [.5,.94]
 if building:
  pivot = [.5,.77]
  if base in ("tower_of_watchmen","cement_tower"): pivot=[.5,.88]
  if base in ("fields_of_grain","gathering_of_flocks","works_of_defense","cement_wall","place_of_many_waters"): pivot=[.5,.68]
 if base in ("elephant","curelom","cumelom","sheep","kings_chariot","wild_game"): pivot=[.52,.88]
 if base in ("hagoths_vessel","jaredite_barge"): pivot=[.5,.72]
 sprites[key]={"texture":path,"size":[size,size],"pivot":pivot,"uv":[0,0,1,1],
  "tint_mode":"none","color":[1,1,1,1],
  "hitbox":{"min":[.18,.22],"max":[.82,.98]},
  "context_hitbox":{"min":[.06,.08],"max":[.94,.98]},
  "animations":{}}
 icons[key]={"texture":path,"uv":[0,0,1,1]}
 cards.append({"id":key,"name":name,"group":group,"faction":faction,"path":path})
for group, label in (("units","Units"),("buildings","Buildings"),("resources","Resources")):
 for item in inventory[group]: add_sprite(item["id"],item["id"],item["path"],item["display_name"],label)
for item in inventory["faction_name_variants"]:
 add_sprite(item["visual_key"],item["id"],item["path"],item["display_name"],"Faction buildings" if item["id"] in building_ids else "Faction units",item["faction_id"])
for item in inventory["research_icons"]:
 icons[item["id"]]={"texture":item["path"]}
 cards.append({"id":item["id"],"name":item["display_name"],"group":"Research","path":item["path"]})
for item in inventory["age_choice_aliases"]:
 icons[item["id"]]={"texture":item["path"]}
for group, prefix, label in (("action_icons","actions/","Commands"),("resource_stat_icons","stats/","Resource badges"),("faction_emblems","factions/","Faction emblems")):
 for item in inventory[group]:
  key=prefix+Path(item["path"]).stem
  icons[key]={"texture":item["path"]}
  cards.append({"id":key,"name":item.get("display_name",Path(item["path"]).stem.replace("_"," ").title()),"group":label,"path":item["path"]})
manifest={"schema_version":1,"art_root":"Art","art_revision":"v0.6","sprites":sprites,"icons":icons}
missing=[]; hashes={}
for card in cards:
 file=ART/card["path"]
 card["ready"]=file.is_file()
 if not card["ready"]: missing.append(card["path"]);continue
 raw=file.read_bytes()
 if raw[:8]!=b"\x89PNG\r\n\x1a\n":raise ValueError("Not PNG: "+str(file))
 w,h,bit,color=struct.unpack(">IIBB",raw[16:26])
 card["width"],card["height"]=w,h
 card["sha256"]=hashlib.sha256(raw).hexdigest()
 if color not in (4,6):raise ValueError("PNG has no explicit alpha: "+str(file))
 if card["sha256"] in hashes:raise ValueError("Duplicate image for distinct entries: "+card["id"]+" and "+hashes[card["sha256"]])
 hashes[card["sha256"]]=card["id"]
(DATA/"visuals.json").write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8")
coverage={"planned_images":len(cards),"present_images":len(cards)-len(missing),"missing":missing,
 "sprites":len(sprites),"icons_including_reused_portraits":len(icons),"files":cards}
(ROOT/"Docs/ArtCoverage-v0.6.json").write_text(json.dumps(coverage,indent=2)+"\n",encoding="utf-8")
entries=[]
for c in cards:
 src="../Game/Content/Data/Art/"+c["path"]
 item='<a class="art" href="'+html.escape(src)+'" target="_blank"><img loading="lazy" src="'+html.escape(src)+'" alt="'+html.escape(c["name"])+'"></a>' if c["ready"] else '<div class="pending">In production</div>'
 entries.append('<article data-group="'+html.escape(c["group"])+'" data-find="'+html.escape((c["name"]+" "+c["id"]+" "+c.get("faction","")).lower())+'">'+item+'<h3>'+html.escape(c["name"])+'</h3><p>'+html.escape(c.get("faction","").title() or c["group"])+'</p><code>'+html.escape(c["id"])+'</code></article>')
options=''.join('<option>'+html.escape(g)+'</option>' for g in sorted({c["group"] for c in cards}))
page='''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Chronicles — Art Library v0.6</title>
<style>*{box-sizing:border-box}body{margin:0;background:#111f24;color:#f2e7ce;font:15px system-ui}header{padding:38px 5vw 25px;border-bottom:1px solid #6f6246;background:#172a30}h1{margin:8px 0;font:600 34px Georgia}header p{color:#b8c6c4;max-width:760px;line-height:1.6}.eyebrow{color:#d5ae62;letter-spacing:.16em;font-size:12px;text-transform:uppercase}.tools{display:flex;gap:12px;flex-wrap:wrap;margin-top:22px}input,select{background:#0d191e;color:#f4e6ca;border:1px solid #64716a;border-radius:6px;padding:12px;font:inherit}input{min-width:280px;flex:1}main{display:grid;grid-template-columns:repeat(auto-fill,minmax(190px,1fr));gap:18px;padding:28px 5vw}article{border:1px solid #3b514f;border-radius:8px;overflow:hidden;background:#1c3034;padding-bottom:16px}.art,.pending{height:210px;display:flex;align-items:center;justify-content:center;background:radial-gradient(ellipse,#65746c55,#263d3b20)}img{max-width:100%;max-height:100%;object-fit:contain;padding:12px}h3{font-size:15px;margin:12px 14px 5px}article p{font-size:12px;color:#c5b38d;margin:0 14px 8px}code{display:block;margin:0 14px;font-size:10px;overflow-wrap:anywhere;color:#99b0af}.pending{color:#a6b6b3;font-style:italic}article[hidden]{display:none}footer{padding:20px 5vw;color:#b6c3c0;font-size:13px}</style>
<header><div class="eyebrow">Chronicles of the Promised Land · v0.6</div><h1>Art library</h1><p>Unique low-poly toon sprites and icons, with monumental silhouettes and warm dramatic light inspired by Arnold Friberg. These are original static raster assets; animation is a later pass. Click a piece to inspect its full-resolution transparent PNG.</p><div class="tools"><input id="search" placeholder="Find a unit, building, faction or icon…" aria-label="Search artwork"><select id="group" aria-label="Filter artwork"><option value="">All categories</option>OPTIONS</select></div><p id="count">COUNT</p></header>
<main>ENTRIES</main><footer>Original image generation: built-in image_gen. Source images and per-asset prompts remain editable in the project. The game imports compact 512px sprites and 256px icons.</footer>
<script>const s=document.querySelector('#search'),g=document.querySelector('#group'),cards=[...document.querySelectorAll('article')];function filter(){let n=0;for(const c of cards){c.hidden=!(c.dataset.find.includes(s.value.toLowerCase())&&(!g.value||c.dataset.group===g.value));if(!c.hidden)n++}document.querySelector('#count').textContent=n+' entries shown'}s.oninput=g.onchange=filter;</script></html>'''
page=page.replace("OPTIONS",options).replace("ENTRIES","".join(entries)).replace("COUNT",str(len(cards)-len(missing))+" of "+str(len(cards))+" images generated")
(ROOT/"Docs/ArtGallery.html").write_text(page,encoding="utf-8")
print(json.dumps({k:v for k,v in coverage.items() if k not in ("files","missing")}))
if missing and not args.allow_missing:raise SystemExit("Art production incomplete; see ArtCoverage-v0.6.json")

