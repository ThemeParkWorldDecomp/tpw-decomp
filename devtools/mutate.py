"""mutate.py TREE BUILD TEST MUTANTS.py: the mutation check. MUTANTS.py defines
`mutants = [(name, relative file, old text, new text), ...]`. Each mutant is applied, the tree
built, TEST run, and the failing case count printed; the original is kept on disk while a
mutant is applied and restored at the next start if the run was killed.

A round is VALID only when the unmutated module first runs with a positive case count and no
failure; otherwise the script refuses to apply any mutant (exit 2). A mutant whose run did not
load, crashed, timed out or failed to build is INVALID, never a kill: it is listed and counted
apart. (Until 2026-10-12 anything that was not "0 failing" counted as caught, so rounds whose
shim never loaded printed perfect scores: s6, w20, w41b.)"""
import pathlib, subprocess, sys, os, re, runpy
W, B, TEST, MUT = pathlib.Path(sys.argv[1]), sys.argv[2], sys.argv[3], sys.argv[4]
mutants = runpy.run_path(MUT)["mutants"]
BACKUP = pathlib.Path(MUT).with_suffix(".backup")
BACKUP.mkdir(exist_ok=True)
for saved in BACKUP.iterdir():
    target = W / saved.name.replace("__", "/")
    target.write_text(saved.read_text()); saved.unlink()
    print("restored from backup:", target)
env = dict(os.environ, TPW_SHIM=f"{B}/libtpw_testshim.dylib", PYTHONDONTWRITEBYTECODE="1")
INVALID = ("test run exit", "test run killed", "BUILD FAILED")
def run():
    """Returns (status, cases): status is "N failing cases" for a run that produced case
    counts, else one of the INVALID prefixes with the reason; cases is the number of cases
    the run reported (0 when it reported none)."""
    b = subprocess.run(["ninja", "-C", B], capture_output=True, text=True)
    if b.returncode != 0: return "BUILD FAILED: " + b.stdout[-300:].replace("\n", " "), 0
    # The test run is watched: a mutant can make translated code loop and allocate without end
    # (one filled the disk with swap). Killed over 3 GB of memory or after 240 seconds.
    import tempfile, time
    with tempfile.TemporaryFile("w+") as out, tempfile.TemporaryFile("w+") as err:
        child = subprocess.Popen(["../../venv/bin/python", "-I", "-u", "difftest.py", TEST], cwd=W / "tests", env=env, stdout=out, stderr=err, text=True)
        started = time.time()
        killed = None
        while child.poll() is None:
            time.sleep(0.25)
            rss = subprocess.run(["ps", "-o", "rss=", "-p", str(child.pid)], capture_output=True, text=True).stdout.strip()
            if rss.isdigit() and int(rss) > 3 * 1024 * 1024:
                killed = "memory over 3 GB (a loop that allocates)"
            elif time.time() - started > 240:
                killed = "timed out (an endless loop)"
            if killed:
                child.kill(); child.wait()
                return "test run killed: " + killed, 0
        out.seek(0); err.seek(0)
        class T: pass
        t = T(); t.stdout, t.stderr, t.returncode = out.read(), err.read(), child.returncode
    bad = total = 0
    for line in t.stdout.splitlines():
        m = re.search(r"(\d+)/(\d+) cases match", line)
        if m: bad += int(m.group(2)) - int(m.group(1)); total += int(m.group(2))
    if bad or t.returncode == 0:
        return f"{bad} failing cases", total
    if t.returncode < 0:
        # The process died from a signal (a segmentation fault, an abort) while the unmutated
        # module had passed: the mutant was detected, by a crash. Only a run that never loaded
        # the shim, failed to build or was killed by the watchdog is invalid (2026-10-12, tool-t3).
        return f"crashed (signal {-t.returncode}): {t.stderr[-200:]!r}", total
    return f"test run exit {t.returncode}: {t.stdout[-200:]!r} {t.stderr[-300:]!r}", total
status, cases = run()
if status != "0 failing cases" or cases == 0:
    print(f"BASELINE INVALID, no mutant applied: {status}; {cases} cases reported. Check TREE, BUILD, the shim path and the module name.")
    sys.exit(2)
print(f"baseline: {cases} cases, 0 failing", flush=True)
caught = invalid = survivors = 0
for name, rel, old, new in mutants:
    path = W / rel; original = path.read_text()
    if original.count(old) != 1:
        print(f"{name}: PATTERN COUNT {original.count(old)}"); continue
    saved = BACKUP / rel.replace("/", "__")
    saved.write_text(original)
    try:
        path.write_text(original.replace(old, new))
        result, n = run()
        if result.startswith(INVALID):
            invalid += 1; tag = "INVALID"
        elif result.startswith("0 failing"):
            survivors += 1; tag = "SURVIVOR"
        else:
            caught += 1; tag = "caught (crash)" if result.startswith("crashed") else "caught"
        print(f"{name}: {tag}: {result}", flush=True)
    finally:
        path.write_text(original); saved.unlink()
status, cases = run()
print(f"caught {caught} of {caught + survivors} valid, {survivors} survivors, {invalid} invalid; restored: {status} ({cases} cases)")
