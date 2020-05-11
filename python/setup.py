#!/usr/bin/env python

import sys
import re
import os
import setuptools
import platform
from setuptools import find_packages

def setup(ext_modules):
    setuptools.setup(
        name='punchboot',
        version="0.1.0",
        description=('Python wrapper for the punchboot library'),
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
        )

setup([setuptools.Extension(name='punchboot.punchboot',
                            sources=[
                                'pb_wrapper.c',
                            ],
                            include_dirs=[
                                '../include',
                            ],
                            libraries=[
                                'punchboot',
                                'usb-1.0',
                                'bpak',
                            ],
                            library_dirs=[
                                'lib/.libs'
                            ],
                            )])
