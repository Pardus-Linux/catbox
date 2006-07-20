#!/usr/bin/python

import os
import catbox

def tryWrite(path):
    try:
        f = file(path, "w")
        f.write("hello world\n")
        f.close()
        print "Wrote '%s'" % path
    except Exception, e:
        tmp = str(e.__class__)
        if '.' in tmp:
            tmp = tmp.split('.')[-1]
        print "Cannot write '%s'\n  %s: %s" % (path, tmp, e)

def test():
    # Simple test to see that read access is not controlled
    file("/etc/issue").read()
    # Try to write different places
    tryWrite("/tmp/catboxtest.txt")
    tryWrite("%s/catboxtest.txt" % '/'.join(os.getcwd().split('/')[:-1]))
    tryWrite("catboxtest.txt")

catbox.run(test, writable_paths=[os.getcwd()])

