#!/usr/bin/env python3
# 列出所有 RPC：--dir server(NetServer) / client(NetClient) / both；可选正则过滤类名或函数名
import json, sys, os, re
DUMP = os.environ.get('DUMPROOT', 'dump_baseline_1.0.249/Dumpspace/')
def norm(lst):
    out={}
    for it in lst:
        if isinstance(it, dict):
            for k,v in it.items(): out[k]=v
    return out
mode = sys.argv[1] if len(sys.argv)>1 else 'both'
pat  = sys.argv[2] if len(sys.argv)>2 else ''
rx = re.compile(pat, re.I) if pat else None
j = norm(json.load(open(DUMP+'FunctionsInfo.json'))['data'])
rows=[]
for cls, body in j.items():
    if not isinstance(body, list): continue
    for it in body:
        if not isinstance(it, dict): continue
        for fn, fv in it.items():
            flag = fv[3] if len(fv)>3 else ''
            if 'Net' not in flag: continue
            is_srv = 'NetServer' in flag
            is_cli = 'NetClient' in flag or 'NetMulticast' in flag
            if mode=='server' and not is_srv: continue
            if mode=='client' and not is_cli: continue
            if rx and not (rx.search(cls) or rx.search(fn)): continue
            try:
                ret=fv[0][0]; args=','.join(str(p[0][0]) for p in (fv[1] or []))
            except Exception: ret,args='?',''
            d = 'S->C' if (is_cli and not is_srv) else ('C->S' if is_srv else '?')
            rows.append((d, cls, ret, fn, args))
rows.sort()
for d,cls,ret,fn,args in rows:
    print(f'{d}  {cls}::{ret} {fn}({args})')
print(f'### {mode} /{pat}/ : {len(rows)}')
