# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#

import sys
import os
import shlex
import sphinx_rtd_theme
import alabaster
import subprocess

# import sys
# sys.path.insert(0, os.path.abspath('.'))


# -- Project information -----------------------------------------------------

project = "punchboot"
copyright = "2023, Jonas Blixt"
author = "Jonas Blixt"

# The full version, including alpha/beta/rc tags
version = "1.0.0"
release = version

# -- General configuration ---------------------------------------------------
subprocess.call("set", shell=True)
subprocess.call("doxygen doxygen.cfg", shell=True)

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
# sys.path.insert(0, os.path.abspath('.'))

# -- General configuration ------------------------------------------------

# If your documentation needs a minimal Sphinx version, state it here.
# needs_sphinx = '1.0'
breathe_projects = {
    "pb": "xml/",
}

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = [
    "alabaster",
    "breathe",
    "sphinx.ext.extlinks",
    "sphinx.ext.autodoc",
    "sphinx.ext.viewcode",
    "sphinxcontrib.plantuml",
    "sphinx.ext.autosectionlabel",
]

# Add any paths that contain templates here, relative to this directory.
templates_path = ["_templates"]

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = []


# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
html_theme = "sphinx_rtd_theme"

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ["_static"]

# extlins
extlinks = {
    "github-blob": ("https://github.com/jonasblixt/punchboot/blob/" + version + "/%s", "%s"),
    "github-tree": ("https://github.com/jonasblixt/punchboot/tree/" + version + "/%s", ""),
    "codecov": ("https://codecov.io/gh/jonasblixt/punchboot/src/" + version + "/%s", ""),
    "codecov-tree": ("https://codecov.io/gh/jonasblixt/punchboot/tree/" + version + "/%s", ""),
}
