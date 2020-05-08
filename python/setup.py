#!/usr/bin/env python

import sys
import re
import os
import setuptools
import platform
from setuptools import find_packages
from setuptools.command.egg_info import egg_info

SRC_PATH = os.path.dirname(__file__)

class EggInfoCommand(egg_info):
    def run(self):
        if "build" in self.distribution.command_obj:
            build_command = self.distribution.command_obj["build"]

            self.egg_base = build_command.build_base

            self.egg_info = os.path.join(self.egg_base, os.path.basename(self.egg_info))

        egg_info.run(self)

def setup(ext_modules):
    setuptools.setup(
        name='punchboot',
        version="0.1.0",
        description=('Python wrapper for the punchboot library'),
        package_dir={"": SRC_PATH,},
        long_description="blerp", #open('../README.rst', 'r').read(),
        author='Jonas Blixt',
        author_email='jonpe960@gmail.com',
        license='BSD3',
        classifiers=[
            'License :: OSI Approved :: BSD3 License',
            'Programming Language :: Python :: 3',
        ],
        url='https://github.com/jonasblixt/punchboot-tools',
        ext_modules=ext_modules,
        test_suite="tests",
        cmdclass={"egg_info": EggInfoCommand,})

setup(None)
