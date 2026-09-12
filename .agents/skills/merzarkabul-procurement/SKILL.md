---
name: merzarkabul-procurement
description: Research or update Merzarkabul Airplay Speakers parts, Turkish suppliers, BOM quantities and purchase readiness. Use for shopping lists, alternatives, price/stock refreshes or module compatibility. Do not approve a safety-critical part from retailer title or current alone.
---

# Merzarkabul Airplay Speakers procurement

1. Read `docs/05-procurement/bom.md`, `suppliers.md`, accepted ADRs and the relevant technical plan.
2. Browse current sources because price, stock and product revisions drift.
3. Use manufacturer datasheets for technical limits and retailers only for availability/price.
4. Record seller, exact URL, access date, quantity for the one cabinet (4 x XH-A232, 2 x MP1584-class buck, 1 x ESP32-S3 N16R8, 1 x PCM5102A, 1 x 24 V / 2.9 A adapter) and prototype/final distinction.
5. Mark each item `candidate`, `approved`, `ordered`, `received`, `tested` or `rejected`.
6. For the 24 V / 2.9 A DC adapter (no-load output below 25.5 V), the 5.5 x 2.1 mm centre-positive barrel jack (contact rating at 2.9 A continuous is a supplier question), the two bucks and the four amplifier modules, list pre-purchase verification questions and counterfeit/mislabel risk.
7. Never substitute adapter voltage or current rating, polarity, jack size or connector silently. Escalate the decision to an ADR when architecture changes.
8. Update the research log and preserve old decisions rather than overwriting history.
