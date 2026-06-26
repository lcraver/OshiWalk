Import("env")
import subprocess, os

_script = os.path.join(env.subst("$PROJECT_DIR"), "github_release.ps1")
_pio    = ["powershell", "-ExecutionPolicy", "Bypass", "-File", _script]

def _run(cmd):
    def action(source, target, env):
        subprocess.run(cmd, check=True)
    return action

env.AddCustomTarget(
    name="github_release",
    dependencies=None,
    actions=_run(_pio),
    title="GitHub Release",
    description="Build, tag, and publish a GitHub release with firmware.bin",
)
