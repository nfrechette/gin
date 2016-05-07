#!/usr/bin/env python

import os
import sys
import subprocess

GIN_ROOT_DIR = os.path.abspath(os.path.dirname(os.path.dirname(__file__)))
GIN_EXT_DIR = os.path.join(GIN_ROOT_DIR, "external")
GIN_INCLUDE_DIR = os.path.join(GIN_ROOT_DIR, "include")
GIN_TEST_DIR = os.path.join(GIN_ROOT_DIR, "test")
GIN_INTERMEDIATE_DIR = os.path.join(GIN_ROOT_DIR, "intermediate")
GIN_BIN_DIR = os.path.join(GIN_ROOT_DIR, "bin")

CATCH_INCLUDE_DIR = os.path.join(GIN_EXT_DIR, "catch-1.1.1")

#COMPILER = "clang -x c++ -std=gnu++11 -stdlib=libc++" 
COMPILER = "clang++"
LINKER = "clang++"
WARNING_LIST = ["no-trigraphs", "no-missing-field-initializers",
                "no-missing-prototypes", "error=return-type",
                "unreachable-code", "no-non-virtual-dtor",
                "no-overloaded-virtual", "no-exit-time-destructors",
                "no-missing-braces", "parentheses", "switch",
                "unused-function", "no-unused-label",
                "no-unused-parameter", "unused-variable",
                "unused-value", "empty-body", "conditional-uninitialized",
                "no-unknown-pragmas", "no-shadow", "no-four-char-constants",
                "no-conversion", "constant-conversion", "int-conversion",
                "bool-conversion", "enum-conversion", "shorten-64-to-32",
                "no-newline-eof", "no-c++11-extensions", "no-sign-conversion",
                "deprecated-declarations", "invalid-offsetof"]
FLAGS = ["no-rtti", "asm-blocks", "strict-aliasing",
         "visibility-inlines-hidden"]
EXTRA_FLAGS = ["-std=c++11", "-c", "-g", "-DDEBUG=1", "-O0", "-MMD"]
INCLUDE_DIRS = [CATCH_INCLUDE_DIR, GIN_INCLUDE_DIR]
EXEC_NAME = "test"

def parse_dependencies(obj):
    dep_path = os.path.join(GIN_INTERMEDIATE_DIR, obj.replace(".o", ".d"))
    if not os.path.exists(dep_path):
        print "No dependency file found at: {}".format(dep_path)
        return []

    lines = [line.strip() for line in open(dep_path)]

    # Remove the first line which references the object file itself
    del lines[0]

    # Strip tailing ' \' stuff
    lines = [line.rstrip(" \\") for line in lines]

    return lines

def should_compile(obj):
    obj_path = os.path.join(GIN_INTERMEDIATE_DIR, obj)
    if not os.path.exists(obj_path):
        # Object file doesn't exist, build it
        return True

    obj_mtime = os.path.getmtime(obj_path)

    deps = parse_dependencies(obj)
    for dep_path in deps:
        if not os.path.exists(dep_path):
            # Dependency file doesn't exist, assume we are dirty
            return True

        dep_mtime = os.path.getmtime(dep_path)
        if dep_mtime > obj_mtime:
            # Dependency modify time is newer than object modify time, we are dirty
            return True

    return False

def main():
    #print "gin root: %s" % GIN_ROOT_DIR
    #print "gin external: %s" % GIN_EXT_DIR
    #print "gin include: %s" % GIN_INCLUDE_DIR
    #print "gin test: %s" % GIN_TEST_DIR

    # Make sure output directories exists
    if not os.path.exists(GIN_BIN_DIR):
        os.makedirs(GIN_BIN_DIR)

    if not os.path.exists(GIN_INTERMEDIATE_DIR):
        os.makedirs(GIN_INTERMEDIATE_DIR)

    test_sources = [file for file in os.listdir(GIN_TEST_DIR) if os.path.isfile(os.path.join(GIN_TEST_DIR, file)) and file.endswith(".cpp")]

    test_obj = [file.replace(".cpp", ".o") for file in test_sources]

    test_exec_list = zip(test_sources, test_obj)

    warnings = " ".join(["-W{0}".format(item) for item in WARNING_LIST])
    flags = " ".join(["-f{0}".format(item) for item in FLAGS])
    extra_flags = " ".join(EXTRA_FLAGS)
    include_dirs = " ".join(["-I{0}".format(item) for item in INCLUDE_DIRS])

    obj_list = []
    compilation_failed = False
    is_exec_dirty = False

    # Compile everything into object files
    for src, obj in test_exec_list:
        src_path = os.path.join(GIN_TEST_DIR, src)
        obj_path = os.path.join(GIN_INTERMEDIATE_DIR, obj)
        obj_cmd = "-o " + obj_path

        obj_list.append(obj_path)

        if not should_compile(obj):
            print "Skipping '%s'..." % src_path
            continue

        compile_list = [COMPILER, warnings, flags, extra_flags,
                        include_dirs, src_path, obj_cmd]
        compile_cmd = " ".join(compile_list)

        #print compile_cmd
        print "Compiling '%s'..." % src_path

        result = subprocess.call(compile_cmd, shell=True)
        compilation_failed = compilation_failed or (result != 0)
        is_exec_dirty = True

    if compilation_failed:
        return -1

    exec_path = os.path.join(GIN_BIN_DIR, EXEC_NAME)

    # Link everything
    if is_exec_dirty:
        objs = " ".join(obj_list)
        exec_out_cmd = "-o " + exec_path
        link_list = [LINKER, objs, exec_out_cmd]
        link_cmd = " ".join(link_list)

        #print link_cmd
        print "Linking   '%s'..." % exec_path

        result = subprocess.call(link_cmd, shell=True)

        if result != 0:
            return -1

    # Execute
    print "Executing '%s'..." % exec_path

    subprocess.call(exec_path)

if __name__ == '__main__':
    sys.exit(main())

