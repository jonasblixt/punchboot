# punchboot and pbstate are built from the punchboot tree this test bench
# is in
PB_TESTBENCH_PUNCHBOOT_SRCDIR = $(realpath $(BR2_EXTERNAL_PB_TESTBENCH_PATH)/..)

# What is not part of the punchboot sources: build directories, this test
# bench, with its Buildroot, downloads and output, and the hidden directories
# at the top, which are tools' own (worktrees with builds and the tests'
# sparse disk images in them, which rsync would write out in full)
PB_TESTBENCH_PUNCHBOOT_RSYNC_EXCLUSIONS = \
	--exclude=/testbench \
	--exclude=/build* \
	--exclude=/blobs \
	--exclude=/.*/ \
	--exclude=*.egg-info

# bpak and kickstart are built from a branch of their git repositories, by
# default the ones next to the punchboot tree. The version is the commit at
# the head of the branch, looked up each time make runs: a new commit is a
# new version, downloaded and built again. Override on the command line,
# e.g. `make BOARD=jiffy PB_TESTBENCH_BPAK_BRANCH=master`.
PB_TESTBENCH_BPAK_REPO ?= $(realpath $(BR2_EXTERNAL_PB_TESTBENCH_PATH)/../../bpak)
PB_TESTBENCH_BPAK_BRANCH ?= master
PB_TESTBENCH_KICKSTART_REPO ?= $(realpath $(BR2_EXTERNAL_PB_TESTBENCH_PATH)/../../kickstart)
PB_TESTBENCH_KICKSTART_BRANCH ?= master

# $(call pb_testbench_git_head,<repository>,<branch>)
pb_testbench_git_head = $(or \
	$(firstword $(shell git ls-remote $(1) refs/heads/$(2) 2>/dev/null)), \
	$(error No branch $(2) in the git repository '$(1)'))

include $(sort $(wildcard $(BR2_EXTERNAL_PB_TESTBENCH_PATH)/boot/*/*.mk))
include $(sort $(wildcard $(BR2_EXTERNAL_PB_TESTBENCH_PATH)/package/*/*.mk))
