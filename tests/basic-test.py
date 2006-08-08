#!/usr/bin/python

import sys
import os
import catbox

def tryRead(path):
    try:
        file(path).read()
        return 0
    except Exception, e:
        print "Sandbox error: cannot read '%s': %s" % (path, e)
        return 1

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
    # Simple test to see that read access is not denied
    ret += tryRead("/etc/issue")
    # Try to write to invalid places
    ret += tryWrite("/tmp/catboxtest.txt", False)
    ret += tryWrite("%s/catboxtest.txt" % '/'.join(os.getcwd().split('/')[:-1]), False)
    # Try to write to valid places
    ret += tryWrite("catboxtest.txt")
    sys.exit(ret)

ret = catbox.run(test, writable_paths=[os.getcwd()])
sys.exit(ret)
