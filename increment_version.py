Import("env")
import os, re, glob

project_dir = env.subst("$PROJECT_DIR")
src_dir     = os.path.join(project_dir, "src")
ver_file    = os.path.join(src_dir, "version.h")

# Only increment when at least one source file changed since the last bump
ver_mtime = os.path.getmtime(ver_file) if os.path.exists(ver_file) else 0
sources   = (glob.glob(os.path.join(src_dir, "**", "*.cpp"), recursive=True) +
             glob.glob(os.path.join(src_dir, "**", "*.h"),   recursive=True))
changed   = not os.path.exists(ver_file) or any(
    os.path.getmtime(s) > ver_mtime for s in sources if s != ver_file
)

build = 0
if os.path.exists(ver_file):
    with open(ver_file) as f:
        m = re.search(r"BUILD_NUMBER (\d+)", f.read())
        if m:
            build = int(m.group(1))

if changed:
    build += 1
    with open(ver_file, "w") as f:
        f.write(
            "#pragma once\n"
            f"#define BUILD_NUMBER {build}\n"
            f'#define VERSION_STR  "v1.{build}"\n'
        )
    print(f"  Build bumped to v1.{build}")
else:
    print(f"  Version unchanged: v1.{build}")
