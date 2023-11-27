"""Punchboot setuptools configuration."""

from setuptools import setup, Extension

pb_base_path: str = "tools/punchboot"

setup(
    name="punchboot",
    version=open("version.txt").read().strip(),
    description="Punchboot tools",
    author="Jonas Blixt",
    author_email="jonpe960@gmail.com",
    license="BSD",
    url="https://github.com/jonasblixt/punchboot",
    packages=["punchboot"],
    package_dir={
        "punchboot": f"{pb_base_path}/punchboot",
    },
    package_data={
        "punchboot": ["py.typed"],
    },
    entry_points={"console_scripts": ["punchboot=punchboot.__main__:cli"]},
    ext_modules=[
        Extension(
            name="_punchboot",
            sources=[
                f"src/wire.c",
                f"src/lib/bpak.c",
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
                f"{pb_base_path}/socket.c",
                f"{pb_base_path}/usb.c",
                f"{pb_base_path}/python_wrapper.c",
                f"{pb_base_path}/exceptions.c",
            ],
            libraries=["usb-1.0"],
            include_dirs=["include"],
        )
    ],
    install_requires=["semver>=3,<4", "click>=8,<9"],
)
