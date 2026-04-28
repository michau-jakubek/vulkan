import subprocess
import sys
import os

output = sys.argv[1]

git_hash = subprocess.check_output(["git", "rev-parse", "HEAD"]).decode().strip()

new_content = f'#define VTF_GIT_VERSION "{git_hash}"\n'

old_content = None
if os.path.exists(output):
    with open(output, "r") as f:
        old_content = f.read()

if old_content != new_content:
    with open(output, "w") as f:
        f.write(new_content)

