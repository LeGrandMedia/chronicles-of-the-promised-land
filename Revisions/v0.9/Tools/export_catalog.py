#!/usr/bin/env python3
"""Export the provisional Book of Mormon RTS vault catalog to runtime JSON.

The vault notes remain authoritative. This script reads their base-stat, availability,
construction, faction-roster, and upgrade tables and emits a normalized catalog.
"""
from __future__ import annotations
import json, re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VAULT = ROOT / 'book of mormon RTS'
OUT = ROOT / 'Game' / 'Content' / 'Data' / 'catalog.json'

FACTION_FILES = {
    'Nephites': ('nephites', [63, 119, 214]),
    'Lamanites': ('lamanites', [181, 91, 54]),
    'Gadianton Robbers': ('gadianton', [104, 83, 132]),
    'Coriantumr Jaredites': ('coriantumr', [154, 112, 68]),
    'Shiz Jaredites': ('shiz', [174, 89, 67]),
    'House of Laban': ('laban', [110, 139, 156]),
}
ALIASES = {'Gadianton': 'Gadianton Robbers'}

def read(path): return path.read_text(encoding='utf-8-sig')
def note_paths(folder): return sorted((VAULT / folder).glob('*.md'))
def clean_cell(s): return re.sub(r'\s+', ' ', s.strip().strip('|').strip())
def section(text, heading):
    lines=text.splitlines()
    marker='## '+heading
    for i,line in enumerate(lines):
        if line.strip()==marker:
            body=[]
            for candidate in lines[i+1:]:
                if candidate.startswith('## '): break
                body.append(candidate)
            return '\n'.join(body).strip()
    return ''
def table_rows(text, heading=None):
    if heading:
        m = re.search(r'^## '+re.escape(heading)+r'\s*$', text, re.M)
        if not m: return []
        text = text[m.end():]
        text = re.split(r'^## ', text, maxsplit=1, flags=re.M)[0]
    rows=[]
    for line in text.splitlines():
        if not line.strip().startswith('|'): continue
        cells=[clean_cell(x) for x in line.strip().strip('|').split('|')]
        if cells and all(re.fullmatch(r':?-{2,}:?', c) for c in cells): continue
        rows.append(cells)
    return rows

def value_for(rows, key):
    for row in rows:
        if len(row)>1 and row[0].lower().rstrip(':') == key.lower(): return row[1]
    return ''
def links(text):
    return re.findall(r'\[\[([^]|]+)(?:\|([^]]+))?\]\]', text)
def slug(s):
    s=s.lower().replace("'", '')
    return re.sub(r'[^a-z0-9]+','_',s).strip('_')
def num(s, default=0):
    m=re.search(r'-?\d+(?:\.\d+)?',s.replace(',',''))
    return float(m.group()) if m else default
def resource_cost(s):
    vals=[]
    for label in ('Food','Timber','Wealth'):
        m=re.search(r'(\d+)\s*'+label,s,re.I)
        vals.append(int(m.group(1)) if m else 0)
    return vals
def unscale_rounded_up(value, factor):
    # Invert the rounded-up faction value, preferring the nearest plausible base.
    candidates=[base for base in range(max(0,int(value/factor)-4),int(value/factor)+5)
                if __import__('math').ceil(base*factor)==value]
    return min(candidates,key=lambda base:abs(base-value/factor)) if candidates else round(value/factor)

def parse_unit(path):
    t=read(path); name=path.stem; sections={}
    for heading in ('Availability and acquisition','Base statistics','Behavior and abilities','Implementation prompt'):
        value=section(t,heading)
        if value: sections[heading]=value
    acq=sections.get('Availability and acquisition','')
    # Acquisition table's first data line is the non-Nephite canonical price/time baseline.
    ar=table_rows(acq)
    table=ar[1:] if ar else []
    base_cost=[0,0,0]; seconds=0; producer=''; age=1; req=[]
    nephite_cost=None; nephite_seconds=0; lamanite_cost=None; lamanite_seconds=0
    for row in table:
        if len(row)<3: continue
        fac=row[0].replace('[[','').replace(']]','')
        cost_match=re.search(r'\d+\s*Food\s*/\s*\d+\s*Timber\s*/\s*\d+\s*Wealth',row[1],re.I)
        if fac not in ('Nephites','Lamanites') and cost_match:
            base_cost=resource_cost(row[1]); seconds=int(num(row[2])); break
        if fac == 'Lamanites' and cost_match:
            lamanite_cost=resource_cost(row[1]); lamanite_seconds=int(num(row[2]))
        if fac == 'Nephites' and cost_match:
            nephite_cost=resource_cost(row[1]); nephite_seconds=int(num(row[2]))
    else:
        if lamanite_cost is not None:
            base_cost=[unscale_rounded_up(v,0.65) for v in lamanite_cost]
            seconds=unscale_rounded_up(lamanite_seconds,0.60)
        elif nephite_cost is not None:
            base_cost=[unscale_rounded_up(v,1.25) for v in nephite_cost]
            seconds=unscale_rounded_up(nephite_seconds,1.20)
    if name=='Ammonite Covenant Keeper':
        # Conversion channel is a special acquisition time, not a faction training modifier.
        seconds=12
    reqline=re.search(r'Requirements:\s*(.+?)(?:\n|$)',acq)
    requirements=reqline.group(1) if reqline else ''
    age_match=re.search(r'Age\s*(\d+)',requirements,re.I)
    if age_match: age=int(age_match.group(1))
    known_building_slugs={slug(p.stem) for p in note_paths('04 Structures')}
    for target,_ in links(requirements):
        if slug(target) in known_building_slugs: producer=slug(target)
    if 'conversion only' in acq.lower(): producer=''
    # Producer/route may be presented inside table for instant assignment/conversion.
    if not producer and len(table):
        for row in table:
            if len(row)>3 and row[0].startswith('[['):
                continue
            # Ignore faction rows; producer is usually specified in the Requirements line.
    stats={r[0].lower():r[1] for r in table_rows(sections.get('Base statistics','')) if len(r)>1}
    behavior=sections.get('Behavior and abilities','').strip()
    behavior=behavior.strip()
    prompt=sections.get('Implementation prompt','')
    if not producer:
        prod=re.search(r'(?:Train from|evolves from|assigned from|Conversion only|Convert from)\s*\[\[([^]]+)',prompt+acq,re.I)
        if prod and slug(prod.group(1)) in known_building_slugs: producer=slug(prod.group(1))
        elif 'instant assignment' in acq.lower(): producer=''
    kind='infantry'
    low=(name+' '+behavior+' '+prompt).lower()
    if name=='Industrious Laborer': kind='worker'
    if name in ('Tiller of the Earth','Keeper of Flocks','Worker in Ore'): kind='job'
    if name in ('Captain Moroni','Ammon','Helaman','Teancum','Coriantumr','Shiz','Laban','Gadianton','Giddianhi','Amalickiah'): kind='hero'
    if 'hero entity' in low: kind='hero'
    if name in ('Bowman','Slinger of Stones','Wilderness Spy'): kind='ranged'
    if name in ('Teacher of the Word','Rameumptom Priest','Corrupt Judge'): kind='priest'
    if name in ('Cumelom','Curelom','Elephant'): kind='beast'
    if name in ("Hagoth's Vessel",'Jaredite Barge'): kind='ship'
    if name=='Trader in Precious Things': kind='trader'
    if name=='Ammonite Covenant Keeper': kind='ammonite'
    if name=="King's Chariot": kind='chariot'
    if kind=='job': producer=''
    ability=behavior
    # Source is the compact basis/adaptation paragraph in each unit note.
    source=''
    source=section(t,'Presentation and scriptural basis')
    if source: source=re.sub(r'\s+',' ',source).strip()
    return {'id':slug(name),'name':name,'hp':num(stats.get('hp','')),'attack':num(stats.get('attack damage','')),
      'interval':num(stats.get('attack interval (s)',''),1.5),'range':num(stats.get('attack range (m)','')),
      'speed':num(stats.get('speed (m/s)','')),'sight':num(stats.get('sight (m)','')),'armor':num(stats.get('flat armor','')),
      'pop':int(num(stats.get('population',''),1)),'age':age,'producer':producer,'requires':req,'cost':base_cost,'seconds':seconds,
      'kind':kind,'description':re.sub(r'\s+',' ',behavior),'ability':re.sub(r'\s+',' ',behavior),'source':source}

units=[parse_unit(p) for p in note_paths('05 Units')]
unit_names={u['name']:u['id'] for u in units}

# Faction availability lists are the source of truth for exact allowed rosters.
factions=[]
for name,(fid,color) in FACTION_FILES.items():
    p=VAULT/'03 Civilizations'/f'{name}.md'
    if name=='Gadianton Robbers': p=VAULT/'03 Civilizations'/'Gadianton Robbers.md'
    if name=='Coriantumr Jaredites': p=VAULT/'03 Civilizations'/'Coriantumr Jaredites.md'
    if name=='Shiz Jaredites': p=VAULT/'03 Civilizations'/'Shiz Jaredites.md'
    if name=='House of Laban': p=VAULT/'03 Civilizations'/'House of Laban.md'
    t=read(p)
    roster=[]
    for row in table_rows(t,'Exact available roster'):
        if row and row[0].lower()!='unit':
            for target,_ in links(row[0]):
                if target in unit_names: roster.append(unit_names[target])
    buildings=[]
    structures_text=section(t,'Structures')
    if structures_text:
        for target,_ in links(structures_text):
            if target in building_names if 'building_names' in globals() else False: buildings.append(slug(target))
    upgrade_names=[]
    upgrades_text=section(t,'Available upgrades')
    if upgrades_text:
        raw=upgrades_text.split('Full prices',1)[0]
        upgrade_names=[x.strip() for x in raw.replace('[[','').replace(']]','').replace('\n',' ').split(',') if x.strip()]
    factions.append({'id':fid,'name':name,'units':roster,'buildings':buildings,'upgrades':[slug(x) for x in upgrade_names],
      'description':re.sub(r'\s+',' ',section(t,'Civilization prompt')).strip(), 'color':color})

buildings=[]
BUILDING_RADII={
    'seat_of_the_people':3.0,'fields_of_grain':2.4,'gathering_of_flocks':2.4,
    'tents_and_habitations':1.6,'storehouse_of_provisions':1.8,'encampment_of_war':2.5,
    'house_of_curious_workmanship':2.4,'place_of_trade':2.2,'temple_of_the_land':2.8,
    'judgment_seat':2.4,'tower_of_watchmen':1.2,'works_of_defense':2.0,
    'rameumptom':1.8,'place_of_many_waters':2.5,'hill_of_ore':1.8,
}
for p in note_paths('04 Structures'):
    t=read(p); name=p.stem
    rows=table_rows(t,'Base construction data')
    cost=resource_cost(value_for(rows,'Cost'))
    prereq=value_for(rows,'Prerequisites')
    age=int(num(re.search(r'Age\s*(\d+)',prereq,re.I).group(1))) if re.search(r'Age\s*(\d+)',prereq,re.I) else 1
    requires=[slug(x) for x,_ in links(prereq)]
    # avoid converting civilization mentions; prerequisites generally contain only building refs
    desc=[]
    for h in ('Implementation prompt','Production and research','Placement, variants and interactions'):
        body=section(t,h)
        if body: desc.append(re.sub(r'\s+',' ',body).strip())
    bid=slug(name)
    intro=section(t,'Implementation prompt')
    attack_match=re.search(r'attacks? for\s+(\d+(?:\.\d+)?)\s+ranged damage',intro,re.I)
    interval_match=re.search(r'every\s+(\d+(?:\.\d+)?)s',intro,re.I)
    range_match=re.search(r'at\s+(\d+(?:\.\d+)?)m range',intro,re.I)
    pop_match=re.search(r'(?:Adds\s+(\d+)\s+population|\+(\d+)\s+population)',intro,re.I)
    detect_match=re.search(r'stealth detection\s+(\d+)m',intro,re.I)
    pop=int(pop_match.group(1) or pop_match.group(2)) if pop_match else 0
    buildings.append({'id':bid,'name':name,'hp':int(num(value_for(rows,'Base max HP'))),'armor':int(num(value_for(rows,'Armor'))),
      'sight':num(value_for(rows,'Vision')),'cost':cost,'seconds':int(num(value_for(rows,'Construction work / nominal seconds'))),'age':age,
      'requires':requires,'pop':pop,'radius':BUILDING_RADII[bid],
      'attack':num(attack_match.group(1)) if attack_match else 0,
      'interval':num(interval_match.group(1)) if interval_match else 0,
      'range':num(range_match.group(1)) if range_match else 0,
      'stealthDetection':num(detect_match.group(1)) if detect_match else 0,
      'description':' '.join(desc),'source':f'04 Structures/{p.name}'})
building_names={b['name']:b['id'] for b in buildings}
for faction in factions:
    p=VAULT/'03 Civilizations'/f'{faction["name"]}.md'; t=read(p)
    structures_text=section(t,'Structures')
    faction['buildings']=[building_names[x] for x,_ in links(structures_text) if x in building_names]

# Normalize inferred unit producers and producer acquisition routes against canonical building ids.
for u in units:
    if u['producer'] in building_names: u['producer']=building_names[u['producer']]
    # Units in roster tables may state special routes rather than a direct producer.
    if not u['producer']:
        for f in factions:
            if u['id'] in f['units']:
                # search roster table producer column for a linked building
                p=VAULT/'03 Civilizations'/(f['name']+'.md'); t=read(p)
                for row in table_rows(t,'Exact available roster'):
                    if row and any(x==u['name'] for x,_ in links(row[0])) and len(row)>3:
                        for x,_ in links(row[3]):
                            if x in building_names: u['producer']=building_names[x]
                        if not u['producer'] and any(x in row[3].lower() for x in ('assignment','conversion')): u['producer']=''
    # Canonical producer refs in requirements, if present.
    if u['producer'] and u['producer'] not in building_names.values(): u['producer']=slug(u['producer'])

# Upgrade rows from the catalog. Their dependencies combine explicit effects/notes and age gates.
upgrades=[]
t=read(VAULT/'02 Systems'/'Upgrade Catalog.md')
rows=table_rows(t)
for row in rows[1:]:
    if len(row)<7: continue
    name,age,producer,cost,seconds,fac_text,effect=row[:7]
    name=name.replace('**','').strip()
    facs=[FACTION_FILES.get(x.strip(),(slug(x.strip()),[]))[0] for x in fac_text.split(',')]
    req=[]
    if 'requires ' in effect.lower():
        m=re.search(r'requires\s+([^.;]+)',effect,re.I)
        if m:
            for x in re.findall(r'[A-Z][A-Za-z ]+',m.group(1)):
                candidate=slug(x.strip())
                if candidate: req.append(candidate)
    if name=='Works of Timbers': req=['heaps_of_earth']
    if name=='Pickets': req=['works_of_timbers']
    if name=='Elephant Husbandry': req=['curelom_husbandry']
    upgrades.append({'id':slug(name),'name':name,'cost':resource_cost(cost),'seconds':int(num(seconds)),'age':int(num(age)),
      'producer':slug(re.sub(r'\[\[|\]\]','',producer)),'requires':req,'description':effect.strip(),'factions':facs})
# Add building and upgrade references in faction records based on exact availability.
upgrade_ids={u['name']:u['id'] for u in upgrades}
for faction in factions:
    faction['upgrades']=[uid for uid in faction['upgrades'] if uid in upgrade_ids.values()]

# Research dependencies named in acquisition requirements are explicit unlock gates.
for u in units:
    note=read(VAULT/'05 Units'/f"{u['name']}.md")
    reqline=re.search(r'Requirements:\s*(.+?)(?:\n|$)',note)
    if reqline:
        text=reqline.group(1)
        u['requires']=[uid for uname,uid in upgrade_ids.items() if uname.lower() in text.lower()]

catalog={'units':units,'buildings':buildings,'factions':factions,'upgrades':upgrades}
def validate(data):
    assert [len(data[k]) for k in ('units','buildings','factions','upgrades')]==[39,15,6,19]
    for key in ('units','buildings','factions','upgrades'):
        ids=[item['id'] for item in data[key]]
        assert len(ids)==len(set(ids)), f'duplicate {key} id'
    unit_ids={x['id'] for x in data['units']}; building_ids={x['id'] for x in data['buildings']}
    upgrade_ids={x['id'] for x in data['upgrades']}; faction_ids={x['id'] for x in data['factions']}
    allowed_kinds={'worker','job','infantry','ranged','priest','hero','beast','ship','trader','ammonite','chariot'}
    for u in data['units']:
        assert u['kind'] in allowed_kinds, f"unknown unit kind: {u['name']}"
        assert not u['producer'] or u['producer'] in building_ids, f"unknown unit producer: {u['name']}"
        assert set(u['requires']) <= upgrade_ids | building_ids, f"unknown unit requirement: {u['name']}"
    for b in data['buildings']:
        assert set(b['requires']) <= building_ids, f"unknown building prerequisite: {b['name']}"
        assert b['radius'] > 0 and b['pop'] >= 0
    for u in data['upgrades']:
        assert u['producer'] in building_ids, f"unknown upgrade producer: {u['name']}"
        assert set(u['requires']) <= upgrade_ids | building_ids, f"unknown upgrade prerequisite: {u['name']}"
        assert set(u['factions']) <= faction_ids, f"unknown upgrade faction: {u['name']}"
    for f in data['factions']:
        assert set(f['units']) <= unit_ids and set(f['buildings']) <= building_ids and set(f['upgrades']) <= upgrade_ids
    assert [x['id'] for x in data['factions']]==['nephites','lamanites','gadianton','coriantumr','shiz','laban']
validate(catalog)
OUT.parent.mkdir(parents=True,exist_ok=True)
OUT.write_text(json.dumps(catalog,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
print(f'Wrote {OUT.relative_to(ROOT)}: {len(units)} units, {len(buildings)} buildings, {len(factions)} factions, {len(upgrades)} upgrades')
