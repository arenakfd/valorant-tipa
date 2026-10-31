#!/usr/bin/env python3
"""Remove per-file COMPILER_FLAGS blocks that contain LLVM obfuscation passes.

Only called when the toolchain does not understand -mllvm -enable-* passes.
"""
import re
import sys

PBX = "地图.xcodeproj/project.pbxproj"

src = open(PBX, encoding="utf-8").read()
before = src.count("COMPILER_FLAGS")
out = re.sub(r' ?settings = \{COMPILER_FLAGS = "[^"]*"; \};', "", src)
after = out.count("COMPILER_FLAGS")

open(PBX, "w", encoding="utf-8").write(out)
print("COMPILER_FLAGS blocks before=%d after=%d" % (before, after))
if after:
    sys.exit("still %d COMPILER_FLAGS blocks left" % after)
