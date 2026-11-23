#include "navegacao_nucleo1.h"

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "floodfill.h"
#include "labirinto.h"
#include "modo_automatico.h"

namespace
{
// ---------- troca de dados entre os núcleos (só RAM e FreeRTOS, em IRAM) ----------
portMUX_TYPE trava = portMUX_INITIALIZER_UNLOCKED;
struct Sensores
{
    bool frente, esquerda, direita;
    Direcao orientacao;
    uint32_t versao;
};
Sensores entrada = {false, false, false, NORTE, 0};
Navegacao::Estado saida = {-1, 0, 0, 0, 0, 0, 0};

// ---------- navegação real (robô parado na largada) ----------
Labirinto mapa;
FloodFill floodFill;

NUCLEO1 void marcar(Direcao d, bool parede)
{
    if (parede)
        mapa.marcarParede(mapa.x(), mapa.y(), d);
    else
        mapa.marcarAberta(mapa.x(), mapa.y(), d);
}

// ---------- corrida simulada (carga) ----------
// MISTO_4X4 de test/desenhos_labirintos.h: rota mínima de 6 movimentos,
// conferida à mão. Em RAM (sem const), fora da cache da flash.
char labirintoSimulado[] = "+---+---+---+---+\n"
                           "|               |\n"
                           "+   +   +   +   +\n"
                           "|       |       |\n"
                           "+   +---+   +   +\n"
                           "|           |   |\n"
                           "+   +   +   +   +\n"
                           "|   |           |\n"
                           "+---+---+---+---+\n";
const uint8_t ROTA_CONFERIDA = 6;

class RoboSimuladoNucleo1 : public InterfaceRobo
{
public:
    NUCLEO1 explicit RoboSimuladoNucleo1(const Labirinto &g) : gabarito_(g) {}
    NUCLEO1 ~RoboSimuladoNucleo1() override {}
    NUCLEO1 bool calibrar() override { return true; }
    NUCLEO1 void sentir(Labirinto &m) override
    {
        Direcao f = m.orientacao();
        revelar(m, f);
        revelar(m, girarDireita(f));
        revelar(m, girarEsquerda(f));
    }
    NUCLEO1 bool mover(Direcao d) override
    {
        if (gabarito_.temParede(x_, y_, d))
            return false;
        x_ = (uint8_t)(x_ + deslocamentoX(d));
        y_ = (uint8_t)(y_ + deslocamentoY(d));
        return true;
    }

private:
    NUCLEO1 void revelar(Labirinto &m, Direcao d)
    {
        if (gabarito_.temParede(x_, y_, d))
            m.marcarParede(x_, y_, d);
        else
            m.marcarAberta(x_, y_, d);
    }
    const Labirinto &gabarito_;
    uint8_t x_ = 0, y_ = 0;
};

Labirinto gabaritoSimulado;
Labirinto mapaSimulado;
uint8_t rotaEsperada = 0;

NUCLEO1 uint8_t corridaSimulada()
{
    mapaSimulado.iniciar(4, 4, NORTE);
    RoboSimuladoNucleo1 robo(gabaritoSimulado);
    ModoAutomatico modo(robo, mapaSimulado);
    modo.iniciar();
    if (modo.executar() != ESTADO_PARADO)
        return 0;
    return modo.passosRapido();
}

NUCLEO1 void tarefaNavegacao(void *)
{
    uint32_t versaoVista = UINT32_MAX;
    uint64_t somaUs = 0;
    TickType_t ultimaCorrida = 0;
    for (;;)
    {
        Sensores s;
        portENTER_CRITICAL(&trava);
        s = entrada;
        portEXIT_CRITICAL(&trava);

        if (s.versao != versaoVista)
        {
            versaoVista = s.versao;
            int64_t inicio = esp_timer_get_time();
            mapa.girar(s.orientacao);
            marcar(s.orientacao, s.frente);
            marcar(girarEsquerda(s.orientacao), s.esquerda);
            marcar(girarDireita(s.orientacao), s.direita);
            floodFill.recalcular(mapa, EXPLORACAO);
            uint8_t d = floodFill.valorAtual(mapa);
            uint32_t us = (uint32_t)(esp_timer_get_time() - inicio);

            portENTER_CRITICAL(&trava);
            saida.valorFlood = d == INFINITO ? -1 : d;
            saida.nucleo = (uint8_t)xPortGetCoreID();
            saida.ciclos++;
            somaUs += us;
            saida.tempoMedioUs = (uint32_t)(somaUs / saida.ciclos);
            if (us > saida.tempoMaxUs)
                saida.tempoMaxUs = us;
            portEXIT_CRITICAL(&trava);
        }

        if (xTaskGetTickCount() - ultimaCorrida >= pdMS_TO_TICKS(1000))
        {
            ultimaCorrida = xTaskGetTickCount();
            bool ok = corridaSimulada() == rotaEsperada;
            portENTER_CRITICAL(&trava);
            if (ok)
                saida.corridasOk++;
            else
                saida.corridasErradas++;
            portEXIT_CRITICAL(&trava);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
} // namespace

namespace Navegacao
{
void iniciar(uint8_t largura, uint8_t altura)
{
    mapa.iniciar(largura, altura, NORTE);
    // Gabarito carregado no núcleo 0, antes da tarefa existir. A rota esperada
    // é a conferida à mão: se o desenho não carregar, toda corrida conta como errada.
    rotaEsperada = carregar(labirintoSimulado, gabaritoSimulado, NORTE) ? ROTA_CONFERIDA : 255;
    xTaskCreatePinnedToCore(tarefaNavegacao, "navegacao", 8192, nullptr, 2, nullptr, 1);
}

void atualizarSensores(bool paredeFrente, bool paredeEsquerda, bool paredeDireita, Direcao orientacao)
{
    portENTER_CRITICAL(&trava);
    entrada.frente = paredeFrente;
    entrada.esquerda = paredeEsquerda;
    entrada.direita = paredeDireita;
    entrada.orientacao = orientacao;
    entrada.versao++;
    portEXIT_CRITICAL(&trava);
}

Estado estado()
{
    portENTER_CRITICAL(&trava);
    Estado e = saida;
    portEXIT_CRITICAL(&trava);
    return e;
}
} // namespace Navegacao
