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

## Arquitetura

- `components/system_config`: expõe a configuração privada gerada a partir do `.env`.
- `components/mpr121`: driver I2C reutilizável para ler os 12 eletrodos do MPR121.
- `components/input`: mapeia estados capacitivos estáveis para ações lógicas.
- `components/input_protocol`: monta payloads puros e validados para o protocolo do receptor.
- `components/transport_espnow`: inicializa Wi-Fi STA, ESP-NOW, PMK/LMK e envia payloads ao receptor cadastrado.
- `components/app`: orquestra inicialização, polling do MPR121 e envio das ações mapeadas.
- `main`: inicializa NVS e chama a aplicação.

## Entrada Capacitiva

O emissor usa um MPR121 no barramento I2C com `SDA=21`, `SCL=22` e endereço `0x5A`. O driver lê todos os 12 canais, mas o mapeamento inicial usa apenas os canais 0 a 4:

| Canal | Ação |
| --- | --- |
| 0 | Mouse esquerdo |
| 1 | Mouse direito |
| 2 | Mouse centro |
| 3 | Tecla `W` |
| 4 | Tecla `Q` |

O polling roda a cada 20 ms e o mapper exige leituras consecutivas iguais antes de emitir `PRESSED` ou `RELEASED`. Eventos não são repetidos enquanto o eletrodo permanece no mesmo estado.

## Setup

Crie o `.env` local a partir de `.env.example` e copie as chaves do `sender_config.env` do receptor. O `.env` não deve ser versionado.

Variáveis principais:

- `HEAD_CLICK_RECEIVER_WIFI_STA_MAC`: MAC Wi-Fi STA do receptor.
- `HEAD_CLICK_ESP_NOW_WIFI_CHANNEL`: canal fixo ESP-NOW, atualmente `6`.
- `HEAD_CLICK_ESP_NOW_PMK_HEX`: PMK compartilhada, 16 bytes em hexadecimal.
- `HEAD_CLICK_SENDER_COMBO_LMK_HEX`: LMK do emissor combo, 16 bytes em hexadecimal.
- `HEAD_CLICK_APP_AUTH_KEY_HEX`: chave futura de autenticação da aplicação, 32 bytes em hexadecimal.

## Build

Sempre use build em RAM:

```sh
. /home/max/projetos/esp/esp-idf/export.sh
idf.py -B /mnt/rambuild/head_emitter build
```

Para gravar, configure `ESPPORT` no `.env` ou exporte a porta antes de rodar `idf.py flash`.
