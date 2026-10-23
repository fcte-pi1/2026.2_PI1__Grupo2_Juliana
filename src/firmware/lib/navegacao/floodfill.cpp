#include "floodfill.h"

#include <string.h>

static const Direcao TODAS[] = {NORTE, LESTE, SUL, OESTE};

// Busca em largura a partir do objetivo: cada vizinho alcançável recebe a
// distância da célula de onde veio + 1.
void FloodFill::recalcular(const Labirinto &lab, ModoFlood modo)
{
    modo_ = modo;
    memset(dist_, INFINITO, sizeof(dist_));

    // Cada célula entra na fila uma vez só, então 16x16 posições bastam.
    uint8_t filaX[MAX_LARG * MAX_ALT];
    uint8_t filaY[MAX_LARG * MAX_ALT];
    uint16_t inicio = 0;
    uint16_t fim = 0;

    for (uint8_t x = 0; x < lab.largura(); x++)
        for (uint8_t y = 0; y < lab.altura(); y++)
            if (lab.ehObjetivo(x, y))
            {
                dist_[x][y] = 0;
                filaX[fim] = x;
                filaY[fim] = y;
                fim++;
            }

    while (inicio < fim)
    {
        uint8_t x = filaX[inicio];
        uint8_t y = filaY[inicio];
        inicio++;
        for (Direcao d : TODAS)
        {
            if (!passavel(lab, x, y, d))
                continue;
            uint8_t vizinhoX = x + deslocamentoX(d);
            uint8_t vizinhoY = y + deslocamentoY(d);
            if (dist_[vizinhoX][vizinhoY] != INFINITO)
                continue;
            dist_[vizinhoX][vizinhoY] = dist_[x][y] + 1;
            filaX[fim] = vizinhoX;
            filaY[fim] = vizinhoY;
            fim++;
        }
    }
}

uint8_t FloodFill::distancia(uint8_t x, uint8_t y) const
{
    if (x >= MAX_LARG || y >= MAX_ALT)
        return INFINITO;
    return dist_[x][y];
}

uint8_t FloodFill::valorAtual(const Labirinto &lab) const
{
    return distancia(lab.x(), lab.y());
}

bool FloodFill::proximaDirecao(const Labirinto &lab, Direcao &saida) const
{
    if (distancia(lab.x(), lab.y()) == INFINITO)
        return false;
    return escolher(lab, lab.x(), lab.y(), lab.orientacao(), INFINITO, saida);
}

uint8_t FloodFill::rotaMaisCurta(const Labirinto &lab, Direcao saida[], uint8_t max)
{
    recalcular(lab, RAPIDO);
    uint8_t passos = dist_[0][0];
    if (passos == INFINITO || passos > max)
        return 0;

    // Pela convenção do labirinto.h, a largada é (0,0) e a frente depende do
    // lado da borda.
    uint8_t x = 0;
    uint8_t y = 0;
    Direcao frente = lab.orientacaoInicial();
    for (uint8_t i = 0; i < passos; i++)
    {
        Direcao d;
        escolher(lab, x, y, frente, dist_[x][y], d);
        saida[i] = d;
        x += deslocamentoX(d);
        y += deslocamentoY(d);
        frente = d;
    }
    return passos;
}

bool FloodFill::rotaGarantida(const Labirinto &lab)
{
    recalcular(lab, EXPLORACAO);
    uint8_t otimista = dist_[0][0];
    recalcular(lab, RAPIDO);
    return dist_[0][0] != INFINITO && dist_[0][0] == otimista;
}

// Desconhecido conta como aberto na exploração e como fechado na rota rápida.
// As bordas já são paredes conhecidas, então o vizinho sempre existe.
bool FloodFill::passavel(const Labirinto &lab, uint8_t x, uint8_t y, Direcao d) const
{
    if (lab.temParede(x, y, d))
        return false;
    return modo_ == EXPLORACAO || lab.conhecida(x, y, d);
}

// Vizinho passável com a menor distância abaixo de limite. A ordem da lista
// decide o empate: reto, direita, esquerda e trás.
bool FloodFill::escolher(const Labirinto &lab, uint8_t x, uint8_t y, Direcao frente,
                         uint8_t limite, Direcao &saida) const
{
    const Direcao ordem[] = {frente, girarDireita(frente), girarEsquerda(frente), oposta(frente)};
    uint8_t melhor = limite;
    bool achou = false;
    for (Direcao d : ordem)
    {
        if (!passavel(lab, x, y, d))
            continue;
        uint8_t distanciaVizinho = dist_[x + deslocamentoX(d)][y + deslocamentoY(d)];
        if (distanciaVizinho < melhor)
        {
            melhor = distanciaVizinho;
            saida = d;
            achou = true;
        }
    }
    return achou;
}

// Tamanho da linha sem os espaços e o '\r' do fim.
static uint16_t tamanhoUtil(const char *linha, uint16_t tamanho)
{
    while (tamanho > 0 && (linha[tamanho - 1] == ' ' || linha[tamanho - 1] == '\r'))
        tamanho--;
    return tamanho;
}

bool carregar(const char *ascii, Labirinto &gabarito, Direcao orientacaoInicial)
{
    const uint8_t MAX_LINHAS = 2 * MAX_ALT + 1;
    const char *linhas[MAX_LINHAS];
    uint16_t tamanhos[MAX_LINHAS];
    uint8_t n = 0;
    bool acabou = false;

    const char *p = ascii;
    while (*p)
    {
        const char *inicio = p;
        while (*p && *p != '\n')
            p++;
        uint16_t tamanho = tamanhoUtil(inicio, p - inicio);
        if (*p)
            p++;

        if (tamanho == 0)
        {
            acabou = n > 0;
            continue;
        }
        if (acabou || n == MAX_LINHAS)
            return false;
        linhas[n] = inicio;
        tamanhos[n] = tamanho;
        n++;
    }

    // n = 2 * altura + 1 e cada linha tem 4 * largura + 1 colunas.
    if (n < 3 || n % 2 == 0 || tamanhos[0] < 5 || (tamanhos[0] - 1) % 4 != 0)
        return false;
    for (uint8_t i = 1; i < n; i++)
        if (tamanhos[i] != tamanhos[0])
            return false;
    uint8_t altura = (n - 1) / 2;
    uint16_t largura = (tamanhos[0] - 1) / 4;
    if (largura > MAX_LARG || !gabarito.iniciar(largura, altura, orientacaoInicial))
        return false;

    for (uint8_t linha = 0; linha < altura; linha++)
    {
        uint8_t y = altura - 1 - linha;
        const char *celulas = linhas[2 * linha + 1];
        const char *teto = linhas[2 * linha];
        for (uint8_t x = 0; x < largura; x++)
        {
            if (x < largura - 1)
            {
                if (celulas[4 * x + 4] == '|')
                    gabarito.marcarParede(x, y, LESTE);
                else
                    gabarito.marcarAberta(x, y, LESTE);
            }
            if (y < altura - 1)
            {
                if (teto[4 * x + 2] == '-')
                    gabarito.marcarParede(x, y, NORTE);
                else
                    gabarito.marcarAberta(x, y, NORTE);
            }
        }
    }
    return true;
}

// Caractere de um lado no desenho: parede, aberto ou ainda desconhecido.
static char lado(const Labirinto &lab, uint8_t x, uint8_t y, Direcao d, char parede)
{
    if (!lab.conhecida(x, y, d))
        return '.';
    return lab.temParede(x, y, d) ? parede : ' ';
}

bool imprimirMapa(const Labirinto &lab, const FloodFill &ff, char *buf, size_t tamanho)
{
    const uint8_t largura = lab.largura();
    const uint8_t altura = lab.altura();
    const size_t colunas = 4 * largura + 2;
    if (largura == 0 || tamanho < colunas * (2 * altura + 1) + 1)
        return false;

    char *p = buf;
    // Linha da parede Norte de cada fileira, do topo (y = altura - 1) para baixo,
    // e por último a borda Sul.
    for (int y = altura - 1; y >= -1; y--)
    {
        for (uint8_t x = 0; x < largura; x++)
        {
            char c = y < 0 ? '-' : lado(lab, x, y, NORTE, '-');
            *p++ = '+';
            *p++ = c == '.' ? ' ' : c;
            *p++ = c;
            *p++ = c == '.' ? ' ' : c;
        }
        *p++ = '+';
        *p++ = '\n';
        if (y < 0)
            break;

        *p++ = '|';
        for (uint8_t x = 0; x < largura; x++)
        {
            uint8_t distancia = ff.distancia(x, y);
            if (distancia == INFINITO)
            {
                *p++ = ' ';
                *p++ = '-';
            }
            else
            {
                *p++ = distancia < 10 ? ' ' : (char)('0' + distancia / 10);
                *p++ = (char)('0' + distancia % 10);
            }
            *p++ = ' ';
            *p++ = lado(lab, x, y, LESTE, '|');
        }
        *p++ = '\n';
    }
    *p = '\0';
    return true;
}
