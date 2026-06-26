Import("env")
import subprocess, os

_script = os.path.join(env.subst("$PROJECT_DIR"), "flash_all.ps1")
_pio    = ["powershell", "-ExecutionPolicy", "Bypass", "-File", _script]

def _run(cmd):
    def action(source, target, env):
        subprocess.run(cmd, check=True)
    return action

env.AddCustomTarget(
    name="flash_all",
    dependencies=None,
    actions=_run(_pio),
    title="Flash All Devices",
    description="Firmware only -> COM16 + COM17",
)

env.AddCustomTarget(
    name="flash_all_fs",
    dependencies=None,
    actions=_run(_pio + ["-IncludeFS"]),
    title="Flash All Devices + FS",
    description="Firmware + filesystem -> COM16 + COM17",
)
