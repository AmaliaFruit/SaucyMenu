import re,os,csv,sys,json
root=sys.argv[1]; out=sys.argv[2]
src=open(os.path.join(root,'Menu/Buttons.cs'),encoding='utf-8-sig').read()
start=src.index('public static ButtonInfo[][] buttons'); end=src.index('public static string[] categoryNames')
body=src[start:end]; base_line=src[:start].count('\n')+1
# index method definitions across Mods/ Managers/ Menu/
defs={}
for d in ['Mods','Managers','Menu','Classes','Patches','Utilities']:
    for dp,_,fs in os.walk(os.path.join(root,d)):
        for f in fs:
            if not f.endswith('.cs'): continue
            p=os.path.join(dp,f); rel=os.path.relpath(p,root)
            txt=open(p,encoding='utf-8-sig',errors='replace').read()
            cls=None
            for i,l in enumerate(txt.split('\n'),1):
                m=re.search(r'\b(?:class|struct)\s+(\w+)',l)
                if m and not l.strip().startswith('//'): cls=m.group(1)
                m=re.match(r'\s*(?:public|private|internal|protected)\s+(?:static\s+)?(?:async\s+)?[\w<>\[\],\. ]+\s+(\w+)\s*\(',l)
                if m:
                    stem=os.path.splitext(f)[0]
                    defs.setdefault(f"{stem}.{m.group(1)}",f"{rel}:{i}")
                    if cls: defs.setdefault(f"{cls}.{m.group(1)}",f"{rel}:{i}")
cats={}
for m in re.finditer(r'new(?: ButtonInfo)?\[\] \{ ?//\s*([^\n\[]+)\[(\d+)\]',body):
    cats[m.start()]=(int(m.group(2)),m.group(1).strip())
cat_pos=sorted(cats)
rows=[]
for m in re.finditer(r'new ButtonInfo \{(.*)',body):
    line=m.group(1); pos=m.start()
    c=None
    for cp in cat_pos:
        if cp<pos: c=cats[cp]
    def g(k):
        mm=re.search(k+r'\s*=\s*"((?:[^"\\]|\\.)*)"',line); return mm.group(1) if mm else ''
    meth=re.search(r'\bmethod\s*=\s*(?:\(\)\s*=>\s*)?([A-Za-z_][\w\.]*)',line)
    en=re.search(r'\benableMethod\s*=\s*(?:\(\)\s*=>\s*)?([A-Za-z_][\w\.]*)',line)
    target=(meth.group(1) if meth else '')
    loc=''
    for t in [target,(en.group(1) if en else '')]:
        if t and '.' in t:
            k='.'.join(t.split('.')[-2:])
            if k in defs: loc=defs[k]; break
    rows.append(dict(category_index=c[0] if c else '',category=c[1] if c else '',name=re.sub(r'<[^>]+>','',g('buttonText')),
        method=target, enable_method=en.group(1) if en else '', defined_at=loc,
        registered_at=f"Menu/Buttons.cs:{base_line+body[:pos].count(chr(10))}",
        legal='legal = true' in line, detected='detected = true' in line, togglable='isTogglable = false' not in line,
        tooltip=re.sub(r'<[^>]+>','',g('toolTip'))))
with open(out,'w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)
print(len(rows), sum(1 for r in rows if r['defined_at']))
