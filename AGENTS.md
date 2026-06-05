# Instrucoes para o Codex

- Responda ao usuario em portugues do Brasil.
- Use termos tecnicos em ingles quando forem nomes de APIs, comandos, arquivos, bibliotecas ou conceitos estabelecidos no codigo.
- Mantenha explicacoes objetivas, com tom colaborativo e natural.
- Ao alterar codigo, preserve o estilo existente do projeto e explique a validacao feita.
- Para builds ESP-IDF, use sempre o diretorio de build em RAM: `idf.py -B /mnt/rambuild/head_emitter build`.
- Quando precisar informar a porta serial nos comandos `idf.py`, use `/dev/ttyUSB0`, por exemplo: `idf.py -p /dev/ttyUSB0 -B /mnt/rambuild/head_emitter build`.
