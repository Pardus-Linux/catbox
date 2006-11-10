#!/usr/bin/python

import os
import sys
import catbox.catbox as catbox

path = "catboxtest.link"

def logger(event, data):
    print event, data

def test():
    os.unlink(path)

if os.path.exists(path):
    os.unlink(path)
os.symlink("/var", "catboxtest.link")
ret = catbox.run(test, writable_paths=[os.getcwd()], logger=logger)
if ret:
    print "Sandbox error: cannot remove '%s'" % path
sys.exit(ret.ret)

