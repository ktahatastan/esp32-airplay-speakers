---
name: merzarkabul-orchestrate
description: Orchestrate complex Merzarkabul Airplay Speakers project work across hardware, firmware, acoustics, procurement and documentation. Use when a task spans domains, requests agents or parallel work, changes architecture, or needs a milestone/gate plan. Do not use for a single trivial edit.
---

# Merzarkabul Airplay Speakers orchestration

1. Read `AGENTS.md`, `docs/Home.md`, current requirements and relevant ADRs.
2. State the outcome and measurable completion criteria.
3. Split only independent work. Prefer subagents for read-heavy research, exploration, test or review.
4. Give each writer a disjoint file set and identify read-only areas.
5. Keep architecture, safety decisions and integration with the primary orchestrator.
6. Wait for workers, inspect their evidence and reconcile conflicts.
7. Update the ADR when a decision changed, the development log for material work, and test records for verification.
8. Report completed work, PASS/FAIL/BLOCKED evidence, remaining physical tests and next gate.

Never let a worker silently decide driver limits, power topology, protocol support or product identity.
