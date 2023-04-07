from io import SEEK_END, SEEK_SET
import math
import os
import secrets
import time
from typing import Iterator

def scrub(filename: str, remove=True, block_size=4096) -> None:
    # scrub files will overwrite the page region(s) the file consumed
    # and then delete the file
    with open(filename, "r+b") as f: # open for read/write and keep data
        f.seek(0, SEEK_END)
        file_size = f.tell()
        f.seek(0, SEEK_SET)
        byte_count = math.ceil(file_size / block_size) * block_size
        f.write(secrets.token_bytes(byte_count))
    
    if remove:
        time.sleep(0.1)

        # attempt to delete the file for 5 seconds
        sleep_time = 0.25
        tries = 0
        while True:
            tries = tries + 1
            if tries > math.ceil(5 / sleep_time):
                print(f"Failed to delete {filename}", flush=True)
                return

            try:
                os.unlink(filename)
                break
            except:
                time.sleep(sleep_time)

class SecureFile():
    def __init__(self, filename: str, content: str, block_size = 4096) -> None:
        self.filename = filename
        self.__filename = os.path.abspath(filename)
        self.__block_size = block_size

        with open(self.__filename, "w") as f:
            f.write(content)
        
    def __enter__(self):
        return self
    
    def __exit__(self, type, value, traceback):
        if os.path.exists(self.__filename):
            scrub(self.__filename, block_size=self.__block_size)

class DestroyFilesOnExit():
    def __init__(self, filenames: Iterator[str], block_size=4096) -> None:
        self.filenames = filenames
        self.__block_size = block_size
    
    def __enter__(self):
        pass
    
    def __exit__(self, type, value, traceback):
        for filename in self.filenames:
            if os.path.exists(filename):
                scrub(filename, block_size=self.__block_size)