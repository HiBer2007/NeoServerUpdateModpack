---
title: Contributing
description: Branch/commit conventions, PR checklist, code rules (UTF-8 paths, English logs, no comments) and documentation updates.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.6/developer/contributing.md
---

# Contributing

This page summarizes pre-commit conventions: branch strategy, commit messages, PR checks, hard code rules, and docs/code synchronization. Build commands and path rules follow the repository's `AGENTS.md` and `CMakePresets.json`.

## Branches

- `main` stays buildable/deployable; history is small steps on `main`.
- External contributions use short-lived topic branches: `feat/<scope>-<topic>`, `fix/<scope>-<topic>`, `docs/<topic>`, deleted after merge.
- Do not rewrite pushed `main` history; use `git revert`.
- One branch = one change; unrelated formatting increases review cost.
- Submodule changes are committed/pushed in their own repos first; the main repo updates the gitlink.

## Commit messages

Conventional Commits; subject in English, imperative, ≤72 chars; body may explain "why" in Chinese:

```text
<type>(<scope>): <summary>

<optional body: problem, constraints, verification>
```

| Type | Use |
|---|---|
| `feat` | New capability/format/command |
| `fix` | Bug fix |
| `chore` | Version bumps, dependency/submodule pointers, maintenance |
| `docs` | Docs only |
| `refactor` | Structural change without behavior change |
| `build` | CMake, presets, packaging |
| `release` | Release commit, e.g. `release: NSUM 1.0.5` |

## Code rules (hard requirements)

- All logs English, no non-ASCII in sources (GUI text excepted);
- Do not add comments; keep code self-documenting;
- Windows paths always UTF-8 (`fsu8::path`, `GetModuleFileNameW`, UTF-8 argv);
- Plugin factories must be `extern "C"` + `__declspec(dllexport)`;
- After changing shared class members/interfaces, rebuild every dependent target;
- Never read moved-from objects; no `std::string`/Qt crossing DLL boundaries without ABI care.

## PR checklist

- [ ] Build passes (`neo_deploy`);
- [ ] Smoke test after layout/class-member changes;
- [ ] `docs_check` passes when docs change;
- [ ] Commit message follows the convention;
- [ ] No build artifacts, `stores/`, `crash-report/`, logs committed;
- [ ] Public API changes update the matching module `usage.md`;
- [ ] Version only bumped when requested.

## Submodules

- Change and push in the submodule repository first;
- Update the main repository gitlink (`git add <submodule>`);
- Do not commit submodule worktree changes as if they were main-repo files.

## Docs

- Docs live in `docs/zh` and `docs/en`; both must be updated together;
- Follow `docs/_meta/style-guide.md`;
- Run `tools/docs_check.ps1` before committing.

## Related

- [Testing](./testing.md)
- [Release](./release.md)
