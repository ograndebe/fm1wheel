# Firmware clonewheel para o M-VAVE FM-1 baseado no setBfree

Análise dos repositórios e plano de execução. Data: 2026-10-23.

**Estado (2026-10-23):** Fase 0 e a Fase 1 completas no host (dry + scanner +
preamp + reverb + Leslie).
O repositório está montado (`fm1-base` e `setBfree` como submodules), o
`app/` compila e toca no simulador de host, e o tone generator, o scanner, o
preamp e o reverb foram portados com A/B contra o setBfree:

| Teste | Correlação | Null | Bandas |
|---|---:|---:|---|
| acorde (888000000) | 0,99995 | -40 dB | ±0,0 dB |
| percussão (2º/3º) | 0,9996 | -31 dB | ±0,0 dB |
| drawbars (mudanças) | 0,9979 | -24 dB | graves/médios ±0,3 dB; agudos -10 dB no transiente |
| scanner V1→C1→V3 | 0,9815 | -14,3 dB | ±0,5 dB até 1,6 kHz; agudos -3,7/+2,9 dB |
| preamp (drive 0→60→127) | 0,9999 | -37,8 dB | ±0,0 dB |
| reverb (demo, mix 40) | 0,9983 | -24,6 dB | ±0,0 dB |
| Leslie (stop→slow→fast) | 0,9995 | -30,4 dB | ±0,0 dB |
| cadeia completa (demo) | 0,9991 | -27,6 dB | ±0,0 dB |

O **emulador web** compila com Emscripten e roda em `app/build/emu/`
(`make serve-web` → http://localhost:8790/), com a mesma cadeia e a mesma UI do
device. O build de device também roda nesta máquina via **Colima + Rosetta**
(`colima start --vz-rosetta`). Pacote gerado: `app/build/fm1-wheel.fwsc`
(611.354 B), app 315.520 B, `.data+.bss` 77.300 B de 98.304, pool 180.844 B de
344.064, sem soft-double. Falta apenas conectar o FM-1 e gravar/medir; até lá, o emulador web é o
ambiente de trabalho interativo.

---

## 1. Resumo e recomendação

**Recomendação:** criar um firmware novo em cima do **fm1-base** (que já usa o **felucca-bsp**) e portar o núcleo DSP do **setBfree** para ele. Não forkear a Felucca inteira.

- **Base de plataforma:** `charlesvestal/fm1-base` (MPL-2.0) — tela, teclado, LEDs, áudio I2S, USB-MIDI, TRS-MIDI, storage, instalador, **simulador de host** e **emulador no navegador**.
- **Base de DSP:** `pantherb/setBfree` (GPL-2.0-or-later) — tone generator de 91 rodas, scanner vibrato/chorus, percussão, key click, key compression, preamp/overdrive, reverb e Leslie.
- **Licença do app:** **GPL-3.0-or-later**. O setBfree é GPL-2.0-or-later; o fm1-base e o BSP são MPL-2.0; o SDK da JieLi é Apache-2.0. A combinação limpa é GPLv3.
- **Produto:** um **módulo de órgão tonewheel** com teclado de 27 teclas embutido. O uso "de verdade" é com um controlador MIDI externo de 61 teclas/drawbars via USB ou TRS. Isso contorna a maior limitação física do FM-1.
- **Prazo realista:** 9 a 14 semanas de trabalho focado para um MVP sólido; 4 a 6 meses em ritmo parcial.

O que **não** recomendo:

- Tentar rodar o setBfree como está: as wavetables dele ocupam **604 KB** em float e o Leslie ocupa **929 KB**. Não cabe nos 336 KB de pool do FM-1.
- Reescrever tudo em ponto fixo: o FM-1 tem **FPU de precisão simples** (`-mfprev1`). O ganho não compensa o risco de perder o timbre.
- Forkear a Felucca só para herdar a UI: a arquitetura dela é por voz/parte (8 vozes × 4 partes), e o setBfree é global (91 rodas compartilhadas). A briga arquitetural custa mais do que escrever a UI de um órgão do zero.
- Prometer um clonewheel completo de 61 teclas: o FM-1 tem **27 teclas** (F3–G5), 7 encoders e 14 botões. O caminho honesto é "módulo de som + teclado de conveniência".

---

## 2. O que cada repositório é (e o que não é)

| Repositório | O que é | Licença | Papel no projeto |
|---|---|---|---|
| `pantherb/setBfree` | DSP de órgão tonewheel em C float, com app JACK, plugin LV2 e GUI OpenGL | GPL-2.0-or-later | **Fonte do algoritmo e do timbre.** Portar só o DSP. |
| `hugelton/Felucca` | Firmware completo para FM-1 (multi-engine, sequencer, web editor) | GPL-3.0-only | **Referência de UX/áudio/latência.** Não é um kit; é um produto. |
| `hugelton/felucca-bsp` | Camada de hardware do FM-1 (clock, IRQ, flash, USB, UART, loader, packaging) | MPL-2.0 | Já vem dentro do fm1-base. Não precisa mexer. |
| `charlesvestal/fm1-base` | Base de firmware com `plat.h`, template, simulador de host, emulador WASM, instalador e site | MPL-2.0 | **Base do projeto.** |

O "kit para criar firmwares customizados" que você suspeitava existir é a combinação **felucca-bsp + fm1-base**. A Felucca é o primeiro produto grande em cima deles, não o kit.

### 2.1 Por que o clonewheel existente (WHEEL da Felucca) não soa como o setBfree

A engine `WHEEL` da Felucca é uma aproximação leve:

- 9 parciais senoidais **por voz**, 8 vozes por parte;
- 16 registrações fixas + 4 macros;
- percussão, click, drive e um "rotary" barato (AM + Doppler sem delay lines);
- ponto fixo, pensada para custar pouco.

O setBfree modela o instrumento:

- **91 rodas livres** rodando continuamente, cada uma com sua frequência mecânica (gear ratios), não uma senoide por nota;
- **matriz de contatos** de 3 manuais × 61 teclas × 9 buses, com **taper** e **crosstalk** de terminal, compartimento e fiação;
- **key compression** (o volume cai conforme mais teclas são pressionadas);
- **percussão** com trigger bus mutando o drawbar de 1';
- **scanner vibrato/chorus** de verdade (delay line modulada);
- **Leslie** com horn/drum, Doppler, reflexões, filtros e aceleração;
- **preamp/overdrive** valvulado.

É outra categoria de modelo. O custo computacional é surpreendentemente baixo porque as 91 rodas são **compartilhadas** entre as teclas: não há polifonia por voz.

---

## 3. Viabilidade no hardware do FM-1

### 3.1 O hardware

| Item | Valor | Observação |
|---|---|---|
| SoC | JieLi AC79, CPU `pi32v2` | 320 MHz no SDK; 360 MHz no boot loader |
| FPU | precisão simples (`-mfprev1`), sem FMA | `-ffp-contract=off`; nada de `double` no runtime |
| RAM | 96 KB (`.data`/`.bss`) + 336 KB (pool) | mapa do fm1-base |
| Flash | ~581 KB de app slot + área de dados | XIP; código roda do flash |
| Áudio | I2S, 44,1 kHz, stereo, 24-bit | half buffer de 128 ou 256 frames |
| Latência | 128 frames = 2,9 ms por half | com `APP_BLOCK 128` |
| Tela | LCD 240×240 (ST7789) | canvas útil de 240×144 no fm1-base |
| Teclado | 27 teclas, F3–G5 | com velocity/pressure opcionais |
| Controles | 7 encoders + 14 botões + pot MASTER | labels físicos: FX, SEL, ENV, LFO, EDIT, GLO, HOME, SAVE, ARP, SEQ, PLAY, REC, OCT-, OCT+ |
| MIDI | USB device (in/out) + TRS **in** | **não há MIDI DIN out** |
| USB | MIDI + áudio (in/out) + serial | ótimo para gravar e comparar |

### 3.2 O que já validei nesta análise (spikes no host)

Compilei o `tonegen.c`, `vibrato.c`, `whirl.c`, `overdrive.c` e `reverb.c` do setBfree **sem JACK/LV2**, num harness mínimo no macOS, e renderizei WAVs. Resultados:

| Medição | Resultado |
|---|---|
| Tonegen, 1 nota (888000000) | 18 rodas ativas |
| Tonegen, 3 notas | 54 rodas ativas |
| Tonegen, 9 notas | 91 rodas (satura) |
| Tonegen, 91 rodas | ~740× tempo real no Mac (host; não é o FM-1) |
| Wavetables do setBfree @44,1 kHz | **604 KB em float** (91 rodas) |
| `keyContrib` (3 manuais, 154 teclas) | **20.488 entradas**, máx. 194/tecla, ~120 KB a 6 B/entrada |
| `struct b_whirl` original | **929 KB** (tabelas de 16.384 posições) |
| `b_whirl` com 1.024 posições | **89 KB**; diferença de **-50,2 dB** vs. original num acorde com Leslie fast |
| Reverb | ~79 KB de delay lines; ~867× tempo real no Mac |
| Preamp/overdrive | poucos KB; ~1.424× tempo real no Mac |

Conclusões:

1. **O tonegen é viável.** Mesmo com 91 rodas, o custo por amostra é dominado por 3 MACs + leitura de tabela. A FPU dá conta; o gargalo real é memória, não CPU.
2. **As wavetables de 604 KB não cabem.** A solução é usar **acumulador de fase + tabela senoidal compartilhada**. Isso é fiel ao setBfree porque, na configuração default, **as rodas são senoides puras** (`wheel_Harmonics[0] = 1.0`, o resto 0; os harmônicos no `default.cfg` estão comentados). O timbre vem do routing/drawbars/crosstalk/percussão/Leslie, não de harmônicos por roda.
3. **O Leslie é o maior consumo de RAM.** Reduzir `WHIRL_DISPLC_SIZE` de 16.384 para 1.024 derruba de 929 KB para 89 KB com diferença de -50 dB. É o ponto doce para o pool do FM-1.
4. **O reverb custa 79 KB** e é opcional. Cabe, mas é o primeiro candidato a cortar se faltar pool.
5. **A matriz de contatos pode ir para o flash** (120 KB) ou ser regenerada de forma compacta a partir de 9 entradas de taper por tecla (~6 KB). Para o MVP, flash é mais simples.

### 3.3 Orçamento de memória proposto

| Bloco | Pool | Flash |
|---|---:|---:|
| Canvas da UI (240×144×2 B, o pool do template) | 68 KB | — |
| Leslie (1.024 posições) | 89 KB | ~40 KB de código |
| Reverb | 77 KB | ~5 KB |
| Tonegen (fases, ganhos, AOT, buffers) | ~20 KB | ~60 KB |
| Preamp/overdrive | ~3 KB | ~10 KB |
| Buffers de áudio/UI | ~5 KB | — |
| Tabelas de rodas/contatos | — | ~120 KB (ou ~6 KB compacto) |
| Plataforma fm1-base (fora o canvas) | ~1 KB | ~77 KB |
| UI/app própria | ~10 KB | ~40 KB |
| **Total aproximado** | **~273 KB de 336 KB** | **~350 KB de 581 KB** |

Sobra pouco pool, mas sobra. Sem Bluetooth/WiFi no v1. Se precisar de BLE (+24 KB pool, +45 KB RAM, +124 KB flash), primeiro corte o reverb ou compacte a matriz.

### 3.4 Orçamento de CPU (estimativa para 320 MHz)

| Bloco | Custo estimado | % de 320 MHz |
|---|---:|---:|
| Tonegen, 91 rodas | 40–100 M ciclos/s | 12–31% |
| Scanner vibrato | ~1 M | <1% |
| Preamp | 10–30 M | 3–9% |
| Reverb | ~1–2 M | <1% |
| Leslie | 5–15 M | 2–5% |
| **Áudio total** | **~60–150 M** | **~19–47%** |
| UI/main loop | à parte | ~10–20% |

Margem razoável, mas a faixa é larga. **A primeira coisa a medir no device é o pior caso** (9 notas, 888888888, Leslie fast, drive + reverb, 91 rodas). Se passar de 60% médio, corte crosstalk, reduza o Leslie para 512 posições ou offload para o segundo core.

---

## 4. Arquitetura proposta

```
                 USB-MIDI / TRS-MIDI          27 teclas + encoders/botões
                        │                              │
                        ▼                              ▼
                 fila de eventos (lock-free)     UI (app_frame, 60 Hz)
                        │                              │
                        ▼                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│ app_render (ISR de áudio, blocos de 128 frames)                     │
│                                                                     │
│  tonegen (91 rodas, 27 buses, taper/crosstalk)                      │
│      │  swell + vibrato + percussion (mono)                         │
│      ▼                                                              │
│  scanner vibrato/chorus                                             │
│      ▼                                                              │
│  preamp / overdrive (mono)                                          │
│      ▼                                                              │
│  reverb (mono)                                                      │
│      ▼                                                              │
│  Leslie (mono → stereo: horn L/R + drum L/R)                        │
│      ▼                                                              │
│  limiter/soft-clip + DC blocker + MASTER                            │
│      ▼                                                              │
│  int32 24-bit → I2S / USB audio                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 4.1 Tonegen: o coração

Portar de `setBfree/src/tonegen.c`:

- `aot[91]` (active oscillator table) e as listas de contribuição;
- `oscKeyOn` / `oscKeyOff` com a fila de mensagens;
- `keyContrib` (taper + crosstalk) — gerado no build ou compactado;
- `drawBarGain[27]`, `drawBarLevel[27][9]`;
- percussão (normal/soft, fast/slow, 2º/3º harmônico, trigger bus);
- key click (4 modelos de envelope, attack/release);
- key compression;
- scanner vibrato/chorus (V1/V2/V3/C1/C2/C3).

**Mudança principal:** trocar as wavetables por **91 acumuladores de fase** e uma tabela senoidal compartilhada (ex.: 2.048 pontos, float ou int16 com interpolação linear).

- Frequências: calcular as gear ratios do setBfree no build e gerar incrementos de fase Q32.
- Ganho por roda: `attenuation` do EQ do setBfree, gerado no build.
- Fase inicial: 0 para todas (como no setBfree).
- Dither: o setBfree adiciona 1 LSB de ruído nas wavetables; irrelevante. Se quiser, adicione um LFSR minúsculo.

Isso preserva o **batimento** entre rodas (a parte que faz o órgão "vivo") e elimina 604 KB.

### 4.2 Matriz de contatos

Duas opções:

1. **MVP — tabela completa no flash:** gerar `key_contrib[]` com o próprio código do setBfree num host tool e emitir um `.h` com arrays `const` (120 KB). Leitura só no key-on; não pesa no áudio.
2. **Otimização — compacta:** guardar por tecla as 9 contribuições diretas (terminal + nível) e o `terminalMix[91]`; reconstruir o crosstalk de fiação no key-on com a mesma fórmula (`defaultWiringCrosstalk * level / |Δbus|`). Cai para ~6 KB e é fiel.

Comece pela 1; migre para a 2 se o flash apertar (BLE, presets, samples).

### 4.3 Leslie

Portar `b_whirl/whirl.c` com:

- `WHIRL_DISPLC_SIZE = 1024` (medido: -50,2 dB vs. original, 89 KB);
- buffers de delay de 2.048 amostras (32 KB);
- filtros IIR e reflexões como estão;
- `setRevSelect` (STOP/SLOW/FAST) com rampas de aceleração;
- preset de microfone único (horn/drum) para economizar.

Se o pool permitir, teste 2.048 posições (153 KB) e compare de ouvido. 1.024 já é suficiente para o FM-1.

### 4.4 Preamp, reverb e overdrive

- `b_overdrive/overdrive.c` + `filterTools.c`: portar como está (oversampling 4×, filtros de 33 taps). Barato.
- `b_reverb/reverb.c`: portar como está; 79 KB. Default 20–30% de mix.
- Sem cabinet IR (convolução) no v1.

### 4.5 Integração com o fm1-base

- Copiar `fm1-base/template/` para `app/`.
- `APP_BLOCK 128` (2,9 ms) para latência de teclado aceitável.
- `app_render`: drenar MIDI, ler teclas, rodar o motor em blocos de 128, aplicar MASTER, converter para int32 24-bit.
- `app_frame`: UI a ~60 Hz; LEDs; persistência.
- `app_tick` (1 kHz) opcional para o relógio da UI/sequencer; não para o áudio.
- Compartilhamento de estado: fila de eventos para drawbars/percussão/Leslie; snapshots atômicos de 32 bits. Nada de locks no ISR.
- `fm1.toml`: `o2 = ["dsp/*.c"]` para o DSP a -O2; `vendored = ["third_party/setbfree/"]` para o código do setBfree; `gen` para as tabelas.

### 4.6 Mapeamento de controles (proposta)

**Encoders**

| Encoder | HOME | DRAWBARS | PERC/VIB | FX |
|---|---|---|---|---|
| SELECT | pagina | cursor (barra 1–9) | — | — |
| ALGO | modelo (B3/C3/A100) | manual (U/L/P) | — | — |
| PRESET | registração | — | — | — |
| K1 | 16' / 5⅓' | valor da barra | percussão on/off | drive |
| K2 | 8' / 4' | — | decay (fast/slow) | reverb mix |
| K3 | 2⅔' / 2' | — | harmonic (2nd/3rd) | leslie speed |
| K4 | 1⅗' / 1⅓' / 1' | — | vibrato/chorus (V1..C3) | — |

**Botões**

| Botão | Função |
|---|---|
| FX | drive on/off |
| SEL | cursor / próxima página |
| ENV | percussão on/off |
| LFO | vibrato/chorus on/off |
| EDIT | menu avançado (tuning, MIDI, etc.) |
| GLO | brilho dos LEDs (plataforma) |
| HOME | tela principal |
| SAVE | salvar preset |
| ARP | manual U/L/P (rótulo físico difere; mostrar na tela) |
| SEQ | split on/off (rótulo difere) |
| PLAY | Leslie fast/slow (LED verde/vermelho) |
| REC | panic / MIDI learn |
| OCT- / OCT+ | transpose do teclado |

Os rótulos físicos não batem com um órgão. A tela deve mostrar a função real; um adesivo/overlay impresso ajuda. Para uso sério, o controlador externo resolve.

### 4.7 MIDI

Adotar o mapa do setBfree para compatibilidade com controladores existentes:

- Canais 1/2/3 = upper/lower/pedal.
- CC 1/11 = swell.
- CC 70–78 = drawbars 16', 5⅓', 8', 4', 2⅔', 2', 1⅗', 1⅓', 1' (invertidos: 0 = máximo).
- CC 64 = Leslie speed toggle.
- CC 80/81/82 = percussion enable/decay/harmonic.
- CC 83 = vibrato knob; CC 92 = vibrato routing.
- CC 93 = overdrive character.
- Program change = registração/preset.
- USB-MIDI out para DAW/editor; TRS é só entrada.

Isso transforma o FM-1 num módulo compatível com drawbar controllers (Ocean Beach, Crumar, Viscount etc.) e DAWs.

### 4.8 Presets e storage

- 16–32 registrações clássicas de fábrica (888000000, 888800000, 888000008, 800000888 etc.).
- Presets de usuário em flash (usar o store do fm1-base; `APP_STORE_MAP`).
- Salvar: drawbars U/L/P, percussão, vibrato, drive, reverb, Leslie, tuning, configurações MIDI.
- Backup/restore via USB: o fm1-base já tem `backup = true` (opcional).

---

## 5. Licenciamento

| Componente | Licença | Obrigação |
|---|---|---|
| setBfree (DSP portado) | GPL-2.0-or-later | manter headers; disponibilizar fonte das modificações |
| fm1-base | MPL-2.0 | modificações nos arquivos dele ficam MPL-2.0; o app pode ter outra licença |
| felucca-bsp | MPL-2.0 | idem |
| SDK JieLi (boot/cfg) | Apache-2.0 | compatível com GPLv3; manter avisos |
| **App/firmware combinado** | **GPL-3.0-or-later** | publicar o código e o binário; sem restrições extras |

Cuidados:

- **Não copie código da Felucca** (GPL-3.0-only) para o app se quiser manter a opção "or later"; use só como referência. Se copiar, o combinado fica GPL-3.0-only (ainda compatível com o setBfree "or later", mas menos flexível).
- **Não use as texturas/fontes da GUI do setBfree**; elas têm licenças próprias.
- **Marcas:** evite "Hammond", "B3" e "Leslie" no nome do produto e no marketing. Descreva como "tonewheel organ" e "rotary speaker". No código, "leslie" como termo técnico é aceitável, mas o produto não deve se chamar assim.
- Se um dia habilitar Bluetooth/WiFi da JieLi (bibliotecas binárias Apache-2.0), revise a combinação com GPLv3 e o custo de RAM. No v1, não habilite.

---

## 6. Roadmap

Estimativas para uma pessoa focada. Some 30–50% se for em paralelo com outras tarefas.

### Fase 0 — Base e referência (3–5 dias)

- [x] Repositório criado; `fm1-base` (com o BSP) e `setBfree` como submodules fixados.
- [x] `app/` copiado do template; `APP_BLOCK 128`; `id_prefix FM-1_12`.
- [x] Build de host e `tests/basic.txt` passando.
- [x] `tools/setbfree-host/refrender`: cadeia completa do setBfree (tonegen → preamp → reverb → whirl) em WAV float.
- [x] `tools/setbfree-host/dump_tables`: `wheel_inc`, `wheel_att`, `key_contrib`, `key_comp`, envelopes.
- [ ] Instalar Docker Desktop, toolchain JieLi e SDK AC79.
- [x] Compilar o firmware no device via Colima/Rosetta (pacote `fm1-wheel.fwsc`).
- [ ] Gravar no FM-1 e testar boot, tela, teclas, áudio, USB-MIDI, TRS, update, recovery.

**Aceite:** host OK; falta o teste no device (Docker/toolchain/SDK).

### Fase 1 — Port do tonegen (2–3 semanas) — em andamento

- [x] Tone generator dry: rodas como acumulador de fase + tabela senoidal.
- [x] Drawbars, keyContrib, taper/crosstalk, key compression, click, percussão.
- [x] App de host tocando: 27 teclas, MIDI USB/TRS, drawbars, percussão, settings.
- [x] A/B contra o setBfree (`make compare`): acorde 0,99995 / -40 dB; percussão 0,9996 / -31 dB.
- [x] Portar `vibrato.c` (scanner V1–V3/C1–C3): 0,9815 / -14,3 dB.
- [x] Portar preamp/overdrive: 0,9999 / -37,8 dB; reverb: 0,9983 / -24,6 dB.
- [x] Portar o Leslie (whirl, 1.024 posições): 0,9995 / -30,4 dB.
- [ ] Medir CPU no device e ajustar o limiter.

**Aceite:** a cadeia completa bate com o setBfree no host; falta o teste no device.

### Fase 2 — Device: áudio e performance (1–2 semanas)

- Integrar no `app_render`; saída 24-bit; limiter/soft-clip; DC blocker.
- Portar preamp, reverb e Leslie (1.024 posições).
- Medir CPU no FM-1: ocioso, 1, 3, 9, 27 notas; 888888888; Leslie fast; drive+reverb.
- Medir latência de tecla e estabilidade (30 min sem dropout).
- Ajustar níveis e headroom.

**Aceite:** toca no FM-1; CPU média < 50% e pico < 80% no pior caso; latência < 10 ms.

### Fase 3 — UI, MIDI e persistência (2–3 semanas)

- Telas: HOME (drawbars), DRAWBARS, PERC/VIB, FX, SETTINGS, PRESETS.
- Mapeamento dos encoders/botões/LEDs.
- MIDI completo (USB + TRS, canais, CCs, program change).
- Salvar/carregar presets e settings; factory reset.
- Testes de UI no simulador de host e screenshots.

**Aceite:** dá para tocar e mudar registração sem computador; settings sobrevivem a reboot; MIDI externo controla tudo.

### Fase 4 — Paridade de som e polimento (2–3 semanas)

- Crosstalk completo (ou versão compacta), key compression, trigger bus.
- Calibrar Leslie (aceleração, brake, mic), drive, reverb.
- A/B contra setBfree e gravações de referência.
- Presets clássicos; usuário.
- Testes golden (hash de render), orçamento de CPU no CI.

**Aceite:** A/B subjetivo aprovado; nenhuma regressão de timbre; CPU estável.

### Fase 5 — Produto (1–2 semanas)

- Site + instalador (fm1-base), manual, guia de recuperação.
- Release 0.1; changelog; licenças e créditos.
- Opcional: editor web via USB SysEx, BLE MIDI, segundo core.

**Aceite:** instalador funciona em Chrome/Edge; manual cobre controles, MIDI e recovery.

---

## 7. Riscos e mitigação

| Risco | Prob. | Impacto | Mitigação |
|---|---|---|---|
| CPU/FPU pior que o estimado | média | alto | medir na Fase 2; reduzir crosstalk; Leslie 1.024; offload para CPU1 (experimental) |
| Pool estourar | média | alto | cortar reverb; compactar keyContrib; canvas menor; sem BLE |
| Leslie a 1.024 soar diferente | baixa | médio | teste A/B; 2.048 se o pool permitir; ajustar filtros |
| Latência de tecla alta | média | médio | `APP_BLOCK 128`; ler teclas no ISR; evitar filas longas |
| API do fm1-base mudar | alta | baixo | fixar commit; patches mínimos; vendorizar |
| Bug sutil no port do setBfree | alta | médio | port bloco a bloco; golden tests; A/B |
| Licença | baixa | alto | GPL-3.0-or-later; headers; fonte publicada; sem marcas |
| Brick na gravação | baixa | alto | testar recovery antes; manter firmware oficial; `fm1_rescue`/transporter |
| 27 teclas limitam o produto | certa | médio | posicionar como módulo; MIDI externo; octave shift |

---

## 8. MVP vs. depois

**MVP (v1):**

- Upper manual no teclado de 27 teclas, com OCT-/OCT+.
- 9 drawbars editáveis + 16 registrações.
- Percussão (on/off, 2nd/3rd, fast/slow, normal/soft).
- Vibrato/chorus V1–V3/C1–C3.
- Drive, reverb, Leslie slow/fast.
- MIDI USB + TRS (upper/lower/pedal por canal; mapa de CC do setBfree).
- Presets de usuário e settings em flash.
- Host simulator + testes golden.
- Sem BLE, sem WiFi, sem segundo core, sem editor web.

**Depois:**

- Lower/pedal editáveis na UI e split no teclado.
- Editor web (Web MIDI/SysEx).
- BLE MIDI (se o pool/flash permitirem).
- Segundo core para o Leslie ou o tonegen.
- Modelos de órgão (50 Hz, 82 rodas, 91FB00/12).
- Harmônicos por roda (wheels "sujas").
- Cabinet IR.
- Sequencer/arpejador (a Felucca já tem; não é o foco).

---

## 9. Próximos passos concretos (primeiras 2 semanas)

1. **Ambiente de device** (1–2 dias) — pendente
   ```sh
   # from the repository root; needs Docker Desktop running
   fm1-base/tools/get_toolchain.sh
   git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
       https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
   make device
   python3 fm1-base/tools/fm1_install.py app/build/fm1-wheel.fwsc
   ```
   Requer Docker Desktop (o toolchain é Linux x86-64; no Apple Silicon roda via Rosetta).
   Testar boot, tela, teclas, áudio, USB-MIDI, TRS, update e recovery.

2. **Repositório** (1 dia) — feito
   - `app/` copiado do template; `APP_BLOCK 128`; `id_prefix FM-1_12`.
   - `fm1.toml`: nome, USB names, licença GPL-3.0-or-later.
   - Submodules `fm1-base` (com BSP) e `setBfree` fixados.
   - `make build-host` e `make test-host` passando.

3. **Referência** (2–3 dias) — feito
   - `tools/setbfree-host/refrender` (tonegen → preamp → reverb → whirl).
   - `tools/setbfree-host/dump_tables` (`wheel_inc`, `wheel_att`, `key_contrib`, `key_comp`, envelopes).
   - `make ref` e `make tables`.

4. **Tonegen no host** (1 semana) — feito (dry)
   - `app/dsp/organ.c`: rodas de fase, drawbars, key matrix, percussão, click.
   - `app/app.c`: teclado, MIDI, drawbars, presets, settings; toca no simulador.
   - A/B: `make compare`.
   - Feitos o scanner, o preamp, o reverb e o Leslie; próximo: o device.

5. **Primeiro som no device** (2–3 dias)
   - Integrar no `app_render`; saída 24-bit; limiter. (já integrado)
   - Tocar um acorde no FM-1 e gravar via USB audio.
   - Medir CPU/latência.

## 10. Decisões que preciso de você

Defaults que adotei (diga se quer outro caminho):

1. **Base:** fm1-base + setBfree, app novo. (Alternativa: fork da Felucca.)
2. **Posicionamento:** módulo de órgão com teclado de 27 teclas; MIDI externo para a experiência completa.
3. **Licença:** GPL-3.0-or-later.
4. **Escopo v1:** upper manual, drawbars, percussão, vibrato/chorus, drive, reverb, Leslie; sem BLE/WiFi/segundo core/editor web.
5. **Leslie:** 1.024 posições (89 KB, -50 dB) — se você tiver ouvido exigente, testamos 2.048.
6. **Nome do produto:** **fm1-wheel**, em referência às tonewheels. O `id_prefix` do pacote continua `FM-1_12` para não invalidar instalações.

A Fase 0 está feita no host. O próximo passo é o ambiente de device (Docker + toolchain + SDK), gravar o template no FM-1 e então começar a Fase 1 (port do tonegen).

---

## Apêndice A — Comandos e arquivos de referência

- Toolchain/SDK: `fm1-base/tools/get_toolchain.sh`, `fm1-base/README.md` (Build).
- Build: `fm1-base/tools/build.py APP_DIR [--host|--web|--release X.Y]`.
- Instalação: `python3 fm1-base/tools/fm1_install.py build/NAME.fwsc`.
- Simulador: `APP_DIR/build/host/NAME_host APP_DIR/tests/basic.txt APP_DIR/build/host`.
- Emulador web: `fm1-base/tools/build.py APP_DIR --web`, servir `APP_DIR/build/emu/`.
- Mapa de memória: `fm1-base/device/app.ld`.
- HAL do FM-1: `felucca-bsp/chip/`, `felucca-bsp/boards/fm1/`.
- CPU/FPU: `felucca-bsp/docs/CPU.md`, `fm1-base/include/fastmath.h`.
- setBfree: `src/tonegen.c`, `src/vibrato.c`, `b_whirl/whirl.c`, `b_overdrive/overdrive.c`, `b_reverb/reverb.c`, `cfg/default.cfg`.
- Felucca (referência de UX): `firmware/src/audio.c`, `firmware/src/main.c`, `firmware/hal/fm1_input.h`.

## Apêndice B — Por que a tabela senoidal é fiel

No setBfree, `initOscillators` gera uma wavetable por roda a partir de `wheel_Harmonics[]`. O default é:

```c
t->wheel_Harmonics[0] = 1.0;   /* fundamental */
/* todos os demais = 0 */
```

E os harmônicos extras no `cfg/default.cfg` estão comentados. Ou seja: **cada roda é uma senoide pura**, com frequência mecânica (gear ratio) e atenuação (EQ). O "som de órgão" vem de:

- 9 drawbars por manual, cada um conectando a uma roda diferente (16', 5⅓', 8', 4', 2⅔', 2', 1⅗', 1⅓', 1');
- taper e crosstalk dos contatos;
- key click e key compression;
- percussão;
- scanner vibrato/chorus;
- preamp, reverb e Leslie.

Portanto, trocar as wavetables por `sin(2π · fase)` com a mesma frequência/atenuação preserva o modelo default. Se um dia quisermos rodas com harmônicos, basta gerar uma tabela senoidal por perfil (12 harmônicos) e compartilhar entre rodas do mesmo perfil — sem voltar aos 604 KB.

## Apêndice C — Reprodução dos spikes

O harness usado nesta análise:

- Compila `tonegen.c` + `vibrato.c` sem JACK/LV2, define `SampleRateD`, e chama `allocTonegen`, `initToneGenerator`, `oscKeyOn`, `oscGenerateFragment`.
- Compila `whirl.c` + `eqcomp.c` e chama `allocWhirl`, `initWhirl`, `whirlProc3`.
- Compila `overdrive.c` + `filterTools.c` e `reverb.c` para medir memória/CPU.
- Para o teste do Leslie, muda `WHIRL_DISPLC_SIZE` de `1 << 14` para `1 << 10` e compara os WAVs (diferença -50,2 dB).

Esses arquivos vivem em `tools/setbfree-host/` no repositório e são a base dos testes golden.
