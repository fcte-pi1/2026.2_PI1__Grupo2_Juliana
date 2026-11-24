// Controle de movimento (RF03-S, RF15-S, RF16-S) e modo automático com o robô
// físico simulado (robo_fisico_simulado.h): os critérios do roteiro de testes
// (docs/4.4.6, CT-SE05 a CT-SE08 e CT-SE13) antes de levar ao robô.
#include <initializer_list>
#include <math.h>
#include <unity.h>

#include "controle_movimento.h"
#include "desenhos_labirintos.h"
#include "floodfill.h"
#include "labirinto.h"
#include "modo_automatico.h"
#include "robo_fisico.h"
#include "robo_fisico_simulado.h"

void setUp() {}
void tearDown() {}

// CT-SE05 e CT-SE07: parar a no máximo 2 cm do centro. CT-SE06: ± 5° nos giros.
static const float ERRO_POSICAO_MM = 20;
static const float ERRO_GIRO_GRAUS = 5;

static void carregarDesenho(const char *ascii, Labirinto &gab, Direcao o = NORTE)
{
    TEST_ASSERT_TRUE(carregar(ascii, gab, o));
}

// Corredor reto de 4 células (linha de baixo do CORREDOR_RETO), robô virado para o Leste.
struct Corredor
{
    Labirinto gab;
    Corredor() { carregarDesenho(CORREDOR_RETO_ASCII, gab, LESTE); }
};

// ---------- funções soltas ----------

void test_angulos_das_direcoes_a_partir_da_largada()
{
    TEST_ASSERT_EQUAL_FLOAT(0, anguloDe(NORTE, NORTE));
    TEST_ASSERT_EQUAL_FLOAT(90, anguloDe(OESTE, NORTE));
    TEST_ASSERT_EQUAL_FLOAT(-90, anguloDe(LESTE, NORTE));
    TEST_ASSERT_EQUAL_FLOAT(180, anguloDe(SUL, NORTE));
    // Largada virada para o Leste: o Norte fica 90° à esquerda.
    TEST_ASSERT_EQUAL_FLOAT(0, anguloDe(LESTE, LESTE));
    TEST_ASSERT_EQUAL_FLOAT(90, anguloDe(NORTE, LESTE));
    TEST_ASSERT_EQUAL_FLOAT(180, anguloDe(OESTE, LESTE));
}

void test_direcao_mais_perto_do_angulo()
{
    TEST_ASSERT_EQUAL(NORTE, direcaoDe(10, NORTE));
    TEST_ASSERT_EQUAL(OESTE, direcaoDe(80, NORTE));
    TEST_ASSERT_EQUAL(SUL, direcaoDe(-170, NORTE));
    TEST_ASSERT_EQUAL(LESTE, direcaoDe(-100, NORTE));
    TEST_ASSERT_EQUAL(NORTE, direcaoDe(88, LESTE));
    TEST_ASSERT_EQUAL_FLOAT(-170, normalizarGraus(190));
    TEST_ASSERT_EQUAL_FLOAT(180, normalizarGraus(-180));
}

void test_controle_ocioso_nao_liga_os_motores()
{
    ControleMovimento c;
    EntradaMovimento e = {};
    SaidaMovimento s = c.atualizar(e);
    TEST_ASSERT_EQUAL_FLOAT(0, s.esquerdo);
    TEST_ASSERT_EQUAL_FLOAT(0, s.direito);
    TEST_ASSERT_EQUAL(MOV_OCIOSO, c.estado());
    TEST_ASSERT_EQUAL_STRING("bloqueado", nomeMovimento(MOV_BLOQUEADO));
}

void test_sem_bno_o_rumo_vem_dos_encoders()
{
    ControleMovimento c;
    EntradaMovimento e = {};
    c.reiniciar(e, 0);
    // Roda direita 1/4 de volta da circunferência do giro à frente da esquerda:
    // arco de π·largura/4 em cada roda = 90° à esquerda.
    float arco = 3.14159265f * c.parametros().larguraEixoMm / 4;
    e.encoderDireito = (int32_t)(arco * c.parametros().pulsosPorMm);
    e.encoderEsquerdo = -e.encoderDireito;
    c.atualizar(e);
    TEST_ASSERT_FLOAT_WITHIN(1, 90, c.rumo());
}

void test_com_bno_o_rumo_soma_as_variacoes_do_yaw()
{
    ControleMovimento c;
    EntradaMovimento e = {};
    e.yawOk = true;
    e.yawGraus = 30; // referência qualquer: só a variação conta
    c.reiniciar(e, 0);
    for (int i = 0; i < 9; i++) // gira 45° à esquerda aos poucos
    {
        e.yawGraus += 5;
        c.atualizar(e);
    }
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 45, c.rumo());
    // O BNO085 reinicia e volta com outra referência: o salto é ignorado.
    e.yawGraus = -120;
    c.atualizar(e);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 45, c.rumo());
    e.yawGraus = -115;
    c.atualizar(e);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 50, c.rumo());
    // Passando de 180° para -180° não é salto.
    e.yawGraus = 178;
    c.reiniciar(e, 175);
    e.yawGraus = -178;
    c.atualizar(e);
    e.yawGraus = -174;
    c.atualizar(e);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -177, c.rumo());
}

// ---------- CT-SE05: avanço célula a célula ----------

static void avancar(Corredor &k, ParametrosMovimento p, int celulas, bool yawOk = true)
{
    RoboFisicoSimulado sim(k.gab);
    sim.yawOk = yawOk;
    ControleMovimento c(p);
    RoboFisico robo(sim, c, LESTE);
    TEST_ASSERT_TRUE(robo.calibrar());
    for (int i = 0; i < celulas; i++)
        TEST_ASSERT_TRUE(robo.andarMm(CELULA_MM, 0));
    TEST_ASSERT_EQUAL(celulas, sim.celulaX());
    TEST_ASSERT_EQUAL(0, sim.celulaY());
    TEST_ASSERT_FLOAT_WITHIN(ERRO_POSICAO_MM, 0, sim.foraDoCentro());
    TEST_ASSERT_FALSE(sim.colidiu());
}

void test_avanca_uma_celula_e_para_no_centro()
{
    Corredor k;
    avancar(k, parametrosCalibrados(), 1);
}

void test_avanca_tres_celulas_e_para_no_centro()
{
    Corredor k;
    avancar(k, parametrosCalibrados(), 3);
}

void test_avanca_sem_calibracao_das_paredes_so_com_encoders()
{
    Corredor k;
    avancar(k, ParametrosMovimento(), 3);
}

void test_avanca_sem_bno_com_rumo_pelos_encoders()
{
    Corredor k;
    avancar(k, parametrosCalibrados(), 3, false);
}

// A parede no fim do corredor manda na parada: mesmo com os encoders errando
// a distância (roda de diâmetro diferente), o robô para no centro da última.
void test_parede_da_frente_corrige_o_erro_dos_encoders()
{
    Corredor k;
    RoboFisicoSimulado sim(k.gab);
    ParametrosMovimento p = parametrosCalibrados();
    p.pulsosPorMm *= 1.08f; // encoder conta 8% a mais: sozinho, pararia 1,4 cm antes por célula
    ControleMovimento c(p);
    RoboFisico robo(sim, c, LESTE);
    robo.calibrar();
    sim.colocar(2, 0, LESTE);
    robo.calibrar();
    TEST_ASSERT_TRUE(robo.andarMm(CELULA_MM, 0));
    TEST_ASSERT_TRUE(c.paradaPelaFrente());
    TEST_ASSERT_EQUAL(3, sim.celulaX());
    TEST_ASSERT_FLOAT_WITHIN(10, 0, sim.foraDoCentroX());
}

// ---------- CT-SE06: giros ----------

static void girar(float rumo, bool yawOk)
{
    Labirinto gab;
    carregarDesenho(BECO_LARGADA_ASCII, gab);
    RoboFisicoSimulado sim(gab);
    sim.yawOk = yawOk;
    sim.colocar(1, 1, NORTE); // célula aberta no meio
    ControleMovimento c(parametrosCalibrados());
    RoboFisico robo(sim, c, NORTE);
    robo.calibrar();
    TEST_ASSERT_TRUE(robo.girarPara(rumo));
    TEST_ASSERT_FLOAT_WITHIN(ERRO_GIRO_GRAUS, 0, normalizarGraus(sim.rumoVerdadeiro() - rumo));
    TEST_ASSERT_FLOAT_WITHIN(ERRO_POSICAO_MM, 0, sim.foraDoCentro());
    TEST_ASSERT_FALSE(sim.colidiu());
}

void test_gira_90_a_direita_e_a_esquerda_e_meia_volta()
{
    girar(-90, true);
    girar(90, true);
    girar(180, true);
}

void test_gira_sem_bno_pelos_encoders()
{
    girar(-90, false);
    girar(180, false);
}

// ---------- CT-SE07: robô desalinhado na largada ----------

static void corrigirDesalinhamento(float lateral, float graus)
{
    Corredor k;
    RoboFisicoSimulado sim(k.gab);
    ControleMovimento c(parametrosCalibrados());
    RoboFisico robo(sim, c, LESTE);
    robo.calibrar();
    sim.colocar(0, 0, LESTE, lateral, graus);
    for (int i = 0; i < 3; i++)
        TEST_ASSERT_TRUE(robo.andarMm(CELULA_MM, 0));
    TEST_ASSERT_EQUAL(3, sim.celulaX());
    TEST_ASSERT_FLOAT_WITHIN(ERRO_POSICAO_MM, 0, sim.erroLateralVerdadeiro());
    // Depois de corrigido, fica dentro de ± 2 cm até o fim.
    TEST_ASSERT_TRUE(sim.maiorErroLateralDepois_ <= ERRO_POSICAO_MM);
    TEST_ASSERT_FALSE(sim.colidiu());
}

void test_corrige_robo_fora_do_centro_e_girado()
{
    corrigirDesalinhamento(20, 8);   // 2 cm à direita e 8° para a esquerda
    corrigirDesalinhamento(-20, -8); // o outro lado
}

// ---------- CT-SE08: bloqueio e recuperação ----------

// Parede que o mapa não tinha: o robô não conta a célula e continua onde o mapa diz.
void test_parede_inesperada_nao_conta_como_celula_andada()
{
    Labirinto gab;
    carregarDesenho(CORREDOR_RETO_ASCII, gab, LESTE);
    gab.marcarParede(1, 0, LESTE); // parede entre (1,0) e (2,0)
    RoboFisicoSimulado sim(gab);
    ControleMovimento c(parametrosCalibrados());
    RoboFisico robo(sim, c, LESTE);
    robo.calibrar();
    TEST_ASSERT_TRUE(robo.andarMm(CELULA_MM, 0));
    TEST_ASSERT_FALSE(robo.andarMm(CELULA_MM, 0));
    TEST_ASSERT_EQUAL(MOV_BLOQUEADO, robo.ultimoResultado());
    TEST_ASSERT_EQUAL(1, sim.celulaX());
    TEST_ASSERT_FLOAT_WITHIN(ERRO_POSICAO_MM, 0, sim.foraDoCentro());
    TEST_ASSERT_FALSE(sim.colidiu());
}

// Sem calibração da frente, o robô só descobre a parede ao bater: as rodas
// travam, o controle acusa bloqueio e o robô volta de ré ao centro.
void test_colisao_trava_as_rodas_e_o_robo_volta_ao_centro()
{
    Labirinto gab;
    carregarDesenho(CORREDOR_RETO_ASCII, gab, LESTE);
    gab.marcarParede(0, 0, LESTE);
    RoboFisicoSimulado sim(gab);
    ControleMovimento c; // sem frontalCentroMm
    RoboFisico robo(sim, c, LESTE);
    robo.calibrar();
    TEST_ASSERT_FALSE(robo.andarMm(CELULA_MM, 0));
    TEST_ASSERT_EQUAL(MOV_BLOQUEADO, robo.ultimoResultado());
    TEST_ASSERT_EQUAL(1, robo.recuperacoes());
    TEST_ASSERT_EQUAL(0, sim.celulaX());
    TEST_ASSERT_FLOAT_WITHIN(ERRO_POSICAO_MM, 0, sim.foraDoCentro());
}

void test_abortar_para_o_robo_na_hora()
{
    Corredor k;
    RoboFisicoSimulado sim(k.gab);
    ControleMovimento c(parametrosCalibrados());
    RoboFisico robo(sim, c, LESTE);
    robo.calibrar();
    sim.abortarEm_ = sim.agora() + 300;
    TEST_ASSERT_FALSE(robo.andarMm(CELULA_MM, 0));
    TEST_ASSERT_FALSE(c.ocupado());
    TEST_ASSERT_FALSE(robo.mover(LESTE));
}

// ---------- paredes vistas parado no centro ----------

void test_sentir_marca_as_paredes_dos_tres_lados()
{
    Labirinto gab, mapa;
    carregarDesenho(BECO_LARGADA_ASCII, gab);
    mapa.iniciar(4, 4, NORTE);
    RoboFisicoSimulado sim(gab);
    ControleMovimento c(parametrosCalibrados());
    RoboFisico robo(sim, c, NORTE);
    robo.calibrar();
    robo.sentir(mapa);
    // Beco da largada: parede à direita (Leste), aberto à frente (Norte).
    TEST_ASSERT_TRUE(mapa.conhecida(0, 0, LESTE));
    TEST_ASSERT_TRUE(mapa.temParede(0, 0, LESTE));
    TEST_ASSERT_TRUE(mapa.conhecida(0, 0, NORTE));
    TEST_ASSERT_FALSE(mapa.temParede(0, 0, NORTE));
}

// Bancada, 23/11/2026: o giro parou a 35° (encostou) e as paredes lidas na
// diagonal entraram no mapa. Torto, sentir() não marca nada.
void test_sentir_nao_marca_paredes_com_o_robo_torto()
{
    Labirinto gab, mapa;
    carregarDesenho(BANCADA_23_11_ASCII, gab, SUL);
    mapa.iniciar(4, 4, SUL);
    RoboFisicoSimulado sim(gab);
    ControleMovimento c(parametrosCalibrados());
    RoboFisico robo(sim, c, SUL);
    robo.calibrar();
    sim.colocar(0, 0, SUL, 0, 35); // 35° fora do eixo
    c.reiniciar(sim.ler(), 35);
    robo.sentir(mapa);
    TEST_ASSERT_EQUAL(1, robo.leiturasTortas());
    TEST_ASSERT_FALSE(mapa.conhecida(0, 0, LESTE)); // a saída continua desconhecida (aberta para o Flood Fill)
    TEST_ASSERT_FALSE(mapa.conhecida(0, 0, NORTE));
}

// Sem calibração da frente: no fim do corredor, parado pelos encoders, o robô
// aprende a distância da parede da frente e passa a parar por ela.
void test_aprende_a_referencia_da_frente_na_primeira_parede()
{
    Corredor k;
    RoboFisicoSimulado sim(k.gab);
    ParametrosMovimento p = parametrosCalibrados();
    p.frontalCentroMm = 0;
    ControleMovimento c(p);
    RoboFisico robo(sim, c, LESTE);
    Labirinto mapa;
    mapa.iniciar(4, 4, LESTE);
    robo.calibrar();
    for (int i = 0; i < 3; i++)
        TEST_ASSERT_TRUE(robo.mover(LESTE));
    robo.sentir(mapa);
    TEST_ASSERT_TRUE(robo.aprendeuFrente());
    TEST_ASSERT_UINT16_WITHIN(12, (uint16_t)RoboFisicoSimulado::FRONTAL_CENTRO, c.parametros().frontalCentroMm);
}

// ---------- CT-SE13: corrida completa com o robô físico simulado ----------

static void corridaCompleta(const LabirintoTeste &f, Direcao o, bool yawOk, ParametrosMovimento p,
                            float rodaDireita = 0.93f, float larguraEixoReal = 95, float zonaMortaEsquerda = 0.12f)
{
    Labirinto gab, mapa;
    TEST_ASSERT_TRUE_MESSAGE(carregar(f.ascii, gab, o), f.nome);
    gab.definirObjetivo(f.objetivoX, f.objetivoY);
    mapa.iniciar(gab.largura(), gab.altura(), o);
    mapa.definirObjetivo(f.objetivoX, f.objetivoY);

    RoboFisicoSimulado sim(gab);
    sim.yawOk = yawOk;
    sim.ganhoRodaDireita = rodaDireita;
    sim.larguraEixoReal = larguraEixoReal;
    sim.zonaMortaEsquerda = zonaMortaEsquerda;
    ControleMovimento c(p);
    RoboFisico robo(sim, c, o);
    ModoAutomatico modo(robo, mapa);
    modo.iniciar();
    TEST_ASSERT_EQUAL_MESSAGE(ESTADO_PARADO, modo.executar(), f.nome);
    TEST_ASSERT_EQUAL_MESSAGE(SEM_FALHA, modo.motivo(), f.nome);
    // O mapa e o robô de verdade estão na mesma célula: o objetivo.
    TEST_ASSERT_EQUAL_MESSAGE(f.objetivoX, sim.celulaX(), f.nome);
    TEST_ASSERT_EQUAL_MESSAGE(f.objetivoY, sim.celulaY(), f.nome);
    TEST_ASSERT_EQUAL_MESSAGE(mapa.x(), sim.celulaX(), f.nome);
    TEST_ASSERT_EQUAL_MESSAGE(mapa.y(), sim.celulaY(), f.nome);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(ERRO_POSICAO_MM, 0, sim.foraDoCentro(), f.nome);
    TEST_ASSERT_FALSE_MESSAGE(sim.colidiu(), f.nome);
    // A rota rápida é a conferida à mão nos desenhos.
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(f.movimentosConferidos, modo.passosRapido(), f.nome);
}

void test_corrida_completa_em_todos_os_desenhos_com_o_robo_fisico()
{
    for (const auto &f : LABIRINTOS_TESTE)
    {
        corridaCompleta(f, NORTE, true, parametrosCalibrados());
        corridaCompleta(f, LESTE, true, parametrosCalibrados());
    }
}

void test_corrida_completa_sem_bno_e_sem_calibracao_das_paredes()
{
    corridaCompleta(LABIRINTOS_TESTE[0], NORTE, false, parametrosCalibrados());
    corridaCompleta(LABIRINTOS_TESTE[2], NORTE, false, parametrosCalibrados());
    corridaCompleta(LABIRINTOS_TESTE[0], NORTE, true, ParametrosMovimento());
}

// O caso esperado na bateria (BNO085 falhando, docs/7.3): rumo só pelos
// encoders, corrigido pelas paredes. Aguenta uma roda 15% mais fraca e a
// largura do eixo medida com até 2% de erro (~2 mm).
void test_corrida_completa_sem_bno_com_roda_fraca_e_eixo_medido_com_erro()
{
    for (const auto &f : LABIRINTOS_TESTE)
        for (Direcao o : {NORTE, LESTE})
        {
            corridaCompleta(f, o, false, parametrosCalibrados(), 0.85f, 93);
            corridaCompleta(f, o, false, parametrosCalibrados(), 0.85f, 97);
        }
}

// O labirinto da bancada (23/11/2026) com o robô largando como foi posto:
// virado para a borda de baixo (Sul), com a outra borda à direita.
void test_corrida_no_labirinto_da_bancada_largando_virado_para_a_borda()
{
    Labirinto teste;
    TEST_ASSERT_TRUE(teste.iniciarNaLargada(BORDA_FRENTE_DIREITA, 4, 4));
    TEST_ASSERT_EQUAL(SUL, teste.orientacaoInicial());
    corridaCompleta(BANCADA_23_11, SUL, true, parametrosCalibrados());
    corridaCompleta(BANCADA_23_11, SUL, false, parametrosCalibrados());
    corridaCompleta(BANCADA_23_11, SUL, false, ParametrosMovimento());
    corridaCompleta(BANCADA_23_11, SUL, false, parametrosCalibrados(), 0.85f, 97);
}

// Bancada, 23/11/2026: com PWM 50/255 a roda esquerda não saiu do lugar, o
// robô pivotou na direita e bateu na parede no primeiro giro. Roda esquerda
// que só anda com 0,45 (acima do máximo de 0,31): o reforço de arranque a
// destrava e o robô gira no lugar, sem encostar.
void test_roda_dura_recebe_reforco_e_o_robo_gira_no_lugar()
{
    Labirinto gab;
    carregarDesenho(BANCADA_23_11_ASCII, gab, SUL);
    RoboFisicoSimulado sim(gab);
    sim.zonaMortaEsquerda = 0.45f;
    ControleMovimento c(parametrosCalibrados());
    RoboFisico robo(sim, c, SUL);
    robo.calibrar();
    TEST_ASSERT_TRUE(robo.girarPara(anguloDe(LESTE, SUL)));
    TEST_ASSERT_FLOAT_WITHIN(ERRO_GIRO_GRAUS, anguloDe(LESTE, SUL), sim.rumoVerdadeiro());
    TEST_ASSERT_FLOAT_WITHIN(ERRO_POSICAO_MM, 0, sim.foraDoCentro());
    TEST_ASSERT_FALSE(sim.colidiu());
    TEST_ASSERT_TRUE(robo.andarMm(CELULA_MM, anguloDe(LESTE, SUL)));
    TEST_ASSERT_EQUAL(1, sim.celulaX());
    TEST_ASSERT_FALSE(sim.colidiu());
}

// Roda esquerda dura, sem BNO085 e sem calibração. Com 0,35 para sair do lugar
// (acima dos 0,31 de cruzeiro), largando para o Sul e para o Leste; com 0,45,
// largando para o Leste, como na bancada. Com 0,45 e largada para o Sul, o
// simulador às vezes encosta ou termina a mais de 2 cm do centro: nesse nível
// de atrito, só o robô mostra.
void test_corrida_na_bancada_com_a_roda_esquerda_dura()
{
    corridaCompleta(BANCADA_23_11, SUL, false, ParametrosMovimento(), 0.93f, 95, 0.35f);
    corridaCompleta(BANCADA_23_11, LESTE, false, ParametrosMovimento(), 0.93f, 95, 0.35f);
    corridaCompleta(BANCADA_23_11, LESTE, false, ParametrosMovimento(), 0.93f, 95, 0.45f);
}

// Sem reforço (o que aconteceu na bancada), girar com a frente perto da parede
// para antes de bater: o sensor da frente vigia o giro.
void test_giro_que_pivota_para_antes_de_bater_na_parede()
{
    Labirinto gab;
    carregarDesenho(BANCADA_23_11_ASCII, gab, SUL);
    RoboFisicoSimulado sim(gab);
    sim.zonaMortaEsquerda = 2; // roda esquerda não anda nunca
    ParametrosMovimento p;
    p.potenciaArranqueMaxima = p.potenciaMaxima; // sem reforço
    ControleMovimento c(p);
    RoboFisico robo(sim, c, SUL);
    robo.calibrar();
    TEST_ASSERT_FALSE(robo.girarPara(anguloDe(LESTE, SUL)));
    TEST_ASSERT_FALSE(sim.colidiu());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_angulos_das_direcoes_a_partir_da_largada);
    RUN_TEST(test_direcao_mais_perto_do_angulo);
    RUN_TEST(test_controle_ocioso_nao_liga_os_motores);
    RUN_TEST(test_sem_bno_o_rumo_vem_dos_encoders);
    RUN_TEST(test_com_bno_o_rumo_soma_as_variacoes_do_yaw);
    RUN_TEST(test_avanca_uma_celula_e_para_no_centro);
    RUN_TEST(test_avanca_tres_celulas_e_para_no_centro);
    RUN_TEST(test_avanca_sem_calibracao_das_paredes_so_com_encoders);
    RUN_TEST(test_avanca_sem_bno_com_rumo_pelos_encoders);
    RUN_TEST(test_parede_da_frente_corrige_o_erro_dos_encoders);
    RUN_TEST(test_gira_90_a_direita_e_a_esquerda_e_meia_volta);
    RUN_TEST(test_gira_sem_bno_pelos_encoders);
    RUN_TEST(test_corrige_robo_fora_do_centro_e_girado);
    RUN_TEST(test_parede_inesperada_nao_conta_como_celula_andada);
    RUN_TEST(test_colisao_trava_as_rodas_e_o_robo_volta_ao_centro);
    RUN_TEST(test_abortar_para_o_robo_na_hora);
    RUN_TEST(test_sentir_marca_as_paredes_dos_tres_lados);
    RUN_TEST(test_sentir_nao_marca_paredes_com_o_robo_torto);
    RUN_TEST(test_aprende_a_referencia_da_frente_na_primeira_parede);
    RUN_TEST(test_corrida_completa_em_todos_os_desenhos_com_o_robo_fisico);
    RUN_TEST(test_corrida_completa_sem_bno_e_sem_calibracao_das_paredes);
    RUN_TEST(test_corrida_completa_sem_bno_com_roda_fraca_e_eixo_medido_com_erro);
    RUN_TEST(test_corrida_no_labirinto_da_bancada_largando_virado_para_a_borda);
    RUN_TEST(test_roda_dura_recebe_reforco_e_o_robo_gira_no_lugar);
    RUN_TEST(test_corrida_na_bancada_com_a_roda_esquerda_dura);
    RUN_TEST(test_giro_que_pivota_para_antes_de_bater_na_parede);
    return UNITY_END();
}
