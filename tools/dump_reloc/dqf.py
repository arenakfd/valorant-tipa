#!/usr/bin/env python3
# dump 函数名查询器：按函数名正则搜，列出所属类 + 签名
import json, sys, os, re
DUMP = os.environ.get('DUMPROOT', 'dump_baseline_1.0.249/Dumpspace/')
def norm(lst):
    out={}
    for it in lst:
        if isinstance(it, dict):
            for k,v in it.items(): out[k]=v
    return out
def main():
    pat = sys.argv[1]
    rx = re.compile(pat, re.I)
    j = norm(json.load(open(DUMP+'FunctionsInfo.json'))['data'])
    n=0
    for cls, body in j.items():
        if not isinstance(body, list): continue
        for it in body:
            if not isinstance(it, dict): continue
            for fn, fv in it.items():
                if not rx.search(fn): continue
                try:
                    ret = fv[0][0]
                    args = []
                    for p in (fv[1] or []):
                        try: args.append(f'{p[0][0]} {p[1]} {p[2]}')
                        except Exception: args.append(str(p)[:40])
                    flag = fv[3] if len(fv)>3 else ''
                except Exception:
                    ret, args, flag = '?', [str(fv)[:60]], ''
                print(f'{cls}::{ret} {fn}({", ".join(a.strip() for a in args)})  [{flag}]')
                n+=1
    print(f'### /{pat}/ : {n} funcs')
main()
