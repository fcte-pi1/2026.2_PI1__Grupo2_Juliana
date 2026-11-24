#!/usr/bin/env python3
"""
Consolida os documentos individuais de testes de integração (`docs/integracao/5.*.md`)
no arquivo oficial `docs/7.5 - Testes de integração.md`.

Uso:
  py scripts/consolidar_integracao.py
  py scripts/consolidar_integracao.py --check
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import difflib
from pathlib import Path
import re
import sys

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")

REPO_ROOT = Path(__file__).resolve().parent.parent
INTEGRACAO_DIR = REPO_ROOT / "docs" / "integracao"
OUTPUT_FILE = REPO_ROOT / "docs" / "7.5 - Testes de integração.md"

REQUIRED_FIELDS = (
    "id",
    "frente",
    "issue",
    "descricao",
    "data_inicio",
    "data_conclusao",
    "sub_equipes",
    "responsavel",
    "resultados_esperados",
    "resultados_obtidos",
    "aprovacao_interna",
    "aprovacao_orientador",
    "observacoes",
)

FRENTE_ORDER = (
    "Hardware, Estrutura e Energia (4.1.4)",
    "Firmware e Hardware (4.1.5)",
    "Firmware e Sistema Web (4.1.6)",
)

ID_PATTERN = re.compile(r"^5\.(\d+)$")
FRONTMATTER_KEY_PATTERN = re.compile(r"^([A-Za-z0-9_]+)\s*:\s*(.*)$")


@dataclass(frozen=True)
class TesteIntegracao:
    """Metadados normalizados de um documento individual de teste de integração."""

    filename: str
    numero: int
    id: str
    frente: str
    issue: str
    descricao: str
    data_inicio: str
    data_conclusao: str
    sub_equipes: str
    responsavel: str
    resultados_esperados: str
    resultados_obtidos: str
    aprovacao_interna: str
    aprovacao_orientador: str
    observacoes: str


def _unescape_double_quoted(raw: str) -> str:
    """Interpreta escapes comuns de strings YAML entre aspas duplas."""
    out: list[str] = []
    i = 0
    length = len(raw)
    escapes = {
        '"': '"',
        "\\": "\\",
        "n": "\n",
        "t": "\t",
        "r": "\r",
        "/": "/",
    }
    while i < length:
        ch = raw[i]
        if ch == "\\" and i + 1 < length:
            nxt = raw[i + 1]
            if nxt in escapes:
                out.append(escapes[nxt])
                i += 2
                continue
        out.append(ch)
        i += 1
    return "".join(out)


def _parse_scalar(raw_value: str) -> str:
    """Extrai um valor escalar simples de uma linha YAML."""
    value = raw_value.strip()
    if len(value) >= 2 and value[0] == '"' and value[-1] == '"':
        return _unescape_double_quoted(value[1:-1]).strip()
    if len(value) >= 2 and value[0] == "'" and value[-1] == "'":
        return value[1:-1].replace("''", "'").strip()
    return value


def parse_frontmatter(path: Path) -> dict[str, str]:
    """Lê e valida o bloco frontmatter YAML do topo de um arquivo Markdown."""
    content = path.read_text(encoding="utf-8")
    lines = content.splitlines()
    if not lines or lines[0].strip() != "---":
        raise ValueError(f"{path.name}: frontmatter YAML ausente (esperado '---' na linha 1).")

    closing_idx: int | None = None
    for idx in range(1, len(lines)):
        if lines[idx].strip() == "---":
            closing_idx = idx
            break

    if closing_idx is None:
        raise ValueError(f"{path.name}: delimitador de fechamento '---' do frontmatter não encontrado.")

    data: dict[str, str] = {}
    for line_no, line in enumerate(lines[1:closing_idx], start=2):
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        match = FRONTMATTER_KEY_PATTERN.match(line)
        if not match:
            raise ValueError(f"{path.name}:{line_no}: linha inválida no frontmatter: {line!r}")
        key, raw_val = match.group(1), match.group(2)
        if key in data:
            raise ValueError(f"{path.name}:{line_no}: chave duplicada no frontmatter: {key!r}")
        data[key] = _parse_scalar(raw_val)

    missing = [field for field in REQUIRED_FIELDS if field not in data]
    if missing:
        raise ValueError(f"{path.name}: campos obrigatórios ausentes no frontmatter: {', '.join(missing)}")

    return data


def load_testes(integracao_dir: Path) -> list[TesteIntegracao]:
    """Carrega, valida e ordena numericamente todos os arquivos `5.*.md`."""
    if not integracao_dir.is_dir():
        raise FileNotFoundError(f"Diretório não encontrado: {integracao_dir}")

    testes: list[TesteIntegracao] = []
    seen_ids: dict[str, str] = {}

    for path in sorted(integracao_dir.glob("5.*.md")):
        if path.name in {"README.md", "_template.md"}:
            continue

        meta = parse_frontmatter(path)
        test_id = meta["id"].strip()
        id_match = ID_PATTERN.match(test_id)
        if not id_match:
            raise ValueError(f"{path.name}: id inválido {test_id!r} (esperado formato '5.<numero>').")

        if test_id in seen_ids:
            raise ValueError(
                f"{path.name}: id duplicado {test_id!r} (já definido em {seen_ids[test_id]})."
            )
        seen_ids[test_id] = path.name

        testes.append(
            TesteIntegracao(
                filename=path.name,
                numero=int(id_match.group(1)),
                id=test_id,
                frente=meta["frente"],
                issue=meta["issue"],
                descricao=meta["descricao"],
                data_inicio=meta["data_inicio"],
                data_conclusao=meta["data_conclusao"],
                sub_equipes=meta["sub_equipes"],
                responsavel=meta["responsavel"],
                resultados_esperados=meta["resultados_esperados"],
                resultados_obtidos=meta["resultados_obtidos"],
                aprovacao_interna=meta["aprovacao_interna"],
                aprovacao_orientador=meta["aprovacao_orientador"],
                observacoes=meta["observacoes"],
            )
        )

    if not testes:
        raise ValueError(f"Nenhum arquivo 5.*.md encontrado em {integracao_dir}")

    testes.sort(key=lambda item: item.numero)
    return testes


def format_cell(value: str) -> str:
    """Normaliza valor para célula única de tabela Markdown."""
    cleaned = value.strip() or "—"
    cleaned = cleaned.replace("\r\n", "\n").replace("\r", "\n")
    cleaned = re.sub(r"\n+", "<br>", cleaned)
    cleaned = cleaned.replace("|", r"\|")
    return cleaned


def summarize_status(teste: TesteIntegracao) -> str:
    """Resume o estado atual do teste para a listagem por frente."""
    aprovacao = teste.aprovacao_interna.strip().upper()
    if aprovacao == "S":
        return "Aprovado internamente"
    if aprovacao == "N":
        return "Reprovado / ajuste necessário"
    obtidos = teste.resultados_obtidos.strip()
    if not obtidos or obtidos.lower().startswith("pendente"):
        return "Pendente"
    if len(obtidos) > 90:
        return f"Em andamento ({obtidos[:87].rstrip()}...)"
    return f"Registrado ({obtidos})"


def render_markdown(testes: list[TesteIntegracao]) -> str:
    """Gera o conteúdo completo de `docs/7.5 - Testes de integração.md`."""
    lines: list[str] = [
        "<!-- ARQUIVO GERADO AUTOMATICAMENTE por scripts/consolidar_integracao.py. -->",
        "<!-- NÃO EDITE ESTE ARQUIVO MANUALMENTE durante os PRs das frentes. -->",
        "<!-- Edite apenas o seu arquivo em docs/integracao/5.X-*.md e rode: py scripts/consolidar_integracao.py -->",
        "",
        "# Testes de Integração",
        "",
        (
            "Este documento consolida o planejamento e o registro oficial dos testes de integração "
            "entre os subsistemas de Estruturas, Energia, Eletrônica e Software. As regras de "
            "avaliação estão no [documento do tema do projeto]"
            "(https://docs.google.com/presentation/d/1hRx0g9mpVGQbTv44gLEoAyf4b4kKM4vnrABTnG8HlUk/edit?usp=sharing)."
        ),
        "",
        "> [!NOTE]",
        (
            "> Para evitar conflitos no Git, a redação detalhada de cada ensaio fica em arquivos "
            "individuais na pasta `docs/integracao/`. **Não edite este arquivo manualmente**: "
            "atualize apenas o `docs/integracao/5.X-*.md` correspondente e consulte o "
            "[guia de contribuição da integração](./integracao/README.md). A atualização desta "
            "visão consolidada é feita automaticamente via `py scripts/consolidar_integracao.py`."
        ),
        "",
        "## 1. Sequência e dependências",
        "",
        "1. Finalizar a soldagem, validar a continuidade elétrica e fixar a eletrônica no chassi (`5.1`).",
        "2. Ajustar a bateria e os sensores, verificar o envelope dimensional, validar a massa total e o acesso para manutenção (`5.2`, `5.3`, `5.5`, `5.7` e `5.8`).",
        "3. Validar a alimentação sob carga, unificar os drivers no firmware e conferir a leitura de bateria, IMU e motores em bancada (`5.4`, `5.9`, `5.10`, `5.11` e `5.12`).",
        "4. Calibrar o controle PID, sincronizar os sensores VL53L0X em movimento e integrar o filtro de ruído e o modo automático (`5.13`, `5.14`, `5.15` e `5.16`).",
        "5. Executar a navegação autônoma com *Flood Fill* no labirinto físico e realizar a inspeção estrutural pós-corridas (`5.17` e `5.6`).",
        "6. Validar a atualização OTA, a telemetria Wi-Fi, a renderização/latência do dashboard, a tolerância a falhas de rede e o histórico/CSV/logs (`5.18` a `5.22`).",
        "",
        "## 2. Planejamento e registro dos testes",
        "",
        (
            "| **ID** | **Descrição** | **Data de Início** | **Data de Conclusão** | "
            "**Sub-equipes** | **Responsável** | **Resultados Esperados** | **Resultados Obtidos** | "
            "**Aprovação Interna (S/N)** | **Aprovação do Orientador (S/N)** | **Observações** |"
        ),
        (
            "|:------:|---------------|:------------------:|:---------------------:|"
            "----------------|-----------------|--------------------------|------------------------|"
            ":---------------------------:|:--------------------------------:|-----------------|"
        ),
    ]

    for teste in testes:
        id_link = f"[{teste.id}](./integracao/{teste.filename})"
        row = [
            id_link,
            format_cell(teste.descricao),
            format_cell(teste.data_inicio),
            format_cell(teste.data_conclusao),
            format_cell(teste.sub_equipes),
            format_cell(teste.responsavel),
            format_cell(teste.resultados_esperados),
            format_cell(teste.resultados_obtidos),
            format_cell(teste.aprovacao_interna),
            format_cell(teste.aprovacao_orientador),
            format_cell(teste.observacoes),
        ]
        lines.append("| " + " | ".join(row) + " |")

    lines.extend([
        "",
        "## 3. Documentos detalhados por frente de integração",
        "",
    ])

    by_frente: dict[str, list[TesteIntegracao]] = {}
    for teste in testes:
        by_frente.setdefault(teste.frente, []).append(teste)

    ordered_frentes = [f for f in FRENTE_ORDER if f in by_frente]
    ordered_frentes.extend(f for f in by_frente if f not in FRENTE_ORDER)

    for idx, frente in enumerate(ordered_frentes, start=1):
        lines.append(f"### 3.{idx} {frente}")
        lines.append("")
        for teste in by_frente[frente]:
            status = summarize_status(teste)
            lines.append(
                f"- [{teste.id} — {teste.descricao}](./integracao/{teste.filename}) "
                f"— {teste.issue} · {teste.responsavel} · Status: **{status}**"
            )
        lines.append("")

    lines.extend([
        "## 4. Evidências mínimas",
        "",
        (
            "Cada teste deve possuir evidências identificadas pelo respectivo ID em "
            "[`docs/evidencias/integracao/`](./evidencias/integracao/README.md) "
            "(subpasta `docs/evidencias/integracao/<ID>/`). Conforme o tipo de ensaio, devem ser anexados:"
        ),
        "",
        "- fotografias da montagem e das medições;",
        "- vídeos das corridas, manobras e falhas simuladas (hospedados no Drive/YouTube e linkados no arquivo `5.X-*.md`, sem subir arquivos > 5 MB no Git);",
        "- logs do ESP32, do servidor e do navegador;",
        "- planilhas ou arquivos CSV com tensão, corrente, temperatura, autonomia e latência;",
        "- mapa reconstruído, trajeto percorrido e contagem dos pacotes de telemetria;",
        "- descrição das falhas encontradas, correções aplicadas e repetições realizadas.",
        "",
        (
            "Os arquivos de evidência devem usar nomes descritivos iniciados pelo ID do teste, por "
            "exemplo `5.5-medicao-largura.jpeg` e `5.20-latencia.csv`."
        ),
        "",
        "## 5. Referências",
        "",
        "- [Guia de contribuição da integração](./integracao/README.md)",
        "- [Organização das evidências de integração](./evidencias/integracao/README.md)",
        "- [Requisitos do projeto](./2%20-%20Requisitos.md)",
        "- [Projeto conceitual de estruturas](./4.1%20-%20Projeto%20conceitual%20de%20estruturas.md)",
        "- [Projeto conceitual de energia](./4.2%20-%20Projeto%20conceitual%20de%20energia.md)",
        "- [Projeto conceitual de hardware](./4.3%20-%20Projeto%20conceitual%20de%20hardware.md)",
        "- [Roteiro de testes funcionais](./4.4.6%20-%20Roteiro%20de%20testes%20funcionais.md)",
        "- [Contrato de telemetria](./4.4.7%20-%20Contrato%20de%20Telemetria.md)",
        "- [Testes de estrutura](./7.1%20-%20Testes%20de%20estrutura.md)",
        "- [Testes de energia](./7.2%20-%20Testes%20de%20energia.md)",
        "- [Testes de hardware](./7.3%20-%20Testes%20de%20hardware.md)",
        "- [Testes de software](./7.4%20-%20Testes%20de%20software.md)",
        "",
    ])

    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Consolida docs/integracao/5.*.md em docs/7.5 - Testes de integração.md."
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Verifica se docs/7.5 - Testes de integração.md está sincronizado sem alterar o arquivo.",
    )
    args = parser.parse_args(argv)

    try:
        testes = load_testes(INTEGRACAO_DIR)
    except Exception as exc:
        print(f"Erro ao carregar arquivos de integração: {exc}", file=sys.stderr)
        return 1

    expected = render_markdown(testes)

    if args.check:
        if not OUTPUT_FILE.exists():
            print(f"Arquivo ausente: {OUTPUT_FILE}", file=sys.stderr)
            return 1
        current = OUTPUT_FILE.read_text(encoding="utf-8").replace("\r\n", "\n")
        if current != expected:
            diff = "".join(
                difflib.unified_diff(
                    current.splitlines(keepends=True),
                    expected.splitlines(keepends=True),
                    fromfile=str(OUTPUT_FILE.relative_to(REPO_ROOT)),
                    tofile="esperado (consolidar_integracao.py)",
                )
            )
            print("docs/7.5 - Testes de integração.md está desatualizado:\n", file=sys.stderr)
            print(diff, file=sys.stderr)
            return 1
        print(f"OK: {OUTPUT_FILE.relative_to(REPO_ROOT)} está sincronizado ({len(testes)} testes).")
        return 0

    OUTPUT_FILE.write_text(expected, encoding="utf-8", newline="\n")
    print(f"Gerado {OUTPUT_FILE.relative_to(REPO_ROOT)} com {len(testes)} testes.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
