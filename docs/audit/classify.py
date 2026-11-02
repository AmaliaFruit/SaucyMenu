import csv,re,os,sys
root=sys.argv[1]; inp=sys.argv[2]; out=sys.argv[3]
files={}
def body_at(loc):
    if not loc: return ''
    f,l=loc.rsplit(':',1); l=int(l)
    if f not in files: files[f]=open(os.path.join(root,f),encoding='utf-8-sig',errors='replace').read().split('\n')
    lines=files[f]; depth=0; started=False; out=[]
    for i in range(l-1,min(len(lines),l+400)):
        s=lines[i]; out.append(s)
        if '=>' in s and not started and '{' not in s:
            # expression-bodied
            j=i
            while not lines[j].rstrip().endswith(';') and j<i+20: j+=1; out.append(lines[j])
            return '\n'.join(out)
        depth+=s.count('{')-s.count('}')
        if '{' in s: started=True
        if started and depth<=0: break
    return '\n'.join(out)
NET=re.compile(r'RaiseEvent|SendRPC|\.RPC\(|PhotonNetwork\.(Instantiate|Destroy|Set|Close|Kick)|NetworkSystem\.Instance\.(Return|Kick|Send)|GorillaGameManager|ReportPlayer|OpRaiseEvent|SetMasterClient|Serialize|LaunchProjectile|PhotonView|photonView|Sound\.|PlayHandTap|SendEvent|RequestOwnership|TransferOwnership')
SELF=re.compile(r'GTPlayer|GorillaTagger\.Instance\.(rigidbody|bodyCollider)|playerRigidBody|transform\.position \+=|linearVelocity|velocity')
HARD_EXCL={'Master Mods','Overpowered Mods','Projectile Spam Mods','Sound Spam Mods','Admin Mods (admins only)','Overpowered Settings','Internal Mods (hidden from user)','Safety Mods','Safety Settings'}
ADV={'Advantage Mods','Advantage Settings'}
GRIEF_WORDS=re.compile(r'\b(gun|all|spam|lag|crash|kick|fling|steal|others?|player|aura|tag|ban|freeze|break|destroy|kill|mute|serverside|ss|everyone|grab|rope|mat)\b',re.I)
ADV_WORDS=re.compile(r'(esp|tracer|chams|box|nametag|beacon|hitbox|infection|tagged|lurker|ghost)',re.I)
rows=list(csv.DictReader(open(inp)))
for r in rows:
    b=body_at(r['defined_at'])
    net=bool(NET.search(b)); selfmove=bool(SELF.search(b))
    c=r['category']; n=r['name']
    if r['detected']=='True' or c in HARD_EXCL: v='EXCLUDE: disrupts others / bypasses safety'
    elif c in ADV: v='EXCLUDE: unfair advantage'
    elif c=='Credits' or 'Discord' in n or 'Patreon' in n or 'Donate' in n: v='DROP: upstream branding/promo/monetisation'
    elif c in ('Friends','Friend Settings','Players') or 'Friend' in n: v='DROP: depends on Seralyth server'
    elif net or GRIEF_WORDS.search(n): v='REVIEW-HIGH: network-affecting or targets others'
    elif c=='Visual Mods' and ADV_WORDS.search(n): v='EXCLUDE-PUBLIC: information advantage'
    elif c in ('Movement Mods','Movement Settings') or selfmove: v='PRIVATE-ONLY: local movement'
    elif 'Settings' in c or c in ('Main','Favorite Mods','Enabled Mods','Menu Presets','Keybind Settings','Rebind Settings','Visual Settings'): v='KEEP-PATTERN: menu infrastructure'
    else: v='REVIEW: local, needs case-by-case check'
    r['network_calls_detected']=net; r['moves_local_player']=selfmove; r['verdict']=v
with open(out,'w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)
from collections import Counter
for k,v in Counter(r['verdict'] for r in rows).most_common(): print(v,k)
