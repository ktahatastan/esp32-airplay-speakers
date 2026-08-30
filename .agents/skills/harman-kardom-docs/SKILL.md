---
name: harman-kardom-docs
description: Maintain the Harman Kardom Obsidian vault, README, ADRs, development logs, handoffs, test records and internal links. Use when project knowledge, plans or agent records change. Do not rewrite accepted decisions without a superseding ADR.
---

# Harman Kardom documentation

1. Start at `docs/Home.md` and locate the canonical domain page.
2. Use frontmatter fields consistently: `status`, `owner`, `updated`, optional `reviewers`, `decision`, `tags`.
3. Keep MOC pages short and link to detailed notes with Obsidian wiki links.
4. Record architecture/safety/product changes as ADRs; preserve superseded history.
5. Record implementation facts and verification in a dated development log.
6. Record worker ownership transfer in a handoff and test results in a test report.
7. Separate confirmed facts, candidates and open questions. Add source URLs and access dates to procurement research.
8. Check internal links, JSON/TOML/YAML syntax and `git diff --check` before finishing.
