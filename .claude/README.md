# Claude Code configuration

The supported project instruction entry point is root `CLAUDE.md`, which imports the canonical contract from `AGENTS.md`.

- Team-shared settings: `settings.json`. Personal overrides belong in `settings.local.json`, which stays out of Git.
- Project subagents: `agents/*.md`. Six roles mirror `.codex/agents/` and `.cursor/agents/`: `explorer`, `hardware-worker`, `firmware-worker`, `acoustics-worker`, `hardware-reviewer`, `verifier`.
- Repo skills live in `.agents/skills/` and are shared across tools.

`settings.json` allow-lists only the read-only verification commands, so routine checks do not prompt. Anything that writes history or regenerates a hardware artefact stays in `ask`.

There is deliberately no blanket `Read` allow rule. A filesystem-absolute pattern such as `Read(//Users/**)` would auto-approve every file in the user's home directory — SSH keys, cloud credentials, sibling repositories — while the `deny` rules below are `./`-relative and therefore only cover this project. Since this file is committed and shared, such a rule would apply on every collaborator's machine.

Do not create runtime team or task state files here; they are not part of the project record.
