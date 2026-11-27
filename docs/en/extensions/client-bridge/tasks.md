---
title: Bridge Task Chain
description: Launch injection point, NSumSyncTask chain, progress UI and cancellation.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/extensions/client-bridge/tasks.md
---

# Bridge Task Chain

## Injection point

bridge wraps HMCL's launch check via Mixin:

```text
LauncherHelper.checkGameState(HMCLGameRepository, GameSettings.Effective, Version)
```

It returns a new chain:

```text
NSumSyncTask("NSUM update check") → original checkGameState result
```

So the NSUM sync is the **first step** of launch and aborts on failure.

## Task chain

```mermaid
sequenceDiagram
    participant H as HMCL
    participant T as NSumSyncTask
    participant W as Branch selection
    participant R as Runtime
    H->>T: submit
    T->>T: Read instance marker
    T->>W: Select branches if needed
    W-->>T: gitBranch / branch
    T->>R: exec build --format hmcl --export <instance>
    R-->>T: Progress/exit code
    alt exit 0
        T-->>H: completed
    else failure
        T-->>H: failed (launch aborted)
    end
```

## Progress UI

- The sync task appears in the HMCL task dialog;
- The bridge panel mounts at the top of `task-executor-dialog-layout` (not a separate window);
- The panel caches the last status so pre-mount status is not lost;
- Cancel interrupts the Runtime process.

## Cancellation

- HMCL cancellation interrupts the task thread;
- bridge calls `destroyForcibly()` on the Runtime process after `Task.isCancelled()`;
- The game is not launched after cancellation.

## Failure handling

| Case | Behavior |
|---|---|
| Runtime non-zero exit | Task fails, launch aborts |
| Download/sync exception | Error summary and logs |
| `launchOnSyncFailure=true` | Launch with stale content |

## Notes

- A failed injection (Mixin version mismatch) leaves the NSUM page working but skips the launch check;
- The sync must not block the HMCL UI thread;
- Task failure must never be silent.

## Related

- [Usage](./usage.md)
- [Sync and launch](../../user/sync-and-launch.md)
