# _Hardware_

Esta pasta deverá armazenar arquivos referentes a:

- Simulações de circuitos eletrônicos (alimentação, sensores, atuadores etc.): [Qucs](https://qucs.sourceforge.net/), [QSPICE](https://www.qorvo.com/design-hub/design-tools/interactive/qspice), [Fritzing](https://fritzing.org/), dentre outros.
- Esquemáticos de circuitos: [KiCad](https://www.kicad.org/), Qucs, QSPICE.
- Diagramas de bloco: [Canva](https://www.canva.com/pt_br/), [TikZ](https://tikz.net/).
- _Datasheets_ de componentes eletrônicos, microcontroladores, _shields_ Arduino, motores etc.
- Testes tabelados de circuitos: arquivos CSV, Excel.
- Imagens PNG e JPEG de resultados práticos: fotos de testes reais, gráficos de resultados etc.
- Outros arquivos referentes ao _hardware_ do projeto.

> [!WARNING]
> **Não acrescente código-fonte a esta pasta.** O código desenvolvido para microcontroladores e SOCs ([Arduino](https://www.arduino.cc/), [ESP32](https://www.espressif.com/), [Raspberry Pi](https://www.raspberrypi.com/)) deverá ser armazenado na pasta [src/firmware](../src/firmware/) deste repositório.

---

## Conteúdo desta pasta

> [!NOTE]
> **Simulações:** Inicialmente foram realizadas algumas simulações básicas de circuitos apenas para fins de elicitação de requisitos, porém os arquivos de simulação foram perdidos.

### Diagramas e Esquemáticos

| Arquivo | Descrição |
|---------|-----------|
| [Diagrama de BlocosV2.jpeg](Diagrama%20de%20BlocosV2.jpeg) | Diagrama de blocos do projeto (v2) |
| [Esquemático Elétrico.jpeg](Esquemático%20Elétrico.jpeg) | Esquemático elétrico do circuito |

### Testes e Registros

| Arquivo | Descrição |
|---------|-----------|
| [testes_hardware.csv](testes_hardware.csv) | Testes tabelados de hardware |
| [registro fotografico.pdf](registro%20fotografico.pdf) | Registros fotográficos dos testes práticos |

### Datasheets

| Arquivo | Componente |
|---------|------------|
| [BNO085.PDF](datasheets/BNO085.PDF) | IMU BNO085 |
| [drv8833.pdf](datasheets/drv8833.pdf) | Driver de motor DRV8833 |
| [DS_vl53l0x-1.pdf](datasheets/DS_vl53l0x-1.pdf) | Sensor de distância VL53L0X |
| [esp32-wroom-32_datasheet_en.pdf](datasheets/esp32-wroom-32_datasheet_en.pdf) | Microcontrolador ESP32-WROOM-32 |
| [ina226.pdf](datasheets/ina226.pdf) | Monitor de corrente/potência INA226 |
| [MP1584_r1.0-779241.pdf](datasheets/MP1584_r1.0-779241.pdf) | Regulador de tensão MP1584 |
| [N20-Micro-Speed-Gear-Motor.pdf](datasheets/N20-Micro-Speed-Gear-Motor.pdf) | Micro motor N20 |
