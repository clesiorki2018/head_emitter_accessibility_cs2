# Instrucoes para o Codex

Este repositorio contem o firmware ESP-IDF do emissor `head_emitter`, parte do
projeto `head_click`. O ESP32 le entradas capacitivas via MPR121 e envia comandos
HID ao receptor por ESP-NOW usando um envelope autenticado.

## Idioma e estilo

- Responda ao usuario em portugues do Brasil.
- Use termos tecnicos em ingles quando forem nomes de APIs, comandos, arquivos,
  bibliotecas, protocolos ou conceitos estabelecidos no codigo.
- Mantenha explicacoes objetivas, com tom colaborativo e natural.
- Ao alterar codigo, preserve o estilo existente do projeto e explique a
  validacao feita.
- Prefira mudancas pequenas e focadas. Evite refactors amplos quando a tarefa
  pedir apenas ajuste de comportamento.
- Nao reverta alteracoes do usuario sem pedido explicito.

## Comandos ESP-IDF

- Antes de usar `idf.py`, carregue o ambiente do ESP-IDF:

```sh
. /home/max/projetos/esp/esp-idf/export.sh
```

- Para builds ESP-IDF, use sempre o diretorio de build em RAM:

```sh
idf.py -B /mnt/rambuild/head_emitter build
```

- Quando precisar informar a porta serial nos comandos `idf.py`, use
  `/dev/ttyUSB0`. Exemplos:

```sh
idf.py -p /dev/ttyUSB0 -B /mnt/rambuild/head_emitter build
idf.py -p /dev/ttyUSB0 -B /mnt/rambuild/head_emitter flash
idf.py -p /dev/ttyUSB0 -B /mnt/rambuild/head_emitter monitor
```

- Para validar mudancas de firmware, rode o build completo em RAM sempre que
  possivel:

```sh
. /home/max/projetos/esp/esp-idf/export.sh
idf.py -B /mnt/rambuild/head_emitter build
```

## Configuracao privada e segredos

- A configuracao privada vem do `.env`, gerada para C por
  `tools/generate_private_config.py` via `components/system_config/CMakeLists.txt`.
- Nunca versione `.env` nem valores reais de chaves, MACs ou segredos.
- Use `.env.example` como referencia para variaveis esperadas.
- Variaveis importantes:
  - `HEAD_EMITTER_RECEIVER_MAC`: MAC Wi-Fi STA do receptor.
  - `ESP_NOW_WIFI_CHANNEL`: canal fixo do ESP-NOW.
  - `ESP_NOW_PMK_HEX`: PMK compartilhada, 16 bytes em hexadecimal.
  - `ESP_NOW_LMK_HEX`: LMK do emissor, 16 bytes em hexadecimal.
  - `APP_AUTH_KEY_HEX`: chave de autenticacao da aplicacao, 32 bytes em
    hexadecimal.
  - `APP_SEQUENCE_NAMESPACE` e `APP_SEQUENCE_KEY`: local do contador persistido
    em NVS.

## Arquitetura do projeto

- `main`: inicializa NVS e chama a aplicacao.
- `components/app`: orquestra inicializacao, polling do MPR121, mapeamento de
  entradas, acessibilidade e envio dos comandos.
- `components/mpr121`: driver I2C reutilizavel para ler os 12 eletrodos do
  MPR121.
- `components/input`: transforma estados capacitivos estaveis em acoes logicas.
- `components/input_protocol`: monta payloads binarios do protocolo do receptor.
- `components/security`: monta o envelope seguro e calcula HMAC-SHA256.
- `components/sequence_store`: persiste o contador de sequencia em NVS.
- `components/transport_espnow`: inicializa Wi-Fi STA, ESP-NOW, PMK/LMK e envia
  payloads ao receptor cadastrado.
- `components/system_config`: expoe configuracao privada gerada a partir do
  `.env`.
- `components/accessibility`: implementa gestos e funcionalidades de
  acessibilidade.

## Entrada capacitiva

- O MPR121 usa `I2C_NUM_0`, endereco `0x5A`, `SDA=GPIO21` e `SCL=GPIO22`.
- O driver le todos os 12 canais do MPR121.
- Todos os 12 canais usam thresholds capacitivos sensiveis:
  `touch=8`, `release=4`.
- Mapa atual dos eletrodos:
  - `E0` a `E5`: lidos pelo driver, sem acao configurada.
  - `E6`: desabilitado por falha de hardware.
  - `E7`: mouse esquerdo.
  - `E8`: desabilitado por falha de hardware.
  - `E9`: mouse direito em caminho prioritario.
  - `E10`: mouse centro.
  - `E11`: tecla `Q` e controle de acessibilidade.
- O polling roda a cada 20 ms.
- O mouse direito (`E9`) e enviado por caminho prioritario, sem debounce do
  mapper e antes do processamento de acessibilidade.
- Para os demais canais, o mapper exige leituras consecutivas iguais antes de
  emitir `PRESSED` ou `RELEASED`.
- Quando mais de um eletrodo com acao configurada e tocado, o mapper considera
  apenas o primeiro canal detectado e so aceita outro canal depois que todos
  forem liberados.

## Acessibilidade

- O controle de acessibilidade usa `E11`, o mesmo canal da tecla `Q`.
- Tres toques rapidos em `E11` alternam a funcionalidade 1.
- Segurar `E11` por cerca de 3 segundos alterna a funcionalidade 2.
- A funcionalidade 1 envia `Ctrl` pressionado por 0,7 s a cada novo toque no
  mouse direito (`E9`), mantendo o clique direito original.
- A funcionalidade 2 mantem `W` pressionado e alterna `A`/`D` periodicamente.
- Os tempos, canais e keycodes ficam em
  `components/accessibility/include/accessibility/accessibility_config.h`.
- Logs detalhados de acessibilidade usam `ESP_LOGD`.

## Protocolo e seguranca

- O primeiro byte dos payloads e sempre o opcode:
  - `0x01`: movimento de mouse.
  - `0x02`: botao de mouse.
  - `0x03`: tecla de teclado.
  - `0x04`: eixos de joystick.
  - `0x05`: botao de joystick.
- Inteiros de 16 bits usam little-endian.
- Comandos sao enviados dentro de envelope seguro com `magic=0xa5`,
  `version=0x01`, `sequence`, payload e tag HMAC-SHA256 truncada para 16 bytes.
- O contador `sequence` e persistido em NVS. Se a NVS falhar, o firmware nao
  deve enviar comandos HID.

## Cuidados ao alterar codigo

- Preserve os limites entre componentes ESP-IDF e atualize `CMakeLists.txt`
  apenas quando uma dependencia real mudar.
- Ao alterar comandos HID, confira `components/input_protocol` e os tamanhos dos
  payloads.
- Ao alterar canais, thresholds ou debounce, confira tambem `README.md` e este
  `AGENTS.md`.
- Ao alterar acessibilidade, garanta que `accessibility_release_all()` solte
  qualquer tecla que possa ter ficado pressionada.
- Ao alterar ESP-NOW ou seguranca, nao exponha chaves reais em logs, docs ou
  commits.
- Prefira `ESP_LOGD` para logs ruidosos de ciclo rapido e `ESP_LOGI/W/E` para
  eventos operacionais relevantes.

## Git

- Antes de commitar, confira `git status --short` e revise o diff.
- Commits devem ser pequenos, com mensagem em portugues clara e no imperativo ou
  descritiva curta.
- Nao inclua artefatos de build nem arquivos privados.
