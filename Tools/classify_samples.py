"""Sorts analysed samples into groups: by keyword in the name when there is one, otherwise by the nearest named sounds.
Usage: python3 -I Tools/classify_samples.py <folder holding analysis.jsonl>; writes classified.json there."""
import json,re
K=[('loop',r'loop|play\.wav|_\d{2,3}_?bpm|\d{2,3} ?bpm'),
('conga',r'conga|tumba|quinto|\bcon\b|con \(|hand drum'),('bongo',r'bongo'),('djembe',r'djemb'),('talking',r'talking|tama\b|dundun|gangan'),
('shaker',r'shak|maraca|cabasa|\begg|caxixi|ganza'),('tamb',r'tamb|tambo|jingle|pandeir'),
('wood',r'clave|wood|block|blok|guiro|claves'),('metal',r'cowbell|bell|agogo|metal|gong|triang|\btri\b|tri \('),('rim',r'rim|stick|side'),
('tom',r'\btom|timbal|vinyltom|bombo'),('clap',r'clap|snap'),('snare',r'snare'),('hat',r'\bhh|hat|^h\d|\bcl\d|open\.wav|close\.wav'),('kick',r'kick|808'),
('fx',r'vox|sifflet|siflet|impact|fx|drop|transition|hit starting|naija|bouteille'),('perc',r'perc|percu')]
def keyword(path):
    n=path.lower()
    return next((k for k,p in K if re.search(p,n)),'?')
def load(f):
    R=[json.loads(l) for l in open(f)]
    for r in R: r['kw']=keyword(r['path'])
    return R

import sys,collections
import numpy as np
R=load(sys.argv[1]+'/analysis.jsonl')
# keyword group -> plugin category
CAT={'djembe':'Djembe','conga':'Congas','bongo':'Congas','talking':'Talking','tom':'Toms','kick':'Toms','rim':'Rims','wood':'Bois',
     'snare':'Claps','clap':'Claps','metal':'Metal','shaker':'Shakers','tamb':'Shakers','hat':'Shakers','fx':'FX'}
def vec(r):
    b=np.array(r['bands'])
    return np.concatenate([[np.log(r['centroid']+1),np.log(r['rolloff85']+1),np.log(r.get('length',.01)+.01),np.log(r.get('pitch',50)),
        np.log(r.get('peakiness',1)+1)*.5,np.log(r['flatness']+1e-6)*.3,np.log(r.get('attack',0)+.005)*.3,np.log(r.get('zcr',1)+1)],b*2])
# A file already in a tab folder (organize_samples.py, or moved there by hand) belongs to that tab.
FOLDER_GROUP={'Djemb\u00e9':'Djembe','M\u00e9tal':'Metal','Congas':'Congas','Talking':'Talking','Percs':'Percs','Toms':'Toms',
              'Rims':'Rims','Shakers':'Shakers','Claps':'Claps','FX':'FX'}
for r in R:
    folder=r['path'].split('/')[0] if '/' in r['path'] else ''
    if folder in FOLDER_GROUP: r['folder']=FOLDER_GROUP[folder]
    if folder in ('Doublons','Autres'): r['skip']=True
    if folder=='Loops': r['kw']='loop'
R=[r for r in R if not r.get('skip')]
seen=set(); U=[]
for r in R:  # exact duplicates: keep the first copy
    if r['md5'] in seen: r['dup']=True; continue
    seen.add(r['md5']); U.append(r)
for r in U:
    r['loop']= not r.get('folder') and (r['kw']=='loop' or (r['dur']>2.5 and r.get('onsets',0)>=8))
X=np.array([vec(r) for r in U]); mu,sd=X.mean(0),X.std(0)+1e-9; X=(X-mu)/sd
lab=[(r.get('folder') or CAT.get(r['kw'])) if not r['loop'] else None for r in U]
idx=[i for i,l in enumerate(lab) if l]
def knn(i,k=7):
    d=np.linalg.norm(X[idx]-X[i],axis=1)
    order=[idx[j] for j in np.argsort(d) if idx[j]!=i][:k]
    votes=collections.Counter(lab[j] for j in order)
    c,n=votes.most_common(1)[0]
    return c,n/k
for i,r in enumerate(U):
    if r['loop']: r['cat']='LOOP'; continue
    c,conf=knn(i)
    r['knn'],r['conf']=c,conf
    if lab[i]: r['cat']=lab[i]; r['how']='dossier' if r.get('folder') else 'nom'
    else: r['cat']=c if conf>=.57 else 'Percs'; r['how']='son'
json.dump(U,open(sys.argv[1]+'/classified.json','w'))
c=collections.Counter(r['cat'] for r in U); print(sorted(c.items(),key=lambda x:-x[1]))
dis=[r for r in U if r.get('how')=='nom' and r['knn']!=r['cat'] and r['conf']>=.85]
print('name vs sound strong disagreement',len(dis))
for r in dis[:40]: print('  ',r['cat'],'->',r['knn'],r['path'])
print('from sound:',collections.Counter((r['cat']) for r in U if r.get('how')=='son'))
print('dups',sum(1 for r in R if r.get('dup')))
