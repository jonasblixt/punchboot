################################################################################
#
# python-scikit-build-core
#
################################################################################

# The build backend of the bpak python package, not in Buildroot
PYTHON_SCIKIT_BUILD_CORE_VERSION = 1.1.1
PYTHON_SCIKIT_BUILD_CORE_SOURCE = scikit_build_core-$(PYTHON_SCIKIT_BUILD_CORE_VERSION).tar.gz
PYTHON_SCIKIT_BUILD_CORE_SITE = https://files.pythonhosted.org/packages/b2/1a/8c00b19c0a1e7acf890676af2efa430339d38e59fa9437f2aab8517af3f4
PYTHON_SCIKIT_BUILD_CORE_SETUP_TYPE = hatch
PYTHON_SCIKIT_BUILD_CORE_LICENSE = Apache-2.0
PYTHON_SCIKIT_BUILD_CORE_LICENSE_FILES = LICENSE
HOST_PYTHON_SCIKIT_BUILD_CORE_DEPENDENCIES = \
	host-python-hatch-vcs \
	host-python-packaging \
	host-python-pathspec

$(eval $(host-python-package))
