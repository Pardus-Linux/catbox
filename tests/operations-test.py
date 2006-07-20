#!/usr/bin/python

import os
import catbox
import stat

def tryOp(name, op, path):
    try:
        op(path)
        print "Done %s '%s'" % (name, path)
    except Exception, e:
        tmp = str(e.__class__)
        if '.' in tmp:
            tmp = tmp.split('.')[-1]
        print "Failed %s '%s'\n  %s: %s" % (name, path, tmp, e)

def test():
    tryOp("writing", lambda x: file(x, "w").write("lala"), "catboxtest.txt")
    tryOp("deleting", os.unlink, "catboxtest.deleteme")
    tryOp("chmoding", lambda x: os.chmod(x, stat.S_IEXEC), "catboxtest.deleteme")
    tryOp("chowning", lambda x: os.chown(x, 1000, 100), "catboxtest.deleteme")
    tryOp("hardlinking", lambda x: os.link(x, x + ".hlink"), "catboxtest.deleteme")
    tryOp("symlinking", lambda x: os.symlink(x, x + ".slink"), "catboxtest.deleteme")
    tryOp("utiming", lambda x: os.utime(x, None), "catboxtest.deleteme")
    tryOp("renaming", lambda x: os.rename(x, x + ".renamed"), "catboxtest.deleteme")
    tryOp("mkdiring", os.mkdir, "catboxtestdir")
    tryOp("rmdiring", os.rmdir, "catboxtestdir.deleteme")

if os.path.isdir("catboxtestdir.deleteme"):
    os.rmdir("catboxtestdir.deleteme")
os.mkdir("catboxtestdir.deleteme")
file("catboxtest.deleteme", "w").write("deleteme\n")
catbox.run(test, writable_paths=[])

