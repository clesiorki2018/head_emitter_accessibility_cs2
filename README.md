<!-- SPDX-License-Identifier: Apache-2.0 -->

# head_emitter

Firmware ESP-IDF para o emissor combo do projeto `head_click`. Este ESP32 envia comandos por ESP-NOW para o receptor HID e foi organizado para suportar mouse, teclado e joystick no mesmo dispositivo.

## Protocolo

O receptor espera payloads binários pequenos. O primeiro byte é sempre o opcode:

| Opcode | Evento | Payload |
| --- | --- | --- |
| `0x01` | Movimento de mouse | `opcode`, `dx_lo`, `dx_hi`, `dy_lo`, `dy_hi` |
| `0x02` | Botão de mouse | `opcode`, `button`, `pressed` |
| `0x03` | Tecla de teclado | `opcode`, `keycode`, `pressed` |
| `0x04` | Eixos de joystick | `opcode`, `x_lo`, `x_hi`, `y_lo`, `y_hi`, `z_lo`, `z_hi`, `rz_lo`, `rz_hi` |
| `0x05` | Botão de joystick | `opcode`, `button`, `pressed` |

Todos os inteiros de 16 bits usam little-endian. `pressed` vale `0` para solto e qualquer valor diferente de zero para pressionado.

Os comandos são enviados dentro de um envelope seguro da aplicação: `magic=0xa5`, `version=0x01`, `sequence` little-endian, `command_payload` e tag com os primeiros 16 bytes do HMAC-SHA256. O HMAC usa `APP_AUTH_KEY_HEX` e cobre todos os bytes antes da tag. O contador `sequence` é persistido em NVS; se a NVS falhar, o firmware não envia comandos HID.

## Arquitetura

- `components/system_config`: expõe a configuração privada gerada a partir do `.env`.
- `components/mpr121`: driver I2C reutilizável para ler os 12 eletrodos do MPR121.
- `components/input`: mapeia estados capacitivos estáveis para ações lógicas.
- `components/input_protocol`: monta payloads puros e validados para o protocolo do receptor.
- `components/security`: monta o envelope seguro e calcula HMAC-SHA256.
- `components/sequence_store`: persiste o contador de sequência em NVS.
- `components/transport_espnow`: inicializa Wi-Fi STA, ESP-NOW, PMK/LMK e envia payloads ao receptor cadastrado.
- `components/app`: orquestra inicialização, polling do MPR121 e envio das ações mapeadas.
- `main`: inicializa NVS e chama a aplicação.

## Entrada Capacitiva

O emissor usa um MPR121 no barramento `I2C_NUM_0` com endereço `0x5A`. O driver lê todos os 12 canais, mas o mapeamento inicial usa apenas os canais 0 a 4.

Mapa I2C/GPIO:

| Sinal | ESP32 | MPR121 | Observação |
| --- | --- | --- | --- |
| `I2C SDA` | `GPIO21` | `SDA` | Dados do barramento I2C |
| `I2C SCL` | `GPIO22` | `SCL` | Clock do barramento I2C |
| `3V3` | `3V3` | `VCC` | Alimentação do módulo |
| `GND` | `GND` | `GND` | Terra comum |
| `ADDR` | sem GPIO | `ADDR` | Aberto ou `GND` mantém endereço `0x5A` |
| `IRQ` | não conectado | `IRQ` | Reservado para interrupção futura |

Mapa dos eletrodos:

| MPR121 | Ação inicial |
| --- | --- |
| `E0` a `E5` | Lidos pelo driver, sem ação configurada |
| `E6` | Desabilitado por falha de hardware |
| `E7` | Mouse esquerdo |
| `E8` | Desabilitado por falha de hardware |
| `E9` | Mouse direito em caminho prioritário |
| `E10` | Mouse centro |
| `E11` | Tecla `Q` e controle de acessibilidade |

O polling roda a cada 20 ms. Todos os 12 canais do MPR121 usam thresholds capacitivos mais sensíveis (`touch=8`, `release=4`), inclusive eletrodos lidos pelo driver que ainda não possuem ação configurada. O mouse direito (`E9`) é enviado por um caminho prioritário, sem debounce do mapper e antes de qualquer processamento de acessibilidade. Para os demais canais, o mapper exige leituras consecutivas iguais antes de emitir `PRESSED` ou `RELEASED`. Eventos não são repetidos enquanto o eletrodo permanece no mesmo estado. Quando mais de um eletrodo com ação configurada é tocado, o mapper considera apenas o primeiro canal detectado e só aceita outro canal depois que todos forem liberados.

## Acessibilidade

O módulo de acessibilidade usa o eletrodo `E11`, o mesmo da tecla `Q`, como controle:

- 3 toques rápidos alternam a funcionalidade 1.
- Segurar por cerca de 3 segundos alterna a funcionalidade 2.

Após ligar, os gestos de acessibilidade são habilitados depois de um curto período com os canais mapeados em repouso. Essa trava evita que leituras instáveis do sensor no boot sejam interpretadas como gesto de acessibilidade.

Esses gestos acontecem junto da ação normal da tecla `Q`: cada toque no `E11` também envia `Q` normalmente. Internamente o long press dispara após 2,5 s e tolera quedas rápidas do toque por até 0,9 s para compensar oscilações do sensor.

Funcionalidade 1: quando estiver habilitada, cada novo toque no botão direito (`E9`) envia `Ctrl` pressionado por 0,7 s. O botão direito original continua sendo enviado normalmente. Novos toques em `E9` durante esse pulso não acumulam pulsos pendentes.

Funcionalidade 2: quando estiver habilitada, mantém a tecla `W` pressionada continuamente e alterna as teclas `A` e `D` a cada 0,6 s, mantendo cada tecla pressionada por 0,25 s. Ao desabilitar, solta `W` e qualquer tecla `A`/`D` que ainda esteja ativa.

Os logs detalhados da acessibilidade usam `ESP_LOGD`, para ficarem disponíveis em debug sem poluir o log normal.

Os tempos, canais e keycodes ficam em `components/accessibility/include/accessibility/accessibility_config.h`.

## Setup

Crie o `.env` local a partir de `.env.example` e copie as chaves do `sender_config.env` do receptor. O `.env` não deve ser versionado.

Variáveis principais:

- `HEAD_EMITTER_RECEIVER_MAC`: MAC Wi-Fi STA do receptor.
- `ESP_NOW_WIFI_CHANNEL`: canal fixo ESP-NOW, atualmente `6`.
- `ESP_NOW_PMK_HEX`: PMK compartilhada, 16 bytes em hexadecimal.
- `ESP_NOW_LMK_HEX`: LMK do emissor combo, 16 bytes em hexadecimal.
- `APP_AUTH_KEY_HEX`: chave de autenticação da aplicação, 32 bytes em hexadecimal.
- `APP_SEQUENCE_NAMESPACE` e `APP_SEQUENCE_KEY`: local do contador persistido em NVS.

## Build

Sempre use build em RAM:

```sh
. /home/max/projetos/esp/esp-idf/export.sh
idf.py -B /mnt/rambuild/head_emitter build
```

Para gravar, configure `ESPPORT` no `.env` ou exporte a porta antes de rodar `idf.py flash`.

## Licenca

Este projeto e distribuido sob a Apache License 2.0. Consulte o arquivo
`LICENSE` para os termos completos.
