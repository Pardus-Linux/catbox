#!/usr/bin/python

import sys
import os
import catbox.catbox as catbox

def tryWrite(path, legal=True):
    try:
        f = file(path, "w")
        f.write("hello world\n")
        f.close()
    except IOError, e:
        if e.errno == 13 and not legal:
            return 0
        print "Sandbox error: cannot write '%s': %s" % (path, e)
        return 1
    
    if not legal:
        print "Sandbox violation: wrote '%s'" % path
        return 1
    return 0

def test():
    ret = 0
    ret += tryWrite("/tmp/catboxtest.txt", False)
    ret += tryWrite("catboxtest.txt")
    sys.exit(ret)

logerror = 1

def logger(event, data):
    if event == "denied" and data[0] == "open" and data[1] == "/tmp/catboxtest.txt":
        global logerror
        logerror = 0
    else:
        print "Unexpected event [%s] [%s]" % (event, data)

ret = catbox.run(test, writable_paths=[os.getcwd()], logger=logger)
ret.ret += logerror
sys.exit(ret.ret)

