---
name: harman-kardom-procurement
description: Research or update Harman Kardom parts, Turkish suppliers, BOM quantities and purchase readiness. Use for shopping lists, alternatives, price/stock refreshes or module compatibility. Do not approve a safety-critical part from retailer title or current alone.
---

# Harman Kardom procurement

1. Read `docs/05-procurement/bom.md`, `suppliers.md`, accepted ADRs and the relevant technical plan.
2. Browse current sources because price, stock and product revisions drift.
3. Use manufacturer datasheets for technical limits and retailers only for availability/price.
4. Record seller, exact URL, access date, unit quantity, four-unit total and prototype/final distinction.
5. Mark each item `candidate`, `approved`, `ordered`, `received`, `tested` or `rejected`.
6. For BMS, charger, cells, buck, amplifier and switches, list pre-purchase verification questions and counterfeit/mislabel risk.
7. Never substitute voltage, chemistry, polarity, cell topology or connector silently. Escalate the decision to an ADR when architecture changes.
8. Update the research log and preserve old decisions rather than overwriting history.
