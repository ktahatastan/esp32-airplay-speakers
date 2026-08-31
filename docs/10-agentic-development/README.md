---
status: active
owner: orchestrator
updated: 2026-08-31
tags: [agents, orchestration, process]
---

# Agentic geliştirme sistemi

Kanonik kurallar depo kökündeki `AGENTS.md` dosyasındadır. Araçlara özel dosyalar aynı sözleşmeyi genişletmeden uygular.

## Rol haritası

```text
Kullanıcı
   `-> Orkestratör: kapsam, sahiplik, karar, birleşim, rapor
        |-> explorer            (read-only)  mevcut durumu haritalar
        |-> hardware-worker     (write)      güç, batarya, devre, şema, BOM
        |-> firmware-worker     (write)      ESP32-S3 firmware
        |-> acoustics-worker    (write)      sürücü ölçümü, crossover, limiter, DSP
        |-> hardware-reviewer   (read-only)  G0-G5 güvenlik incelemesi
        `-> verifier            (read-only)  bağımsız kabul doğrulaması
```

Altı rolün tamamı üç araçta da tanımlıdır ve dosya adları eşleşir:

| Rol | Yazma | Claude | Codex | Cursor |
|---|---|---|---|---|
| explorer | hayır | `.claude/agents/explorer.md` | `.codex/agents/explorer.toml` | `.cursor/agents/explorer.md` |
| hardware-worker | evet | `.claude/agents/hardware-worker.md` | `.codex/agents/hardware-worker.toml` | `.cursor/agents/hardware-worker.md` |
| firmware-worker | evet | `.claude/agents/firmware-worker.md` | `.codex/agents/firmware-worker.toml` | `.cursor/agents/firmware-worker.md` |
| acoustics-worker | evet | `.claude/agents/acoustics-worker.md` | `.codex/agents/acoustics-worker.toml` | `.cursor/agents/acoustics-worker.md` |
| hardware-reviewer | hayır | `.claude/agents/hardware-reviewer.md` | `.codex/agents/hardware-reviewer.toml` | `.cursor/agents/hardware-reviewer.md` |
| verifier | hayır | `.claude/agents/verifier.md` | `.codex/agents/verifier.toml` | `.cursor/agents/verifier.md` |

Yeni bir rol eklenirse üç araçta birden eklenir. Tek araçta yaşayan rol, diğer araçla çalışan kullanıcıyı yanıltır.

## Zorunlu akış

1. Orkestratör hedefi ve ölçülebilir başarı ölçütünü netleştirir.
2. Bağımsız işi alt çalışana sınırlandırılmış kapsam ve tek dosya sahipliğiyle verir.
3. Çalışan önce kabul edilmiş ADR'leri okur; mimariyi sessizce değiştirmez.
4. Sonuç değişiklik + doğrulama kanıtı + risk özetiyle döner.
5. Orkestratör ADR/günlüğü günceller ve final kontrolünü yapar.

## Kurallar

- Aynı dosyada eşzamanlı yazma yok.
- Araştırma/test/inceleme paralel; geniş yazma kontrollüdür.
- Donanım güvenliği veya ürün davranışı ADR olmadan değişmez.
- Agent fiziksel testi geçmiş sayamaz; operatör sonucu kaydeder.
- Kilitli kararlar (`AGENTS.md` > Locked decisions) yalnız supersede eden ADR ile değişir.

## Birleşme öncesi doğrulama

```bash
python3 scripts/check_docs.py
```

Betik wikilink hedeflerini, `docs/` frontmatter alanlarını, ADR durum sözlüğünü, ADR indeksinin eksiksizliğini, agent/skill tanım şemasını ve kilitli kararlardan sapan terimleri denetler. Sıfır hata beklenir. Bu betik yalnız proje kaydının kendi içinde tutarlı olduğunu kanıtlar; hiçbir fiziksel kapının yerine geçmez.

Yerleşim: `.agents/skills/`, `.codex/agents/`, `.claude/agents/`, `.cursor/agents/`, `.cursor/rules/`, `scripts/`, `docs/07-decisions/`, `docs/08-development-log/`, `docs/09-handoffs/`.

Araçların hangi dosyaları resmen okuduğu için [[tooling-compatibility|araç uyumluluk notuna]] bakın.
