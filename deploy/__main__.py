#import argparse
import os
import shutil
import sys
import time
from pyutil.util import DestroyFilesOnExit
from pyutil import config
from pyutil import util

deployment_json_filename = "deploy.json"
compose_env_filename = "docker-compose.env"

command = None
target_name = None
i = 1

#########################################################################################################

while i < len(sys.argv):
    arg = sys.argv[i]
    if arg == "--target":
        if i + 1 == len(sys.argv):
           print("Error: must pass argument to --target")
           sys.exit(1)

        target_name = sys.argv[i + 1]
        i = i + 1
    else:
        command = arg

    i = i + 1

if command == "deploy":
    # deploy is run on development PC and copies this script to remote targets and executes "run"

    if os.path.exists("deploy.zip"):
        os.unlink("deploy.zip")
    
    if os.path.exists("deploy"):
        os.unlink("deploy")

    # zip a deployment package of the required python files for targets
    os.system("zip deploy.zip {}".format(" ".join([
        "__main__.py",
        "pyutil/__init__.py",
        "pyutil/config.py",
        "pyutil/util.py",
    ])))

    # create an executable python file for linux by appending zip to a bash script
    os.system("echo #!/usr/bin/env python3 | cat - deploy.zip > deploy")
    os.unlink("deploy.zip")

    try:
        with DestroyFilesOnExit(["docker-compose-new.yml"]):
            shutil.copy2("docker-compose.yml", "docker-compose-new.yml")
            cfg = config.Config(deployment_json_filename)
            for target in cfg.get_targets():
                print(f"Deploying to {target.name} ({target.ip}) ...", flush=True)
                print("------------------------------------------------------------------", flush=True)
                os.system(f"scp deploy {deployment_json_filename} docker-compose-new.yml {target.username}@{target.ip}:{target.path}")
                os.system(f"ssh -q -t {target.username}@{target.ip} chmod a+x {target.path}/deploy ; python3 {target.path}/deploy --target {target.name} run")
                print("------------------------------------------------------------------", flush=True)
                print("", flush=True)
    finally:
        os.unlink("deploy")

elif command == "run":
    with DestroyFilesOnExit([deployment_json_filename]):
        # this is executed from the target machine itself
        cfg = config.Config(deployment_json_filename)
        target = cfg.get_target(target_name)
        docker_pull_url = f"{cfg.registry.url}/cdns:{cfg.get_cdns_version()}"

        compose_env_content = "\n".join([
            f"REGISTRY_URL={cfg.registry.url}",
            f"CDNS_VERSION={cfg.get_cdns_version()}",
            f"BIND_IP={target.ip}",
            f"BIND_PORT={target.dns.port}",
            f"CLUSTER_KEY={target.cluster.key}",
            f"CLUSTER_IP={target.cluster.ip}",
            f"CLUSTER_PORT={target.cluster.port}",
            f"CLUSTER_SUBNET={target.cluster.subnet}",
            f"CLUSTER_BROADCAST={target.cluster.broadcast}",
        ])

        with util.SecureFile("reg.key", cfg.registry.password) as sf:
            if os.path.exists("docker-compose.yml"):
                # shutdown current docker if its running
                os.system(f"env $(cat {compose_env_filename}) docker-compose down > /dev/null")
                os.unlink("docker-compose.yml")
            
            shutil.copy2("docker-compose-new.yml", "docker-compose.yml")
            os.unlink("docker-compose-new.yml")

            n = os.system(f"cat {sf.filename} | docker login {cfg.registry.url} --username {cfg.registry.username} --password-stdin")
            if n != 0:
                sys.exit(n)

            with open(compose_env_filename, "w") as f:
                f.write(compose_env_content)
            
            n = os.system(f"docker pull {docker_pull_url}")
            if n != 0:
                sys.exit(n)
            
            n = os.system(f"env $(cat {compose_env_filename}) docker-compose --env-file {compose_env_filename} up -d")
            if n != 0:
                sys.exit(n)

elif command == "log":
    def check_running():
        return os.system(f"env $(cat {compose_env_filename}) docker-compose exec cdns echo > /dev/null") == 0

    while True:
        time.sleep(0.5)
        if not check_running():
            continue

        os.system("clear")
        os.system(f"env $(cat {compose_env_filename}) docker-compose logs -f")
