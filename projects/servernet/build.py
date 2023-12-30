import os
import sys
import shutil

arg_build = False
arg_deploy = False

for arg in sys.argv:
    if arg == "--build":
        arg_build = True
    elif arg == "--deploy":
        arg_deploy = True

if not arg_build and not arg_deploy:
    # force build and deploy if neither arguments were provided
    arg_build = True
    arg_deploy = True

#######################################################################

def copy_directory_contents(source_directory, destination_directory):
    destination_directory = os.path.abspath(destination_directory)

    curdir = os.path.abspath(os.curdir)
    os.chdir(source_directory)
    try:
        for root, dirs, files in os.walk("."):
            for dirname in dirs:
                os.makedirs(
                    os.path.join(destination_directory, root, dirname),
                    exist_ok=True
                )
            
            for filename in files:
                virtual_path = os.path.join(root, filename)
                shutil.copy2(
                    virtual_path,
                    os.path.join(destination_directory, virtual_path)
                )
    finally:
        os.chdir(curdir)

def remove_development_appsettings(destination_directory):
    for fn in ["appsettings.local.json", "appsettings.Development.json"]:
        path = os.path.join(destination_directory, fn)
        if os.path.exists(path):
            os.unlink(path)

#######################################################################

curdir = os.path.abspath(os.curdir)

if arg_build:
    print("Building api...", flush=True)

    # build api for windows
    os.makedirs("build/api/windows", exist_ok=True)
    n = os.system("dotnet publish DnsServer/DnsServer.csproj -o build/api/windows -c Release --runtime win-x64 -p:PublishSingleFile=false --self-contained true")
    if n != 0:
        print(f"Failed to build for windows - Code: {n}")
        sys.exit(n)

    os.makedirs("build/api/linux", exist_ok=True)
    n = os.system("dotnet publish DnsServer/DnsServer.csproj -o build/api/linux -c Release --runtime linux-x64 -p:PublishSingleFile=false --self-contained true")
    if n != 0:
        print(f"Failed to build for linux - Code: {n}")
        sys.exit(n)
    
    # build frontend
    print("Building frontend...", flush=True)
    frontend_build_dir = os.path.abspath(os.path.join(curdir, "build/frontend"))
    shutil.rmtree(frontend_build_dir, ignore_errors=True)

    os.chdir("frontend")
    try:
        # install required npm packages
        n = os.system("npm install")
        if n != 0:
            print(f"Failed to install npm tools - Code: {n}")
            sys.exit(n)

        # build frontend
        n = os.system("npm run build")
        if n != 0:
            print(f"Failed to build frontend - Code: {n}")
            sys.exit(n)
        
        shutil.copytree("build", frontend_build_dir)
    finally:
        os.chdir(curdir)

if arg_deploy:
    print("Deploying...", flush=True)
    if not os.path.exists("build/api/windows/DnsServer.exe"):
        print("The build files are missing. Cannot deploy.")
        sys.exit(1)

    if not os.path.exists("build/api/linux/DnsServer"):
        print("The build files are missing. Cannot deploy.")
        sys.exit(1)

    if not os.path.exists("build/frontend"):
        print("The build files are missing. Cannot deploy.")
        sys.exit(1)

    # package into deployment
    shutil.rmtree("deploy", ignore_errors=True)
    for platform in ["windows", "linux"]:
        os.makedirs(f"deploy/{platform}", exist_ok=True)

        copy_directory_contents(
            f"build/api/{platform}",
            f"deploy/{platform}"
        )

        remove_development_appsettings(
            f"deploy/{platform}"
        )

        shutil.rmtree(f"deploy/{platform}/wwwroot", ignore_errors=True)
        shutil.copytree("build/frontend", f"deploy/{platform}/wwwroot")

print("Done.", flush=True)