"""Punchboot setuptools configuration."""

import os
import platform
from pathlib import Path

from setuptools import Extension, setup  # type: ignore

pb_base_path: str = "tools/punchboot"
plat: str = platform.system()

if plat == "Windows":
    _libraries = ["libusb-1.0"]
    _library_dirs = ["vcpkg_installed\\x64-windows\\lib"]
    _include_dirs = ["include", "vcpkg_installed\\x64-windows\\include"]
elif plat == "Darwin":
    # We have two different builds of libusb to build x86_64 and arm64 wheels
    # on an x86_64 runner.
    # CI system is expected to set 'PB_MACOS_ARCH' to help out, since we can't
    # get target arch here when cross compiling wheels.
    _target_arch = os.environ["PB_MACOS_ARCH"]
    _libraries = ["usb-1.0"]
    _library_dirs = [f"libusb_{_target_arch}/1.0.26/lib"]
    _include_dirs = ["include", f"libusb_{_target_arch}/1.0.26/include"]
else:
    _libraries = ["usb-1.0"]
    _library_dirs = []
    _include_dirs = ["include"]


_srcs = [
    "src/wire.c",
    "src/lib/bpak.c",
    f"{pb_base_path}/error.c",
    f"{pb_base_path}/api.c",
    f"{pb_base_path}/api_authentication.c",
    f"{pb_base_path}/api_board.c",
    f"{pb_base_path}/api_boot.c",
    f"{pb_base_path}/api_device.c",
    f"{pb_base_path}/api_misc.c",
    f"{pb_base_path}/api_partition.c",
    f"{pb_base_path}/api_slc.c",
    f"{pb_base_path}/api_stream.c",
    f"{pb_base_path}/usb.c",
    f"{pb_base_path}/python_wrapper.c",
    f"{pb_base_path}/exceptions.c",
]

# For now we only enable the 'socket' transport on linux machines, it's only
# used for running the tests on punchboot.
if plat == "Linux":
    _srcs.append(f"{pb_base_path}/socket.c")


setup(
    name="punchboot",
    version=Path("version.txt").read_text().strip(),
    description="Punchboot tools",
    long_description=Path("README.md").read_text(),
    long_description_content_type="text/markdown",
    author="Jonas Blixt",
    author_email="jonpe960@gmail.com",
    license="BSD",
    classifiers=[
        "Development Status :: 5 - Production/Stable",
        "Intended Audience :: Developers",
        "Topic :: Software Development :: Embedded Systems",
        "License :: OSI Approved :: BSD License",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
    ],
    url="https://github.com/jonasblixt/punchboot",
    packages=["punchboot"],
    package_dir={
        "punchboot": f"{pb_base_path}/punchboot",
    },
    package_data={
        "punchboot": ["py.typed"],
    },
    python_requires=">=3.8",
    entry_points={"console_scripts": ["punchboot=punchboot.__main__:cli"]},
    ext_modules=[
        Extension(
            name="_punchboot",
            sources=_srcs,
            libraries=_libraries,
            library_dirs=_library_dirs,
            include_dirs=_include_dirs,
        )
    ],
    install_requires=["semver>=3,<4", "click>=8,<9"],
)
