#!/usr/bin/env python3
# dump 查询器：按关键词搜类/结构/函数/枚举，输出完整定义（单行大 JSON 用这个，别用 grep）
import json, sys, os, re

DUMP = os.environ.get('DUMPROOT', 'dump_baseline_1.0.249/Dumpspace/')

def load(name):
    with open(DUMP + name) as f:
        return json.load(f)['data']

def norm(lst):
    """把 [{name: [...]}, ...] 拍平成一个 dict name->body"""
    out = {}
    for it in lst:
        if not isinstance(it, dict): continue
        for k, v in it.items():
            out[k] = v
    return out

def fmt_fields(body):
    """body = [__InheritInfo..., __MDKClassSize..., {field: [[type,kind,?,...],offset,size,?]}]"""
    inh, size, fields = [], None, []
    for it in body:
        if not isinstance(it, dict): continue
        if '__InheritInfo' in it:
            inh = it['__InheritInfo']
        elif '__MDKClassSize' in it:
            size = it['__MDKClassSize']
        else:
            for fn, fv in it.items():
                try:
                    t = fv[0][0]; off = fv[1]
                    fields.append((off, fn, t))
                except Exception:
                    fields.append((None, fn, str(fv)[:80]))
    fields.sort(key=lambda x: (x[0] is None, x[0]))
    return inh, size, fields

def fmt_funcs(body):
    out = []
    for it in body:
        if not isinstance(it, dict): continue
        for fn, fv in it.items():
            try:
                ret = fv[0][0]
                params = ','.join(p[0] for p in (fv[1] or []))
                flag = fv[3] if len(fv) > 3 else ''
                out.append((fn, ret, params, flag))
            except Exception:
                out.append((fn, '?', str(fv)[:60], ''))
    return out

def main():
    if len(sys.argv) < 3:
        print(__doc__ or 'usage: dq.py <kind> <pattern>'); return
    kind, pat = sys.argv[1], sys.argv[2]
    rx = re.compile(pat, re.I)
    files = {'class': 'ClassesInfo.json', 'struct': 'StructsInfo.json',
             'func': 'FunctionsInfo.json', 'enum': 'EnumsInfo.json'}
    j = norm(load(files[kind]))
    hits = [k for k in j if rx.search(k)]
    print(f'### {kind} ~ /{pat}/ : {len(hits)} hits')
    for h in hits:
        body = j[h]
        print('=' * 70)
        print('##', h)
        if kind in ('class', 'struct'):
            inh, size, fields = fmt_fields(body)
            if inh: print('   inherit:', ' <- '.join(inh))
            if size is not None: print('   size:', size, f'(0x{size:X})')
            for off, fn, t in fields:
                if off is None:
                    print(f'   ?  {fn} : {t}')
                else:
                    print(f'   +0x{off:X}  {fn} : {t}')
        elif kind == 'func':
            for fn, ret, ps, flag in fmt_funcs(body):
                print(f'   {ret} {fn}({ps})   [{flag}]')
        else:
            for it in (body or []):
                print('  ', it)

if __name__ == '__main__':
    main()
