---
status: active
owner: orchestrator
updated: 2026-08-30
tags: [agents, codex, claude, cursor]
---

# Agent araç uyumluluğu

## Kanonik katman

Kökteki `AGENTS.md` ortak proje sözleşmesidir. Araç dosyaları kuralları çoğaltmak yerine buna bağlanır.

## Codex

- Proje talimatı: `AGENTS.md`.
- Proje ayarı: `.codex/config.toml` (yalnız güvenilen projede yüklenir).
- Özel çalışanlar: `.codex/agents/*.toml`.
- Repo skill'leri: `.agents/skills/*/SKILL.md`.
- Kaynaklar: [AGENTS.md](https://learn.chatgpt.com/docs/agent-configuration/agents-md), [config](https://learn.chatgpt.com/docs/config-file/config-basic), [subagents](https://learn.chatgpt.com/docs/agent-configuration/subagents), [skills](https://learn.chatgpt.com/docs/build-skills).

## Claude Code

- Proje talimatı: kökte `CLAUDE.md`; kanonik sözleşmeyi `@AGENTS.md` ile içe alır.
- Paylaşılan ayar: `.claude/settings.json`; kişisel `settings.local.json` Git dışında kalır.
- Proje çalışanları: `.claude/agents/*.md`.
- Kaynaklar: [Claude directory](https://code.claude.com/docs/en/claude-directory), [configuration](https://code.claude.com/docs/en/configuration), [subagents](https://code.claude.com/docs/en/sub-agents).

## Cursor

- Ortak talimat: `AGENTS.md`.
- Kurallar: `.cursor/rules/*.mdc`.
- Proje çalışanları: `.cursor/agents/*.md`.
- Kaynaklar: [rules](https://cursor.com/docs/rules), [subagents](https://cursor.com/docs/subagents).

Tanınmayan `.claude/teams/teams.json`, `.cursor/config.json`, `.cursor/settings.json` ve eski `.cursorrules` oluşturulmaz. Repo ayarlarında izin kontrolünü tamamen atlayan bir mod zorlanmaz.
