#!/usr/bin/env python3
"""vm_split.py - split a Win10 VM serial.log (tools/vm_run.sh) into per-exe result files and validate them.

  vm_split.py SERIAL_LOG OUTDIR NAME [NAME ...]   [--append-summary] [--missing FILE]

serial.log carries frames written by tools/vm/vmrun.exe:
    @@I7 BEGIN <name> <cmdline>
    ...output...
    @@I7 END <name> exit=0x<code> ms=<n> timeout=<0|1> bytes=<n>
plus @@I7 CFG / @@I7 DONE markers from run.cmd and I7VM: lines from the boot hook.
Each NAME is written to OUTDIR/<name>.txt (CRs stripped) and classified:
    OK, FAIL (SUMMARY failed>0 or nonzero exit), TIMEOUT, SPAWNFAIL, TRUNCATED (BEGIN without END),
    NOSUMMARY (harness exe ended without a SUMMARY line: crash/abort), BADSUMMARY (SUMMARY disagrees with the
    RUN/OK/FAIL/SKIP lines), MISSING (never started).
Names starting with '__' or 'probe' are not harness exes: only exit/timeout/truncation are judged.
Exit status: 0 all OK, 1 test failures only, 3 any infrastructure problem.  --missing FILE lists MISSING names.
"""
import re, sys, os

def main():
    args = [a for a in sys.argv[1:]]
    miss_file = None
    if '--missing' in args:
        i = args.index('--missing'); miss_file = args[i + 1]; del args[i:i + 2]
    append = '--append-summary' in args
    args = [a for a in args if a != '--append-summary']
    if len(args) < 3:
        print(__doc__); return 2
    log, outdir, names = args[0], args[1], args[2:]
    os.makedirs(outdir, exist_ok=True)
    raw = open(log, 'rb').read().decode('utf-8', 'replace').replace('\r', '')
    frames, cur, buf, extra = {}, None, [], []
    for line in raw.split('\n'):
        m = re.match(r'@@I7 BEGIN (\S+)', line)
        if m:
            cur, buf = m.group(1), [line]; continue
        m = re.match(r'@@I7 END (\S+) exit=0x([0-9a-fA-F]+) ms=(\d+) timeout=(\d) bytes=(\d+)', line)
        if m and cur == m.group(1):
            buf.append(line)
            frames[cur] = dict(text=buf, exit=int(m.group(2), 16), ms=int(m.group(3)), timeout=int(m.group(4)), end=True)
            cur, buf = None, []; continue
        if cur: buf.append(line)
        elif line.strip(): extra.append(line)
    if cur:
        frames[cur] = dict(text=buf, exit=None, ms=0, timeout=0, end=False)
    done = any(l.startswith('@@I7 DONE') for l in extra)
    rows, worst, missing = [], 0, []
    for n in names:
        f = frames.get(n)
        if not f:
            rows.append((n, 'MISSING', '', '')); worst = 3; missing.append(n); continue
        body = list(f['text'])
        if '.' in n:   # a data file (e.g. probe CSV): drop the frame lines, keep the content only
            body = [l for l in body if not l.startswith('@@I7 BEGIN ') and not l.startswith('@@I7 END ')]
        while body and body[-1] == '': body.pop()
        with open(os.path.join(outdir, n + ('' if '.' in n else '.txt')), 'w') as fh:
            fh.write('\n'.join(body) + '\n')
        harness = not (n.startswith('__') or n.startswith('probe'))
        summ = [l for l in f['text'] if l.startswith('SUMMARY ')]
        info = ''
        if not f['end']:
            st = 'TRUNCATED'
        elif f['timeout']:
            st = 'TIMEOUT'
        elif f['exit'] == 0xDEAD0000:
            st = 'SPAWNFAIL'
        elif harness and not summ:
            st = 'NOSUMMARY'
        else:
            st = 'OK'
            if harness:
                m = re.match(r'SUMMARY tests=(\d+) failed=(\d+) skipped=(\d+) checks_pass=(\d+) checks_fail=(\d+)', summ[-1])
                if not m:
                    st = 'BADSUMMARY'
                else:
                    t, fl, sk = int(m.group(1)), int(m.group(2)), int(m.group(3))
                    runs = sum(1 for l in f['text'] if l.startswith('RUN  '))
                    rfail = sum(1 for l in f['text'] if l.startswith('FAIL ') and not l.startswith('FAIL exception'))
                    rskip = sum(1 for l in f['text'] if l.startswith('SKIP '))
                    rok = sum(1 for l in f['text'] if l.startswith('OK   '))
                    info = 'tests=%d failed=%d skipped=%d checks_fail=%s' % (t, fl, sk, m.group(5))
                    if runs != t or rfail != fl or rskip != sk or rok + rfail + rskip != t:
                        st = 'BADSUMMARY'; info += ' (lines: run=%d ok=%d fail=%d skip=%d)' % (runs, rok, rfail, rskip)
                    elif fl or f['exit'] != 0:
                        st = 'FAIL'
            elif f['exit'] != 0:
                st = 'FAIL'
        if st == 'FAIL': worst = max(worst, 1)
        elif st != 'OK': worst = 3
        rows.append((n, st, '0x%x' % f['exit'] if f['exit'] is not None else '-', ('%dms ' % f['ms']) + info))
    lines = ['%-28s %-10s %-11s %s' % r for r in rows]
    lines.append('boot: %s' % ('DONE marker seen' if done else 'NO DONE marker (boot cut short or run.cmd aborted)'))
    for l in extra:
        if l.startswith('@@I7 CFG') or l.startswith('I7VM:') or l.startswith('THEMESET'):
            lines.append('  ' + l)
    if not done: worst = 3
    with open(os.path.join(outdir, '_summary.txt'), 'a' if append else 'w') as fh:
        fh.write('== %s\n' % log); fh.write('\n'.join(lines) + '\n')
    with open(os.path.join(outdir, '_boot.txt'), 'a' if append else 'w') as fh:
        fh.write('== %s\n' % log); fh.write('\n'.join(extra) + '\n')
    print('\n'.join(lines))
    if miss_file:
        open(miss_file, 'w').write(''.join(n + '\n' for n in missing))
    return worst

if __name__ == '__main__':
    sys.exit(main())
