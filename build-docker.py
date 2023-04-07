import os
import sys
import re
import time
from deploy.pyutil import config
from deploy.pyutil import util

started = time.time()

if not os.path.exists("deploy/deploy.json"):
    print("deploy/deploy.json missing!", flush=True)
    sys.exit(1)

cfg = config.Config("deploy/deploy.json")
registry = cfg.get_registry()
version = cfg.get_cdns_version()

# increment version
rgx = re.compile("^\\d+\\.\\d+\\.(\\d+)$")
m = rgx.match(version)
if m:
    # if the version is a 1.0.0 format then increment the last digit, otherwise do nothing (in case version is a string)
    version = "{}{}".format(
        version[:m.start(1)],
        int(m.group(1)) + 1
    )

    cfg.set_cdns_version(version)

print(f"Building CDNS (version: {version})", flush=True)

with util.SecureFile("registry.key", cfg.registry.password) as sf:
    n = os.system(f"cat {sf.filename} | docker login {cfg.registry.url} --username {cfg.registry.username} --password-stdin")
    if n != 0:
        sys.exit(n)

# number of cores to build on
number_of_cores = os.cpu_count() * 2

n = os.system(f"docker build -t {cfg.registry.url}/cdns:{version} --build-arg CPU_CORES={number_of_cores} .")
if n != 0:
    sys.exit(n)

n = os.system(f"docker push {cfg.registry.url}/cdns:{version}")
if n != 0:
    sys.exit(n)

curdir = os.path.abspath(os.curdir)
os.chdir("deploy")
try:
    os.system("python . deploy")
finally:
    os.chdir(curdir)

delta = time.time() - started
print("Build and deploy took {:0>2.0f}:{:0>2.0f}".format(
    delta / 60,
    delta % 60
), flush=True)