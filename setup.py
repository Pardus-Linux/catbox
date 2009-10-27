#!/usr/bin/python
# -*- coding: utf-8 -*-
#
# Copyright (C) 2006-2008, TUBITAK/UEKAE
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version. Please read the COPYING file.
#

import sys
import os
import glob
import shutil
import subprocess
from distutils.core import setup, Extension
from distutils.command.install import install

version='1.1'

distfiles = """
    setup.py
    OKUBENİ
    README
    src/*.c
    src/*.h
    tests/*.py
"""

if 'dist' in sys.argv:
    distdir = "catbox-%s" % version
    list = []
    for t in distfiles.split():
        list.extend(glob.glob(t))
    if os.path.exists(distdir):
        shutil.rmtree(distdir)
    os.mkdir(distdir)
    for file_ in list:
        cum = distdir[:]
        for d in os.path.dirname(file_).split('/'):
            dn = os.path.join(cum, d)
            cum = dn[:]
            if not os.path.exists(dn):
                os.mkdir(dn)
        shutil.copy(file_, os.path.join(distdir, file_))
    os.popen("tar -czf %s %s" % ("catbox-" + version + ".tar.gz", distdir))
    shutil.rmtree(distdir)
    sys.exit(0)


class Install(install):
    def finalize_options(self):
        # NOTE: for Pardus distribution
        if os.path.exists("/etc/pardus-release"):
            self.install_platlib = '$base/lib/pardus'
            self.install_purelib = '$base/lib/pardus'
        install.finalize_options(self)
    
    def run(self):
        install.run(self)

source = [
    'src/catbox.c',
    'src/core.c',
    'src/syscall.c',
    'src/paths.c',
    'src/retval.c',
]

setup(
    name='catbox',
    version=version,
    ext_modules=[Extension('catbox', source, extra_compile_args=["-Wall"])],
    cmdclass = {
        'install' : Install
    }
)
