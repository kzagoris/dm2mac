import subprocess
from pathlib import Path

import pytest

pytest.register_assert_rewrite("dm2test")

pytest_plugins = [
    "dm2test.control",
    "dm2test.creature",
    "dm2test.dialog",
    "dm2test.gamefiles",
    "dm2test.gdat",
    "dm2test.input",
    "dm2test.interface",
    "dm2test.lib",
    "dm2test.modes",
    "dm2test.scenario",
]


def pytest_configure(config):
    from dm2test.markers import BUILD, ROOT

    # pytest does not build (tools/test-suite and make do); the main process only stops a run on a missing or old
    # build with one message, before the xdist workers start. The checks of tests/standard read no built file.
    if hasattr(config, "workerinput") or config.option.collectonly:
        return
    standard = (ROOT / "tests" / "standard").resolve()
    paths = [Path(arg.split("::")[0]).resolve() for arg in config.args]
    if all(path == standard or standard in path.parents for path in paths):
        return
    target = "all" if (BUILD / "dm2").exists() else "lib"
    if subprocess.run(["make", "-q", target], cwd=ROOT, capture_output=True).returncode:
        raise pytest.UsageError(f"build/ is missing or older than src/: run `make {target}` first (`make test` builds by itself)")


def pytest_handlecrashitem(crashitem, report, sched):
    # A test that kills its worker (a segmentation fault in the library, or the timeout) fails once. --dist loadfile
    # queues the rest of its file again with the crashed test still in it; left there, the test kills each new worker
    # and the tests after it never run.
    queue = getattr(sched, "workqueue", {})
    for scope, tests in list(queue.items()):
        if crashitem in tests:
            tests[crashitem] = True
            if all(tests.values()):
                del queue[scope]


def pytest_sessionfinish(session, exitstatus):
    from dm2test.markers import ROOT

    # records the git tree of a green full run of a clean checkout; tools/test-suite skips a rerun of that tree
    if hasattr(session.config, "workerinput") or exitstatus != 0 or session.config.args != ["tests"]:
        return
    git = lambda *a: subprocess.run(["git", *a], cwd=ROOT, capture_output=True, text=True).stdout.strip()
    if git("status", "--porcelain", "--untracked-files=no"):
        return
    with open(Path(git("rev-parse", "--path-format=absolute", "--git-common-dir")) / "green-trees", "a") as f:
        f.write(git("rev-parse", "HEAD^{tree}") + "\n")
