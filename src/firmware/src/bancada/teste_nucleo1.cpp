// Teste de bancada: o que o núcleo 1 (com defeito na cache) consegue fazer.
// So USB. Gravar: pio run -e teste_nucleo1 -t upload
//
// E1 - Mapa do defeito: o núcleo 1 lê pela cache dele todo o código (.flash.text)
//      e as constantes (.rodata) e compara com o que o núcleo 0 leu. Ler não
//      executa nada, então não trava. Repete várias vezes para ver se o erro é
//      sempre o mesmo.
// E2 - Núcleo 1 só com código na RAM interna (IRAM_ATTR, sem passar pela cache):
//      conta pesada por ~10 s, conferida pelo núcleo 0.
// E3 - Núcleo 1 com código normal na flash: Flood Fill da lib/ em laço, conferido
//      pelo núcleo 0. Pode travar: um contador em memória RTC (sobrevive ao reset)
//      diz quantas vezes rodou antes.
//
// Comandos: 1, 2 ou 3 repetem um experimento; t roda E1 e E2.
#include <Arduino.h>
#include <esp_system.h>

#include "floodfill.h"
#include "labirinto.h"

extern "C" uint32_t _text_start, _text_end, _rodata_start, _rodata_end;

namespace
{
const int PIN_DRV_SLEEP = 32;

// ---------- comunicação entre os núcleos (só RAM, nada de flash) ----------
volatile int pedido = 0;     // 0 nada, 1 E1, 2 E2, 3 E3
volatile int pronto = 0;
volatile uint32_t progresso = 0;

// E1
const uint32_t LINHA = 32; // bytes por linha de cache
uint32_t *assinaturaN0 = nullptr;
uint32_t *assinaturaN1 = nullptr;
volatile const uint32_t *regInicio;
volatile uint32_t regPalavras;

// E2
volatile uint32_t sementeE2;
volatile uint32_t voltasE2;
volatile uint32_t resultadoE2;

// E3: sobrevive ao reset por pânico
RTC_NOINIT_ATTR uint32_t e3Magico;
RTC_NOINIT_ATTR uint32_t e3Rodadas;
RTC_NOINIT_ATTR uint32_t e3Erradas;
const uint32_t MAGICO = 0xC0DE1E30;
uint32_t referenciaE3[3] = {}; // calculadas pelo núcleo 0

// Assinatura de uma linha: soma e xor misturados (pega bit trocado).
inline uint32_t IRAM_ATTR assinar(volatile const uint32_t *p)
{
    uint32_t a = 0x9E3779B9, x = 0;
    for (uint32_t i = 0; i < LINHA / 4; i++)
    {
        uint32_t v = p[i];
        a = (a ^ v) * 16777619u;
        x ^= v + i;
    }
    return a ^ (x << 1);
}

// Linhas suspeitas (posições 0x2160 e 0x2180 de cada bloco de 16 KB), copiadas
// pelo núcleo 1 na mesma leitura, para achar o bit exato que vem errado.
struct LinhaCopiada
{
    uint32_t endereco;
    uint32_t palavras[8];
};
LinhaCopiada suspeitas[48];
volatile uint32_t nSuspeitas = 0;

void IRAM_ATTR assinarRegiao(uint32_t *saida, bool copiarSuspeitas = false)
{
    uint32_t linhas = regPalavras / (LINHA / 4);
    if (copiarSuspeitas)
        nSuspeitas = 0;
    for (uint32_t l = 0; l < linhas; l++)
    {
        volatile const uint32_t *p = regInicio + l * (LINHA / 4);
        saida[l] = assinar(p);
        uint32_t indice = (uint32_t)p & 0x3FE0;
        if (copiarSuspeitas && (indice == 0x2160 || indice == 0x2180) && nSuspeitas < 48)
        {
            suspeitas[nSuspeitas].endereco = (uint32_t)p;
            for (int i = 0; i < 8; i++)
                suspeitas[nSuspeitas].palavras[i] = p[i];
            nSuspeitas++;
        }
    }
}

uint32_t IRAM_ATTR contaPesada(uint32_t semente, uint32_t voltas)
{
    uint32_t x = semente ? semente : 1, acc = 0;
    for (uint32_t i = 0; i < voltas; i++)
    {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        acc = (acc + x) * 2654435761u + (acc >> 7);
    }
    return acc;
}

// Labirinto fixo para o E3 (12x4 com algumas paredes).
uint32_t rodarFloodFill(uint32_t variacao)
{
    static Labirinto lab;
    static FloodFill ff;
    lab.iniciar(12, 4, NORTE);
    for (uint8_t x = 1; x < 11; x += 2)
        lab.marcarParede(x, (uint8_t)((x + variacao) % 3), LESTE);
    ff.recalcular(lab, EXPLORACAO);
    uint32_t soma = 0;
    for (uint8_t x = 0; x < 12; x++)
        for (uint8_t y = 0; y < 4; y++)
            soma = soma * 31 + ff.distancia(x, y);
    return soma;
}

// ---------- tarefa no núcleo 1 ----------
void IRAM_ATTR tarefaNucleo1(void *)
{
    for (;;)
    {
        int p = pedido;
        if (p == 1)
        {
            assinarRegiao(assinaturaN1, true);
            pedido = 0;
            pronto = 1;
        }
        else if (p == 2)
        {
            resultadoE2 = contaPesada(sementeE2, voltasE2);
            pedido = 0;
            pronto = 1;
        }
        else if (p == 4)
        {
            // snprintf usa _svfprintf_r, que neste binário passa por uma linha
            // de cache com defeito: o modelo prevê que trava.
            char buf[96];
            for (uint32_t i = 0; pedido == 4; i++)
            {
                snprintf(buf, sizeof(buf), "%d %u %x %s %c %f %e %g %ld", (int)i, i, i, "abc", 'x',
                         i * 0.5, i * 1.5e3, i / 7.0, (long)i);
                e3Rodadas++;
                progresso = e3Rodadas;
            }
            pronto = 1;
        }
        else if (p == 3)
        {
            // Daqui em diante roda código da flash pelo núcleo 1.
            for (uint32_t i = 0; pedido == 3; i++)
            {
                uint32_t r = rodarFloodFill(i % 3);
                if (r != referenciaE3[i % 3])
                    e3Erradas++;
                e3Rodadas++;
                progresso = e3Rodadas;
            }
            pronto = 1;
        }
        vTaskDelay(1);
    }
}

void esperarNucleo1()
{
    while (!pronto)
        delay(1);
    pronto = 0;
}

// ---------- E1 ----------
void mapear(const char *nome, uint32_t *inicio, uint32_t *fim, int passadas)
{
    regInicio = inicio;
    regPalavras = ((uint32_t)fim - (uint32_t)inicio) / 4;
    uint32_t linhas = regPalavras / (LINHA / 4);
    Serial.printf("\n[E1] %s: 0x%08x a 0x%08x (%u KB, %u linhas de 32 B)\n", nome, (uint32_t)inicio,
                  (uint32_t)fim, (regPalavras * 4) / 1024, linhas);

    assinarRegiao(assinaturaN0); // referência: núcleo 0
    for (int p = 1; p <= passadas; p++)
    {
        pedido = 1;
        esperarNucleo1();
        uint32_t ruins = 0;
        for (uint32_t l = 0; l < linhas; l++)
        {
            if (assinaturaN0[l] == assinaturaN1[l])
                continue;
            ruins++;
            if (ruins > 6)
                continue;
            // Mostra a palavra errada: lida de novo pelo núcleo 0 e pelo 1.
            volatile const uint32_t *base = regInicio + l * (LINHA / 4);
            uint32_t endereco = (uint32_t)base;
            Serial.printf("     passada %d: linha 0x%08x (indice na cache 32KB: 0x%04x, 16KB: 0x%04x)\n", p,
                          endereco, endereco & 0x7FFF, endereco & 0x3FFF);
        }
        Serial.printf("  passada %d: %u de %u linhas diferentes no nucleo 1\n", p, ruins, linhas);
        // Bit exato: cópia do núcleo 1 x leitura do núcleo 0.
        uint32_t candidatas = 0, comBit1 = 0;
        for (uint32_t s = 0; s < nSuspeitas; s++)
        {
            volatile const uint32_t *ref = (volatile const uint32_t *)suspeitas[s].endereco;
            for (int i = 0; i < 8; i++)
            {
                uint32_t certo = ref[i], lido = suspeitas[s].palavras[i];
                if (certo != lido)
                    Serial.printf("       0x%08x palavra %d: certo 0x%08x lido 0x%08x (bits trocados 0x%08x, "
                                  "lidos como %s)\n",
                                  suspeitas[s].endereco, i, certo, lido, certo ^ lido,
                                  (lido & (certo ^ lido)) ? "1" : "0");
            }
            candidatas++;
            if (ref[4] & (1u << 17))
                comBit1++;
        }
        Serial.printf("       %u linhas nas posicoes 0x2160/0x2180; %u com o bit 17 da palavra 4 em 1\n",
                      candidatas, comBit1);
    }
}

void experimento1()
{
    Serial.println("\n===== E1: mapa do defeito (leitura pela cache do nucleo 1) =====");
    mapear(".flash.text (codigo)", &_text_start, &_text_end, 5);
    mapear(".rodata (constantes)", &_rodata_start, &_rodata_end, 5);
}

// ---------- E2 ----------
void experimento2()
{
    Serial.println("\n===== E2: nucleo 1 so com codigo na RAM (IRAM_ATTR) =====");
    uint32_t inicio = millis(), erros = 0, rodadas = 0;
    while (millis() - inicio < 10000)
    {
        sementeE2 = esp_random();
        voltasE2 = 2000000;
        pedido = 2;
        esperarNucleo1();
        uint32_t esperado = contaPesada(sementeE2, voltasE2);
        rodadas++;
        if (esperado != resultadoE2)
            erros++;
    }
    Serial.printf("  %u rodadas de 2 milhoes de iteracoes no nucleo 1, %u resultados errados\n", rodadas, erros);
}

// ---------- E4 ----------
void experimento4()
{
    Serial.println("\n===== E4: nucleo 1 rodando snprintf (codigo numa linha ruim) por 30 s =====");
    Serial.println("  (previsao: trava; o resultado aparece no proximo boot)");
    Serial.flush();
    e3Magico = MAGICO + 4;
    e3Rodadas = 0;
    e3Erradas = 0;
    pedido = 4;
    uint32_t inicio = millis();
    while (millis() - inicio < 30000)
    {
        delay(5000);
        Serial.printf("  %2lus: %u chamadas\n", (millis() - inicio) / 1000, e3Rodadas);
    }
    pedido = 0;
    esperarNucleo1();
    Serial.printf("  E4 SOBREVIVEU: %u chamadas em 30 s\n", e3Rodadas);
    e3Magico = 0;
}

// ---------- E3 ----------
void experimento3()
{
    Serial.println("\n===== E3: nucleo 1 rodando codigo da flash (Flood Fill) por 30 s =====");
    Serial.println("  (se travar, a ESP reinicia e o resultado aparece no proximo boot)");
    Serial.flush();
    e3Magico = MAGICO;
    e3Rodadas = 0;
    e3Erradas = 0;
    pedido = 3;
    uint32_t inicio = millis();
    while (millis() - inicio < 30000)
    {
        delay(5000);
        Serial.printf("  %2lus: %u rodadas, %u erradas\n", (millis() - inicio) / 1000, e3Rodadas, e3Erradas);
    }
    pedido = 0;
    esperarNucleo1();
    Serial.printf("  E3 SOBREVIVEU: %u rodadas em 30 s, %u resultados errados\n", e3Rodadas, e3Erradas);
    e3Magico = 0;
}
} // namespace

void setup()
{
    pinMode(PIN_DRV_SLEEP, OUTPUT);
    digitalWrite(PIN_DRV_SLEEP, LOW);
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== Teste do nucleo 1 ===");

    esp_reset_reason_t motivo = esp_reset_reason();
    if (e3Magico == MAGICO + 4)
    {
        Serial.printf("[E4] RESULTADO DO BOOT ANTERIOR: o nucleo 1 travou no snprintf (motivo do reset %d) "
                      "depois de %u chamadas\n", motivo, e3Rodadas);
        e3Magico = 0;
    }
    if (e3Magico == MAGICO)
    {
        Serial.printf("[E3] RESULTADO DO BOOT ANTERIOR: o nucleo 1 travou (motivo do reset %d) depois de "
                      "%u rodadas do Flood Fill (%u erradas antes de travar)\n",
                      motivo, e3Rodadas, e3Erradas);
        e3Magico = 0;
    }

    uint32_t maxLinhas = (((uint32_t)&_text_end - (uint32_t)&_text_start) / LINHA) + 1;
    assinaturaN0 = (uint32_t *)malloc(maxLinhas * 4);
    assinaturaN1 = (uint32_t *)malloc(maxLinhas * 4);
    if (!assinaturaN0 || !assinaturaN1)
    {
        Serial.println("Sem memoria para o E1");
        while (true)
            delay(1000);
    }
    for (uint32_t v = 0; v < 3; v++)
        referenciaE3[v] = rodarFloodFill(v);
    xTaskCreatePinnedToCore(tarefaNucleo1, "nucleo1", 4096, nullptr, 1, nullptr, 1);
    Serial.println("Comandos: 1=E1  2=E2  3=E3  4=E4  t=E1+E2");
    experimento1();
    experimento2();
    Serial.println("\nPronto. Mande 3 para o E3 (pode reiniciar a ESP).");
}

void loop()
{
    delay(10); // no núcleo 0, o loop precisa ceder a vez à tarefa ociosa (watchdog)
    if (!Serial.available())
        return;
    char c = (char)Serial.read();
    if (c == '1')
        experimento1();
    else if (c == '2')
        experimento2();
    else if (c == '3')
        experimento3();
    else if (c == '4')
        experimento4();
    else if (c == 't')
    {
        experimento1();
        experimento2();
    }
}
