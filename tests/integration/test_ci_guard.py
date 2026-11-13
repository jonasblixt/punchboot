"""The compat CI job's guard, scripts/ci/compat-guard.sh (compatibility.md, "Guards").

abi.txt only grows and the transcripts never change, against the commit a
change starts from. On a push to master the merge base with master is the
pushed head itself, so the guard compared the push with itself (F-227); it
compares with the push's previous head. These tests run it in a scratch git
repository, with the environment each CI event gives it.
"""

from __future__ import annotations

import os
import subprocess
from typing import TYPE_CHECKING

import pytest

from .pbtest import REPO_ROOT

if TYPE_CHECKING:
    import pathlib

GUARD = REPO_ROOT / "scripts" / "ci" / "compat-guard.sh"
NULL = "0" * 40


def _env(repo: pathlib.Path, **extra: str) -> dict[str, str]:
    env = {k: v for k, v in os.environ.items() if not k.startswith(("GITHUB_", "GIT_"))}
    env |= {
        # Never the repository the tests run in
        "GIT_CEILING_DIRECTORIES": str(repo.parent),
        "GIT_AUTHOR_NAME": "test",
        "GIT_AUTHOR_EMAIL": "test@example.com",
        "GIT_COMMITTER_NAME": "test",
        "GIT_COMMITTER_EMAIL": "test@example.com",
        "BEFORE": "",
    }
    return env | extra


def _git(repo: pathlib.Path, *args: str) -> str:
    run = subprocess.run(  # noqa: S603
        ["git", *args],  # noqa: S607
        cwd=repo,
        env=_env(repo),
        check=True,
        capture_output=True,
        text=True,
    )
    return run.stdout.strip()


def _commit(repo: pathlib.Path, abi: str, transcript: str = "step a\n") -> str:
    (repo / "tests" / "compat" / "abi.txt").write_text(abi)
    (repo / "tests" / "compat" / "transcripts" / "a.txt").write_text(transcript)
    _git(repo, "add", "-A")
    _git(repo, "commit", "-q", "-m", "commit")
    return _git(repo, "rev-parse", "HEAD")


@pytest.fixture
def repo(tmp_path: pathlib.Path) -> pathlib.Path:
    """Master's previous head with two values, and a pushed head that has one."""
    repo = tmp_path / "repo"
    (repo / "tests" / "compat" / "transcripts").mkdir(parents=True)
    (repo / "tests" / "compat" / "bpak").mkdir()
    (repo / "tests" / "compat" / "bpak" / "SHA256SUMS").write_text("")
    _git(repo, "init", "-q", "-b", "master")
    _commit(repo, "value A 1\nvalue B 2\n")
    _commit(repo, "value A 1\n")
    # The clone of a push to master: origin/master is the pushed head
    _git(repo, "update-ref", "refs/remotes/origin/master", "HEAD")
    return repo


def _guard(repo: pathlib.Path, **env: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(  # noqa: S603
        [str(GUARD)], cwd=repo, env=_env(repo, **env), check=False, capture_output=True, text=True
    )


def test_a_push_is_compared_with_its_previous_head(repo: pathlib.Path) -> None:
    run = _guard(repo, BEFORE=_git(repo, "rev-parse", "HEAD^"))

    assert run.returncode == 1, run.stdout
    assert "value B 2" in run.stdout


def test_a_push_without_its_previous_head_is_compared_with_the_parent(repo: pathlib.Path) -> None:
    # A new branch, or a force push: the previous head isn't in the clone
    for before in (NULL, "1" * 40):
        run = _guard(repo, BEFORE=before)

        assert run.returncode == 1, run.stdout
        assert "comparing with HEAD's parent" in run.stdout


def test_a_pull_request_is_compared_with_the_merge_base(repo: pathlib.Path) -> None:
    _git(repo, "update-ref", "refs/remotes/origin/master", "HEAD^")

    assert _guard(repo, GITHUB_BASE_REF="master").returncode == 1


def test_values_added_and_transcripts_kept_pass(repo: pathlib.Path) -> None:
    before = _commit(repo, "value A 1\nvalue C 3\n")
    _commit(repo, "value A 1\nvalue C 3\nvalue D 4\n")

    assert _guard(repo, BEFORE=before).returncode == 0


def test_a_changed_transcript_fails(repo: pathlib.Path) -> None:
    before = _git(repo, "rev-parse", "HEAD")
    _commit(repo, "value A 1\n", transcript="step b\n")

    run = _guard(repo, BEFORE=before)

    assert run.returncode == 1
    assert "tests/compat/transcripts/a.txt" in run.stdout
