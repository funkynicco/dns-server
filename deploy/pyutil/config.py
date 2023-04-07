import os
import json
import re
from typing import Iterator

class Dns():
    def __init__(self, cfg) -> None:
        self.port = cfg["port"]

class Cluster():
    def __init__(self, cfg) -> None:
        self.ip = cfg["ip"]
        self.port = cfg["port"]
        self.subnet = cfg["subnet"]
        self.broadcast = cfg["broadcast"]
        self.key = cfg["key"]
    
    #def get_ip_and_mask(self):
    #    rgx = re.compile("^(\\d+\\.\\d+\\.\\d+\\.\\d+)\\/(\\d+)$")
    #    m = rgx.match(self.ip)
    #    return m.group(1), int(m.group(2))

class Target():
    def __init__(self, cfg) -> None:
        self.name = cfg["name"]
        self.enabled = cfg["enabled"]
        self.ip = cfg["ip"]
        self.username = cfg["username"]
        self.path = cfg["path"]
        self.dns = Dns(cfg["dns"])
        self.cluster = Cluster(cfg["cluster"])
    
    def __str__(self) -> str:
        disabled_str = ""
        if not self.enabled:
            disabled_str = " [DISABLED]"

        return f"{self.name} ({self.ip}){disabled_str} {self.username} {self.path}"

class Registry():
    def __init__(self, cfg) -> None:
        self.url = cfg["url"]
        self.username = cfg["username"]
        self.password = cfg["password"]
    
    def __str__(self) -> str:
        mask = "".ljust(6, "*")
        return f"{self.url} (username: {self.username}, password: {mask})"

class Config():
    def __init__(self, filename) -> None:
        self.__filename = os.path.abspath(filename)
        self.__cfg = None

        with open(self.__filename, "r") as f:
            self.__cfg = json.load(f)
        
        self.registry = Registry(self.__cfg["registry"])

    def __str__(self) -> str:
        return json.dumps(self.__cfg, indent=2)
    
    def __save(self):
        with open(self.__filename, "w") as f:
            json.dump(self.__cfg, f, indent=2)

    #################################################

    def get_cdns_version(self) -> None:
        return self.__cfg["cdns"]["version"]
    
    def set_cdns_version(self, version) -> None:
        self.__cfg["cdns"]["version"] = version
        self.__save()

    def get_targets(self, include_disabled = False) -> Iterator[Target]:
        return map(lambda x: Target(x), filter(lambda y: y["enabled"] or include_disabled, self.__cfg["targets"]))
    
    def get_target(self, name) -> Target:
        for target in self.get_targets():
            if target.name == name:
                return target
        
        return None
    
    def get_registry(self) -> Registry:
        return Registry(self.__cfg["registry"])