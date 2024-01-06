import os, sys, testvm

SSH = f"{testvm.user}@{testvm.ip}"

#####################################################

def exec_commands(commands):
    cmd = "ssh {} {}".format(
        SSH,
        " ; ".join(commands)
    )
    
    n = os.system(cmd)
    if n != 0:
        print(f"Failed to execute command: {cmd}")
        sys.exit(n)

def copy_files(destination, recursive, files):
    cmd = "scp {} {} {}:{}".format(
        "-r" if recursive else "",
        " ".join(files),
        SSH,
        destination
    )
    
    n = os.system(cmd)
    if n != 0:
        print(f"Failed to execute command: {cmd}")
        sys.exit(n)

#####################################################

exec_commands([
    "sudo systemctl stop cdns",
    "sudo systemctl disable cdns",
    f"sudo rm -R {testvm.destination}",
    f"sudo mkdir {testvm.destination}",
    f"sudo mkdir {testvm.destination}/legacy",
    f"sudo chmod -R 754 {testvm.destination}",
    f"sudo chown -R {testvm.user} {testvm.destination}",
    f"sudo chgrp -R {testvm.user} {testvm.destination}"
])

copy_files(testvm.destination, True, [
    "deploy/linux/*"
])

copy_files(f"{testvm.destination}/legacy", True, [
    "DnsServer/legacy/*"
])

copy_files(testvm.destination, False, [
    "cdns.service"
])

exec_commands([
    f"sudo chmod +x {testvm.destination}/DnsServer",
    f"sudo cp {testvm.destination}/cdns.service /lib/systemd/system/cdns.service",
    "sudo systemctl enable cdns",
    "sudo systemctl start cdns",
])