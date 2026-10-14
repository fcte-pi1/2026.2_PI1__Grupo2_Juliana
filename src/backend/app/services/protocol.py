"""Validação e processamento de mensagens segundo o Contrato de Telemetria (4.4.7)."""

import binascii
import json
import re
from dataclasses import dataclass
from typing import Any

CRC_REGEX = re.compile(r"^[0-9A-Fa-f]{4}$")


@dataclass
class ValidationResult:
    """Resultado da validação estrutural do pacote de telemetria."""
    valid: bool
    error: str | None = None
    data: dict[str, Any] | None = None
    raw_frame: str = ""
    corrida: int | None = None
    seq: int | None = None


def calc_crc(dados: bytes) -> int:
    """Calcula o CRC-16/CCITT-FALSE sobre os bytes informados."""
    return binascii.crc_hqx(dados, 0xFFFF)


def format_frame(payload: dict[str, Any]) -> str:
    """Serializa payload em JSON compacto e anexa '|' + CRC-16 de 4 dígitos maiúsculos."""
    js = json.dumps(payload, separators=(",", ":"))
    crc = calc_crc(js.encode("utf-8"))
    return f"{js}|{crc:04X}"


def validate_frame(
    raw_text: str,
    largura_celula: int | None = None,
    altura_celula: int | None = None,
) -> ValidationResult:
    """Valida o quadro de telemetria contra os passos 1 a 6 do Contrato 4.4.7."""
    if not isinstance(raw_text, str) or "|" not in raw_text:
        return ValidationResult(valid=False, error="formato invalido", raw_frame=raw_text)

    json_str, _, received_crc = raw_text.rpartition("|")
    if not json_str or not CRC_REGEX.match(received_crc):
        return ValidationResult(valid=False, error="formato invalido", raw_frame=raw_text)

    # 2. Checksum CRC-16
    calculated_crc = calc_crc(json_str.encode("utf-8"))
    if f"{calculated_crc:04X}" != received_crc.upper():
        return ValidationResult(
            valid=False,
            error=f"checksum invalido (recebido {received_crc.upper()}, calculado {calculated_crc:04X})",
            raw_frame=raw_text,
        )

    # 3. JSON válido
    try:
        data = json.loads(json_str)
    except Exception:
        return ValidationResult(valid=False, error="json invalido", raw_frame=raw_text)

    if not isinstance(data, dict):
        return ValidationResult(valid=False, error="json invalido", raw_frame=raw_text)

    # 4. Versão
    if "v" not in data:
        return ValidationResult(valid=False, error="campo ausente: v", raw_frame=raw_text)
    if not isinstance(data["v"], int) or isinstance(data["v"], bool):
        return ValidationResult(valid=False, error="tipo invalido: v", raw_frame=raw_text)
    if data["v"] != 1:
        return ValidationResult(valid=False, error="versao nao suportada", raw_frame=raw_text)

    # 5. Cabeçalho obrigatório
    for campo in ("t", "corrida", "seq", "tempo_ms"):
        if campo not in data:
            return ValidationResult(valid=False, error=f"campo ausente: {campo}", raw_frame=raw_text)

    if not isinstance(data["t"], str):
        return ValidationResult(valid=False, error="tipo invalido: t", raw_frame=raw_text)
    if data["t"] not in ("tel", "evt"):
        return ValidationResult(valid=False, error="tipo invalido: t", raw_frame=raw_text)

    for campo in ("corrida", "seq", "tempo_ms"):
        val = data[campo]
        if not isinstance(val, int) or isinstance(val, bool):
            return ValidationResult(valid=False, error=f"tipo invalido: {campo}", raw_frame=raw_text)

    corrida = data["corrida"]
    seq = data["seq"]
    tempo_ms = data["tempo_ms"]

    if corrida < 1:
        return ValidationResult(valid=False, error=f"fora do intervalo: corrida={corrida}", raw_frame=raw_text)
    if seq < 0:
        return ValidationResult(valid=False, error=f"fora do intervalo: seq={seq}", raw_frame=raw_text)
    if tempo_ms < 0:
        return ValidationResult(valid=False, error=f"fora do intervalo: tempo_ms={tempo_ms}", raw_frame=raw_text)

    # 6. Campos específicos por tipo
    if data["t"] == "tel":
        obrigatorios_tel = ["pos_x", "pos_y", "orientacao", "paredes", "velocidade", "tensao_bateria", "estado"]
        for campo in obrigatorios_tel:
            if campo not in data:
                return ValidationResult(valid=False, error=f"campo ausente: {campo}", raw_frame=raw_text)

        # pos_x e pos_y
        for coord, limite, nome in [("pos_x", largura_celula, "pos_x"), ("pos_y", altura_celula, "pos_y")]:
            val = data[coord]
            if not isinstance(val, int) or isinstance(val, bool):
                return ValidationResult(valid=False, error=f"tipo invalido: {nome}", raw_frame=raw_text)
            if val < 0 or (limite is not None and val >= limite):
                return ValidationResult(valid=False, error=f"fora do intervalo: {nome}={val}", raw_frame=raw_text)

        # orientacao
        if not isinstance(data["orientacao"], str):
            return ValidationResult(valid=False, error="tipo invalido: orientacao", raw_frame=raw_text)
        if data["orientacao"] not in ("N", "S", "L", "O"):
            return ValidationResult(valid=False, error=f"fora do intervalo: orientacao={data['orientacao']}", raw_frame=raw_text)

        # paredes
        paredes = data["paredes"]
        if not isinstance(paredes, int) or isinstance(paredes, bool):
            return ValidationResult(valid=False, error="tipo invalido: paredes", raw_frame=raw_text)
        if not (0 <= paredes <= 15):
            return ValidationResult(valid=False, error=f"fora do intervalo: paredes={paredes}", raw_frame=raw_text)

        # velocidade e tensao_bateria
        for campo in ("velocidade", "tensao_bateria"):
            val = data[campo]
            if not isinstance(val, (int, float)) or isinstance(val, bool):
                return ValidationResult(valid=False, error=f"tipo invalido: {campo}", raw_frame=raw_text)
            if val < 0:
                return ValidationResult(valid=False, error=f"fora do intervalo: {campo}={val}", raw_frame=raw_text)

        # estado
        if not isinstance(data["estado"], str):
            return ValidationResult(valid=False, error="tipo invalido: estado", raw_frame=raw_text)
        if data["estado"] not in ("explorando", "retornando", "rapido", "parado"):
            return ValidationResult(valid=False, error=f"fora do intervalo: estado={data['estado']}", raw_frame=raw_text)

        # valor_flood
        if "valor_flood" in data and data["valor_flood"] is not None:
            vf = data["valor_flood"]
            if not isinstance(vf, int) or isinstance(vf, bool):
                return ValidationResult(valid=False, error="tipo invalido: valor_flood", raw_frame=raw_text)
            if vf < 0:
                return ValidationResult(valid=False, error=f"fora do intervalo: valor_flood={vf}", raw_frame=raw_text)

    elif data["t"] == "evt":
        if "evento" not in data:
            return ValidationResult(valid=False, error="campo ausente: evento", raw_frame=raw_text)
        if not isinstance(data["evento"], str):
            return ValidationResult(valid=False, error="tipo invalido: evento", raw_frame=raw_text)
        if data["evento"] not in ("inicio", "fim", "bateria_critica", "reconexao"):
            return ValidationResult(valid=False, error=f"fora do intervalo: evento={data['evento']}", raw_frame=raw_text)

        evento = data["evento"]
        if evento == "inicio":
            if seq != 0:
                return ValidationResult(valid=False, error=f"fora do intervalo: seq={seq}", raw_frame=raw_text)
            if tempo_ms != 0:
                return ValidationResult(valid=False, error=f"fora do intervalo: tempo_ms={tempo_ms}", raw_frame=raw_text)
            if "tensao_bateria" in data:
                tb = data["tensao_bateria"]
                if not isinstance(tb, (int, float)) or isinstance(tb, bool) or tb < 0:
                    return ValidationResult(valid=False, error="tipo invalido: tensao_bateria", raw_frame=raw_text)

        elif evento == "fim":
            if "resultado" not in data or data["resultado"] not in ("concluida", "abandonada"):
                return ValidationResult(valid=False, error="campo ausente: resultado", raw_frame=raw_text)

        elif evento == "bateria_critica":
            if "tensao_bateria" not in data:
                return ValidationResult(valid=False, error="campo ausente: tensao_bateria", raw_frame=raw_text)
            tb = data["tensao_bateria"]
            if not isinstance(tb, (int, float)) or isinstance(tb, bool) or tb < 0:
                return ValidationResult(valid=False, error="tipo invalido: tensao_bateria", raw_frame=raw_text)

        elif evento == "reconexao":
            for campo in ("offline_ms", "pendentes"):
                if campo not in data:
                    return ValidationResult(valid=False, error=f"campo ausente: {campo}", raw_frame=raw_text)
                val = data[campo]
                if not isinstance(val, int) or isinstance(val, bool) or val < 0:
                    return ValidationResult(valid=False, error=f"tipo invalido: {campo}", raw_frame=raw_text)

    return ValidationResult(valid=True, data=data, raw_frame=raw_text, corrida=corrida, seq=seq)
