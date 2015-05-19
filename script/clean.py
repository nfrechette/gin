#!/usr/bin/env python

import io
import os
import subprocess
import sys

GIN_ROOT_DIR = os.path.abspath(os.path.dirname(os.path.dirname(__file__)))
GIN_INTERMEDIATE_DIR = os.path.join(GIN_ROOT_DIR, "intermediate")
GIN_BIN_DIR = os.path.join(GIN_ROOT_DIR, "bin")

def main():
    subprocess.call("rm -rf " + GIN_INTERMEDIATE_DIR, shell=True)
    subprocess.call("rm -rf " + GIN_BIN_DIR, shell=True)

if __name__ == '__main__':
    sys.exit(main())

