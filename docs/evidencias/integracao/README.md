# Evidências dos Testes de Integração

Esta pasta armazena as evidências geradas durante a execução dos testes de integração (`5.1` a `5.22`).

## Organização por teste

Crie uma subpasta para o ID correspondente ao seu teste em `docs/evidencias/integracao/<ID>/` (por exemplo, `docs/evidencias/integracao/5.5/` ou `docs/evidencias/integracao/5.20/`).

```text
docs/evidencias/integracao/
├── README.md
├── 5.5/
│   ├── 5.5-medicao-largura.jpeg
│   └── 5.5-giro-celula.jpeg
└── 5.20/
    ├── 5.20-latencia.csv
    └── 5.20-dashboard-tempo-real.png
```

## Convenções e limites

- **Nomenclatura:** inicie o nome de cada arquivo pelo ID do teste seguido de uma descrição curta sem espaços nem acentos (ex.: `5.5-medicao-largura.jpeg`, `5.13-log-pid.txt`, `5.20-latencia.csv`).
- **Tamanho máximo:** não suba arquivos maiores que **5 MB** no repositório (conforme o [README principal](../../../README.md)).
- **Vídeos:** suba vídeos no Google Drive ou YouTube e adicione apenas o **link** no arquivo `docs/integracao/5.X-*.md` correspondente.
- **Referência nos relatórios:** após salvar os arquivos aqui, adicione os links relativos (`../evidencias/integracao/5.X/<arquivo>`) na seção `## 5. Evidências` do respectivo `docs/integracao/5.X-*.md`.
