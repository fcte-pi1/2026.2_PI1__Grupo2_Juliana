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


def calc_crc(dados: bytes) -> int:
    """Calcula o CRC-16/CCITT-FALSE sobre os bytes informados."""
    return binascii.crc_hqx(dados, 0xFFFF)


def format_frame(payload: dict[str, Any]) -> str:
    """Serializa payload em JSON compacto e anexa '|' + CRC-16 de 4 dígitos maiúsculos."""
    js = json.dumps(payload, separators=(",", ":"))
    crc = calc_crc(js.encode("utf-8"))
    return f"{js}|{crc:04X}"


def validate_limites_labirinto(
    pos_x: int,
    pos_y: int,
    largura_celula: int,
    altura_celula: int,
) -> str | None:
    """Valida se pos_x e pos_y estão dentro dos limites do labirinto (0 a limite - 1)."""
    if pos_x >= largura_celula:
        return f"fora do intervalo: pos_x={pos_x}"
    if pos_y >= altura_celula:
        return f"fora do intervalo: pos_y={pos_y}"
    return None


def _reject_constant(val: str) -> Any:
    raise ValueError(f"Constante JSON invalida: {val}")


def validate_frame(
    raw_text: str,
    largura_celula: int | None = None,
    altura_celula: int | None = None,
) -> ValidationResult:
    """Valida o quadro de telemetria contra os passos 1 a 6 do Contrato 4.4.7."""
    if not isinstance(raw_text, str) or "|" not in raw_text:
        return ValidationResult(valid=False, error="formato invalido")

    json_str, _, received_crc = raw_text.rpartition("|")
    if not json_str or not CRC_REGEX.match(received_crc):
        return ValidationResult(valid=False, error="formato invalido")

    # 2. Checksum CRC-16
    calculated_crc = calc_crc(json_str.encode("utf-8"))
    if f"{calculated_crc:04X}" != received_crc.upper():
        return ValidationResult(
            valid=False,
            error=f"checksum invalido (recebido {received_crc.upper()}, calculado {calculated_crc:04X})",
        )

    # 3. JSON válido (rejeita NaN e Infinity)
    try:
        data = json.loads(json_str, parse_constant=_reject_constant)
    except Exception:
        return ValidationResult(valid=False, error="json invalido")

    if not isinstance(data, dict):
        return ValidationResult(valid=False, error="json invalido")

    # 4. Versão
    if "v" not in data:
        return ValidationResult(valid=False, error="campo ausente: v")
    if not isinstance(data["v"], int) or isinstance(data["v"], bool):
        return ValidationResult(valid=False, error="tipo invalido: v")
    if data["v"] != 1:
        return ValidationResult(valid=False, error="versao nao suportada")

    # 5. Cabeçalho obrigatório
    for campo in ("t", "corrida", "seq", "tempo_ms"):
        if campo not in data:
            return ValidationResult(valid=False, error=f"campo ausente: {campo}")

    if not isinstance(data["t"], str):
        return ValidationResult(valid=False, error="tipo invalido: t")
    if data["t"] not in ("tel", "evt"):
        return ValidationResult(valid=False, error="tipo invalido: t")

    for campo in ("corrida", "seq", "tempo_ms"):
        val = data[campo]
        if not isinstance(val, int) or isinstance(val, bool):
            return ValidationResult(valid=False, error=f"tipo invalido: {campo}")

    corrida = data["corrida"]
    seq = data["seq"]
    tempo_ms = data["tempo_ms"]

    if corrida < 1:
        return ValidationResult(valid=False, error=f"fora do intervalo: corrida={corrida}")
    if seq < 0:
        return ValidationResult(valid=False, error=f"fora do intervalo: seq={seq}")
    if tempo_ms < 0:
        return ValidationResult(valid=False, error=f"fora do intervalo: tempo_ms={tempo_ms}")

    # 6. Campos específicos por tipo
    if data["t"] == "tel":
        obrigatorios_tel = ["pos_x", "pos_y", "orientacao", "paredes", "velocidade", "tensao_bateria", "estado"]
        for campo in obrigatorios_tel:
            if campo not in data:
                return ValidationResult(valid=False, error=f"campo ausente: {campo}")

        # pos_x e pos_y
        for coord, limite, nome in [("pos_x", largura_celula, "pos_x"), ("pos_y", altura_celula, "pos_y")]:
            val = data[coord]
            if not isinstance(val, int) or isinstance(val, bool):
                return ValidationResult(valid=False, error=f"tipo invalido: {nome}")
            if val < 0:
                return ValidationResult(valid=False, error=f"fora do intervalo: {nome}={val}")
            if limite is not None and val >= limite:
                return ValidationResult(valid=False, error=f"fora do intervalo: {nome}={val}")

        # orientacao
        if not isinstance(data["orientacao"], str):
            return ValidationResult(valid=False, error="tipo invalido: orientacao")
        if data["orientacao"] not in ("N", "S", "L", "O"):
            return ValidationResult(valid=False, error=f"fora do intervalo: orientacao={data['orientacao']}")

        # paredes
        paredes = data["paredes"]
        if not isinstance(paredes, int) or isinstance(paredes, bool):
            return ValidationResult(valid=False, error="tipo invalido: paredes")
        if not (0 <= paredes <= 15):
            return ValidationResult(valid=False, error=f"fora do intervalo: paredes={paredes}")

        # velocidade: 0 a 5,000 m/s (seção 2.2)
        vel = data["velocidade"]
        if not isinstance(vel, (int, float)) or isinstance(vel, bool):
            return ValidationResult(valid=False, error="tipo invalido: velocidade")
        if not (0 <= vel <= 5.0):
            return ValidationResult(valid=False, error=f"fora do intervalo: velocidade={vel}")

        # tensao_bateria: 0 a 9,00 V (seção 2.2)
        tb = data["tensao_bateria"]
        if not isinstance(tb, (int, float)) or isinstance(tb, bool):
            return ValidationResult(valid=False, error="tipo invalido: tensao_bateria")
        if not (0 <= tb <= 9.0):
            return ValidationResult(valid=False, error=f"fora do intervalo: tensao_bateria={tb}")

        # estado
        if not isinstance(data["estado"], str):
            return ValidationResult(valid=False, error="tipo invalido: estado")
        if data["estado"] not in ("explorando", "retornando", "rapido", "parado"):
            return ValidationResult(valid=False, error=f"fora do intervalo: estado={data['estado']}")

        # valor_flood
        if "valor_flood" in data and data["valor_flood"] is not None:
            vf = data["valor_flood"]
            if not isinstance(vf, int) or isinstance(vf, bool):
                return ValidationResult(valid=False, error="tipo invalido: valor_flood")
            if vf < 0:
                return ValidationResult(valid=False, error=f"fora do intervalo: valor_flood={vf}")

    elif data["t"] == "evt":
        if "evento" not in data:
            return ValidationResult(valid=False, error="campo ausente: evento")
        if not isinstance(data["evento"], str):
            return ValidationResult(valid=False, error="tipo invalido: evento")
        if data["evento"] not in ("inicio", "fim", "bateria_critica", "reconexao"):
            return ValidationResult(valid=False, error=f"fora do intervalo: evento={data['evento']}")

        evento = data["evento"]
        if evento == "inicio":
            if seq != 0:
                return ValidationResult(valid=False, error=f"fora do intervalo: seq={seq}")
            if tempo_ms != 0:
                return ValidationResult(valid=False, error=f"fora do intervalo: tempo_ms={tempo_ms}")
            if "tensao_bateria" in data and data["tensao_bateria"] is not None:
                tb = data["tensao_bateria"]
                if not isinstance(tb, (int, float)) or isinstance(tb, bool):
                    return ValidationResult(valid=False, error="tipo invalido: tensao_bateria")
                if not (0 <= tb <= 9.0):
                    return ValidationResult(valid=False, error=f"fora do intervalo: tensao_bateria={tb}")

        elif evento == "fim":
            if "resultado" not in data:
                return ValidationResult(valid=False, error="campo ausente: resultado")
            if not isinstance(data["resultado"], str):
                return ValidationResult(valid=False, error="tipo invalido: resultado")
            if data["resultado"] not in ("concluida", "abandonada"):
                return ValidationResult(valid=False, error=f"fora do intervalo: resultado={data['resultado']}")

            res = data["resultado"]
            if res == "concluida":
                if "tempo_conc_s" not in data or data["tempo_conc_s"] is None:
                    return ValidationResult(valid=False, error="campo ausente: tempo_conc_s")

            if "tempo_conc_s" in data and data["tempo_conc_s"] is not None:
                tc = data["tempo_conc_s"]
                if not isinstance(tc, (int, float)) or isinstance(tc, bool):
                    return ValidationResult(valid=False, error="tipo invalido: tempo_conc_s")
                if tc < 0:
                    return ValidationResult(valid=False, error=f"fora do intervalo: tempo_conc_s={tc}")

            if "consumo_medio_ma" in data and data["consumo_medio_ma"] is not None:
                cm = data["consumo_medio_ma"]
                if not isinstance(cm, (int, float)) or isinstance(cm, bool):
                    return ValidationResult(valid=False, error="tipo invalido: consumo_medio_ma")
                if cm < 0:
                    return ValidationResult(valid=False, error=f"fora do intervalo: consumo_medio_ma={cm}")

            if res == "abandonada":
                if "motivo" in data and data["motivo"] is not None:
                    motivo = data["motivo"]
                    if not isinstance(motivo, str):
                        return ValidationResult(valid=False, error="tipo invalido: motivo")
                    if motivo not in ("bateria_critica", "falha_sensor_bateria", "colisao", "tempo_esgotado", "manual"):
                        return ValidationResult(valid=False, error=f"fora do intervalo: motivo={motivo}")

        elif evento == "bateria_critica":
            if "tensao_bateria" not in data:
                return ValidationResult(valid=False, error="campo ausente: tensao_bateria")
            tb = data["tensao_bateria"]
            if not isinstance(tb, (int, float)) or isinstance(tb, bool):
                return ValidationResult(valid=False, error="tipo invalido: tensao_bateria")
            if not (0 <= tb <= 9.0):
                return ValidationResult(valid=False, error=f"fora do intervalo: tensao_bateria={tb}")

        elif evento == "reconexao":
            for campo in ("offline_ms", "pendentes"):
                if campo not in data:
                    return ValidationResult(valid=False, error=f"campo ausente: {campo}")
                val = data[campo]
                if not isinstance(val, int) or isinstance(val, bool):
                    return ValidationResult(valid=False, error=f"tipo invalido: {campo}")
                if val < 0:
                    return ValidationResult(valid=False, error=f"fora do intervalo: {campo}={val}")

    return ValidationResult(valid=True, data=data)
