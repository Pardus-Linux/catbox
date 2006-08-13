#!/usr/bin/python

import os
import sys
import catbox

path = "catboxtest.link"

def test():
    os.unlink(path)

if os.path.exists(path):
    os.unlink(path)
os.symlink("/var", "catboxtest.link")
ret = catbox.run(test, writable_paths=[os.getcwd()])
if ret:
    print "Sandbox error: cannot remove '%s'" % path
sys.exit(ret)

