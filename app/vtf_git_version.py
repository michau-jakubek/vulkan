import subprocess
import sys

output = sys.argv[1]

git_hash = subprocess.check_output(["git", "rev-parse", "HEAD"]).decode().strip()

with open(output, "w") as f:
    f.write(f'#define VTF_GIT_VERSION "{git_hash}"\n')

