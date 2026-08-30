---
status: active
owner: orchestrator
updated: 2026-08-30
tags: [agents, governance, moc]
---

# Agentic geliştirme sistemi

Kanonik kurallar depo kökündeki `AGENTS.md` dosyasındadır. Araçlara özel dosyalar aynı sözleşmeyi genişletmeden uygular.

```text
Kullanıcı
   `-> Orkestratör: kapsam, sahiplik, karar, birleşim, rapor
        |-> explorer/researcher (tercihen read-only)
        |-> hardware/power worker
        |-> firmware worker
        |-> acoustics worker
        `-> QA reviewer (bağımsız doğrulama)
```

## Zorunlu akış

1. Orkestratör hedefi ve başarı ölçütünü netleştirir.
2. Bağımsız işi alt çalışana sınırlandırılmış kapsam ve tek dosya sahipliğiyle verir.
3. Çalışan mevcut kararları okur; mimariyi sessizce değiştirmez.
4. Sonuç değişiklik + test kanıtı + risk özetiyle döner.
5. Orkestratör ADR/günlüğü günceller ve final kontrolünü yapar.

## Kurallar

- Aynı dosyada eşzamanlı yazma yok.
- Araştırma/test/inceleme paralel; geniş yazma kontrollüdür.
- Donanım güvenliği veya ürün davranışı ADR olmadan değişmez.
- Agent fiziksel testi geçmiş sayamaz; operatör sonucu kaydeder.

Yerleşim: `.agents/skills/`, `.codex/agents/`, `.claude/agents/`, `.cursor/agents/`, `.cursor/rules/`, `docs/07-decisions/`, `docs/08-development-log/`, `docs/09-handoffs/`.

Araçların hangi dosyaları resmen okuduğu için [[tooling-compatibility|araç uyumluluk notuna]] bakın.
