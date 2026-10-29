"""Punchboot setuptools configuration."""

from pathlib import Path

from setuptools import setup  # type: ignore # noqa: PGH003

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
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
        "Programming Language :: Python :: 3.13",
        "Programming Language :: Python :: 3.14",
        "Programming Language :: Python :: 3.15",
    ],
    url="https://github.com/jonasblixt/punchboot",
    packages=["punchboot"],
    package_dir={
        "punchboot": "tools/punchboot",
    },
    package_data={
        "punchboot": ["py.typed"],
    },
    python_requires=">=3.10",
    entry_points={"console_scripts": ["punchboot=punchboot.__main__:cli"]},
    install_requires=[
        "semver>=3,<4",
        "click>=8.2,<9",
        "pyusb>=1.2,<2",
        # libusb for pyusb, without it pyusb looks for the system's
        "libusb-package>=1.0.26",
    ],
)
