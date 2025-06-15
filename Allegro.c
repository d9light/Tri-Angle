#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_primitives.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

#define LARGURA_TELA 1512
#define ALTURA_TELA 1024
#define NUM_SLOTS 3
#define MAX_HISTORICO 1000

typedef enum {
    ESTADO_MENU,
    ESTADO_JOGAR,
    ESTADO_JOGAR_PVP,
    ESTADO_JOGAR_PVB,
    ESTADO_COMO_JOGAR,
    ESTADO_HISTORICO,
    ESTADO_PAUSA,
    ESTADO_SELECAO_SLOT,
} EstadoTela;

typedef struct {
    int x, y, largura, altura;
} Retangulo;

typedef struct {
    int x, y;
    int jogador;
    bool selecionado;
    char posicao[3];
    float escala;
    float rotacao;
    bool animando;
    int frame_animacao;
} Ponto;

typedef struct {
    int origem, destino;
    bool eh_pulo;
} MovimentoPossivel;

typedef struct {
    char modo[20];
    int vencedor;
    int tempo_segundos;
    char data[20];
} RegistroHistorico;

typedef struct {
    int partidas_pvp;
    int vitorias_p1_pvp;
    int vitorias_p2_pvp;
    int empates_pvp;
    int partidas_pvb;
    int vitorias_jogador_pvb;
    int vitorias_bot_pvb;
    int empates_pvb;
    int menor_tempo_pvp;
    int maior_tempo_pvp;
    int menor_tempo_pvb;
    int maior_tempo_pvb;
    RegistroHistorico registros[MAX_HISTORICO];
    int total_registros;
} Historico;

bool vitoria_ativa = false;
bool derrota_ativa = false;
bool empate_ativo = false;
int contador_delay_vitoria = 0;
int contador_fade = 0;
bool timer_pausado = false;
time_t tempo_pausado = 0;
float alpha_fade = 0.0f;
bool fade_out_ativo = false;
bool fade_in_ativo = false;

int matriz_adjacencia[7][7] = {
    {0, 1, 1, 1, 0, 0, 0},
    {1, 0, 1, 0, 1, 0, 0},
    {1, 1, 0, 1, 0, 1, 0},
    {1, 0, 1, 0, 0, 0, 1},
    {0, 1, 0, 0, 0, 1, 0},
    {0, 0, 1, 0, 1, 0, 1},
    {0, 0, 0, 1, 0, 1, 0}
};

bool dentro_do_retangulo(int px, int py, Retangulo r) {
    return (px >= r.x && px <= r.x + r.largura &&
            py >= r.y && py <= r.y + r.altura);
}

bool clicou_no_ponto(int px, int py, Ponto ponto) {
    int distancia = sqrt(pow(px - ponto.x, 2) + pow(py - ponto.y, 2));
    return distancia <= 30;
}

bool verifica_vitoria(Ponto pontos[], int jogador) {
    int linhas_vitoria[5][3] = {
        {1, 2, 3},
        {4, 5, 6},
        {0, 1, 4},
        {0, 2, 5},
        {0, 3, 6}
    };

    for (int i = 0; i < 5; i++) {
        if (pontos[linhas_vitoria[i][0]].jogador == jogador &&
            pontos[linhas_vitoria[i][1]].jogador == jogador &&
            pontos[linhas_vitoria[i][2]].jogador == jogador) {
            return true;
        }
    }
    return false;
}

bool verifica_empate(Ponto pontos[], int jogadas_sem_progresso) {
    return jogadas_sem_progresso > 50;
}

int contar_bolas_jogador(Ponto pontos[], int jogador) {
    int count = 0;
    for (int i = 0; i < 7; i++) {
        if (pontos[i].jogador == jogador) {
            count++;
        }
    }
    return count;
}

int obter_movimentos_possiveis(Ponto pontos[], int jogador, MovimentoPossivel movimentos[]) {
    int count = 0;

    for (int i = 0; i < 7; i++) {
        if (pontos[i].jogador == jogador) {
            for (int j = 0; j < 7; j++) {
                if (pontos[j].jogador == 0) {
                    if (matriz_adjacencia[i][j]) {
                        movimentos[count].origem = i;
                        movimentos[count].destino = j;
                        movimentos[count].eh_pulo = false;
                        count++;
                    }

                    for (int k = 0; k < 7; k++) {
                        if (pontos[k].jogador != 0 && matriz_adjacencia[i][k] && matriz_adjacencia[k][j]) {
                            bool eh_linha_reta = false;
                            if ((i == 0 && k == 1 && j == 4) || (i == 0 && k == 2 && j == 5) ||
                                (i == 0 && k == 3 && j == 6) || (i == 1 && k == 2 && j == 3) ||
                                (i == 4 && k == 5 && j == 6)) {
                                eh_linha_reta = true;
                            }

                            if (eh_linha_reta) {
                                movimentos[count].origem = i;
                                movimentos[count].destino = j;
                                movimentos[count].eh_pulo = true;
                                count++;
                            }
                        }
                    }
                }
            }
        }
    }
    return count;
}

bool movimento_valido(Ponto pontos[], int origem, int destino, bool *eh_pulo) {
    if (origem < 0 || origem >= 7 || destino < 0 || destino >= 7) return false;
    if (pontos[destino].jogador != 0) return false;

    if (matriz_adjacencia[origem][destino]) {
        *eh_pulo = false;
        return true;
    }

    for (int k = 0; k < 7; k++) {
        if (pontos[k].jogador != 0 && matriz_adjacencia[origem][k] && matriz_adjacencia[k][destino]) {
            bool eh_linha_reta = false;

            if ((origem == 1 && k == 2 && destino == 3) || (origem == 3 && k == 2 && destino == 1) ||
                (origem == 4 && k == 5 && destino == 6) || (origem == 6 && k == 5 && destino == 4) ||
                (origem == 0 && k == 2 && destino == 5) || (origem == 5 && k == 2 && destino == 0) ||
                (origem == 0 && k == 1 && destino == 4) || (origem == 4 && k == 1 && destino == 0) ||
                (origem == 0 && k == 3 && destino == 6) || (origem == 6 && k == 3 && destino == 0)) {
                eh_linha_reta = true;
            }

            if (eh_linha_reta) {
                *eh_pulo = true;
                return true;
            }
        }
    }

    return false;
}

void reiniciar_jogo(Ponto pontos[], int *jogador_atual, int *ponto_selecionado, bool *fase_colocacao,
                    int *bolas_colocadas, int *jogadas_sem_progresso, time_t *inicio_partida) {
    for (int i = 0; i < 7; i++) {
        pontos[i].jogador = 0;
        pontos[i].selecionado = false;
        pontos[i].escala = 1.0f;
        pontos[i].rotacao = 0.0f;
        pontos[i].animando = false;
        pontos[i].frame_animacao = 0;
    }
    *jogador_atual = 1;
    *ponto_selecionado = -1;
    *fase_colocacao = true;
    *bolas_colocadas = 0;
    *jogadas_sem_progresso = 0;
    *inicio_partida = time(NULL);

    vitoria_ativa = false;
    derrota_ativa = false;
    empate_ativo = false;
    contador_delay_vitoria = 0;
    contador_fade = 0;
    timer_pausado = false;
    alpha_fade = 0.0f;
    fade_out_ativo = false;
    fade_in_ativo = false;
}

bool salvar_historico(Historico *hist) {
    FILE *f = fopen("historico.dat", "wb");
    if (!f) return false;
    fwrite(hist, sizeof(Historico), 1, f);
    fclose(f);
    return true;
}

bool carregar_historico(Historico *hist) {
    FILE *f = fopen("historico.dat", "rb");
    if (!f) {
        memset(hist, 0, sizeof(Historico));
        hist->menor_tempo_pvp = 999999;
        hist->menor_tempo_pvb = 999999;
        return false;
    }
    fread(hist, sizeof(Historico), 1, f);
    fclose(f);
    return true;
}

void adicionar_registro_historico(Historico *hist, const char *modo, int vencedor, int tempo_segundos) {
    if (hist->total_registros < MAX_HISTORICO) {
        strcpy(hist->registros[hist->total_registros].modo, modo);
        hist->registros[hist->total_registros].vencedor = vencedor;
        hist->registros[hist->total_registros].tempo_segundos = tempo_segundos;

        time_t agora = time(NULL);
        struct tm *tm_info = localtime(&agora);
        strftime(hist->registros[hist->total_registros].data, 20, "%d/%m/%Y %H:%M", tm_info);
        hist->total_registros++;
    }

    if (strcmp(modo, "PvP") == 0) {
        hist->partidas_pvp++;
        if (vencedor == 1) hist->vitorias_p1_pvp++;
        else if (vencedor == 2) hist->vitorias_p2_pvp++;
        else hist->empates_pvp++;
        if (tempo_segundos < hist->menor_tempo_pvp) hist->menor_tempo_pvp = tempo_segundos;
        if (tempo_segundos > hist->maior_tempo_pvp) hist->maior_tempo_pvp = tempo_segundos;
    } else {
        hist->partidas_pvb++;
        if (vencedor == 1) hist->vitorias_jogador_pvb++;
        else if (vencedor == 2) hist->vitorias_bot_pvb++;
        else hist->empates_pvb++;
        if (tempo_segundos < hist->menor_tempo_pvb) hist->menor_tempo_pvb = tempo_segundos;
        if (tempo_segundos > hist->maior_tempo_pvb) hist->maior_tempo_pvb = tempo_segundos;
    }
    salvar_historico(hist);
}

bool salvar_jogo(const char *filename, Ponto pontos[], int jogador_atual, int ponto_selecionado, bool fase_colocacao,
                 int bolas_colocadas, int jogadas_sem_progresso, time_t inicio_partida, const char *modo) {
    FILE *f = fopen(filename, "wb");
    if (!f) return false;

    int versao = 2;
    fwrite(&versao, sizeof(int), 1, f);
    fwrite(&jogador_atual, sizeof(int), 1, f);
    fwrite(&ponto_selecionado, sizeof(int), 1, f);
    fwrite(&fase_colocacao, sizeof(bool), 1, f);
    fwrite(&bolas_colocadas, sizeof(int), 1, f);
    fwrite(&jogadas_sem_progresso, sizeof(int), 1, f);
    fwrite(&inicio_partida, sizeof(time_t), 1, f);

    int len_modo = strlen(modo);
    fwrite(&len_modo, sizeof(int), 1, f);
    fwrite(modo, sizeof(char), len_modo, f);

    for (int i = 0; i < 7; i++) {
        fwrite(&pontos[i], sizeof(Ponto), 1, f);
    }

    int checksum = jogador_atual + ponto_selecionado + bolas_colocadas + jogadas_sem_progresso;
    fwrite(&checksum, sizeof(int), 1, f);
    fclose(f);
    return true;
}

bool carregar_jogo(const char *filename, Ponto pontos[], int *jogador_atual, int *ponto_selecionado,
                   bool *fase_colocacao, int *bolas_colocadas, int *jogadas_sem_progresso, time_t *inicio_partida,
                   char *modo) {
    FILE *f = fopen(filename, "rb");
    if (!f) return false;

    int versao;
    fread(&versao, sizeof(int), 1, f);
    if (versao != 2) {
        fclose(f);
        return false;
    }

    fread(jogador_atual, sizeof(int), 1, f);
    fread(ponto_selecionado, sizeof(int), 1, f);
    fread(fase_colocacao, sizeof(bool), 1, f);
    fread(bolas_colocadas, sizeof(int), 1, f);
    fread(jogadas_sem_progresso, sizeof(int), 1, f);
    fread(inicio_partida, sizeof(time_t), 1, f);

    int len_modo;
    fread(&len_modo, sizeof(int), 1, f);
    fread(modo, sizeof(char), len_modo, f);
    modo[len_modo] = '\0';

    for (int i = 0; i < 7; i++) {
        fread(&pontos[i], sizeof(Ponto), 1, f);
    }

    int checksum_esperado, checksum_real;
    fread(&checksum_esperado, sizeof(int), 1, f);
    checksum_real = *jogador_atual + *ponto_selecionado + *bolas_colocadas + *jogadas_sem_progresso;
    fclose(f);
    return (checksum_esperado == checksum_real);
}

bool salvar_jogo_slot(int slot, Ponto pontos[], int jogador_atual, int ponto_selecionado, bool fase_colocacao,
                      int bolas_colocadas, int jogadas_sem_progresso, time_t inicio_partida, const char *modo) {
    char filename[20];
    sprintf(filename, "savegame%d.dat", slot);
    return salvar_jogo(filename, pontos, jogador_atual, ponto_selecionado, fase_colocacao, bolas_colocadas,
                       jogadas_sem_progresso, inicio_partida, modo);
}

bool carregar_jogo_slot(int slot, Ponto pontos[], int *jogador_atual, int *ponto_selecionado, bool *fase_colocacao,
                        int *bolas_colocadas, int *jogadas_sem_progresso, time_t *inicio_partida, char *modo) {
    char filename[20];
    sprintf(filename, "savegame%d.dat", slot);
    return carregar_jogo(filename, pontos, jogador_atual, ponto_selecionado, fase_colocacao, bolas_colocadas,
                         jogadas_sem_progresso, inicio_partida, modo);
}

int avaliar_posicao(Ponto pontos[], int jogador) {
    if (verifica_vitoria(pontos, jogador)) return 1000;
    if (verifica_vitoria(pontos, 3 - jogador)) return -1000;

    int score = 0;
    int linhas_vitoria[5][3] = {
        {1, 2, 3}, {4, 5, 6}, {0, 1, 4},
        {0, 2, 5}, {0, 3, 6}
    };

    for (int i = 0; i < 5; i++) {
        int minha_count = 0, oponente_count = 0;
        for (int j = 0; j < 3; j++) {
            if (pontos[linhas_vitoria[i][j]].jogador == jogador) minha_count++;
            else if (pontos[linhas_vitoria[i][j]].jogador == 3 - jogador) oponente_count++;
        }
        if (oponente_count == 0) score += minha_count * minha_count;
        if (minha_count == 0) score -= oponente_count * oponente_count;
    }
    return score;
}

void faz_jogada_bot(Ponto pontos[], int *jogador_atual, int *ponto_selecionado, bool *fase_colocacao,
                    int *bolas_colocadas, int *jogadas_sem_progresso) {
    if (*fase_colocacao) {
        int melhor_pos = -1;
        int melhor_score = -9999;

        for (int i = 0; i < 7; i++) {
            if (pontos[i].jogador == 0) {
                pontos[i].jogador = 2;
                int score = avaliar_posicao(pontos, 2);
                if (score > melhor_score) {
                    melhor_score = score;
                    melhor_pos = i;
                }
                pontos[i].jogador = 0;
            }
        }

        if (melhor_pos != -1) {
            pontos[melhor_pos].jogador = *jogador_atual;
            pontos[melhor_pos].animando = true;
            (*bolas_colocadas)++;
            if (*bolas_colocadas == 6) {
                *fase_colocacao = false;
            }
            *jogador_atual = (*jogador_atual == 1) ? 2 : 1;
        }
    } else {
        MovimentoPossivel movimentos[50];
        int num_movimentos = obter_movimentos_possiveis(pontos, 2, movimentos);

        if (num_movimentos > 0) {
            int melhor_movimento = -1;
            int melhor_score = -9999;

            for (int i = 0; i < num_movimentos; i++) {
                int origem = movimentos[i].origem;
                int destino = movimentos[i].destino;

                pontos[origem].jogador = 0;
                pontos[destino].jogador = 2;
                int score = avaliar_posicao(pontos, 2);
                if (score > melhor_score) {
                    melhor_score = score;
                    melhor_movimento = i;
                }
                pontos[destino].jogador = 0;
                pontos[origem].jogador = 2;
            }

            if (melhor_movimento != -1) {
                int origem = movimentos[melhor_movimento].origem;
                int destino = movimentos[melhor_movimento].destino;
                pontos[origem].jogador = 0;
                pontos[destino].jogador = 2;
                pontos[destino].animando = true;
                (*jogadas_sem_progresso)++;
            }
        }
        *jogador_atual = 1;
    }
}

void pausar_timer_jogo(time_t *inicio_partida) {
    if (!timer_pausado) {
        tempo_pausado = time(NULL);
        timer_pausado = true;
    }
}

void retomar_timer_jogo(time_t *inicio_partida) {
    if (timer_pausado) {
        time_t tempo_atual = time(NULL);
        time_t tempo_pausado_total = tempo_atual - tempo_pausado;
        *inicio_partida += tempo_pausado_total;
        timer_pausado = false;
    }
}

void desenhar_fade(float alpha, ALLEGRO_COLOR cor) {
    al_draw_filled_rectangle(0, 0, LARGURA_TELA, ALTURA_TELA,
                             al_map_rgba_f(cor.r, cor.g, cor.b, alpha));
}

void desenhar_mensagem_derrota(ALLEGRO_FONT *fonte_titulo) {
    al_draw_filled_rectangle(0, 0, LARGURA_TELA, ALTURA_TELA, al_map_rgba(0, 0, 0, 180));
    float escala_derrota = 1.0f + 0.2f * sin(al_get_time() * 3.0f);

    ALLEGRO_TRANSFORM transform;
    al_identity_transform(&transform);
    al_translate_transform(&transform, -LARGURA_TELA / 2, -ALTURA_TELA / 2);
    al_scale_transform(&transform, escala_derrota, escala_derrota);
    al_translate_transform(&transform, LARGURA_TELA / 2, ALTURA_TELA / 2);
    al_use_transform(&transform);

    al_draw_text(fonte_titulo, al_map_rgb(0, 0, 0), LARGURA_TELA / 2 + 3, ALTURA_TELA / 2 + 3,
                 ALLEGRO_ALIGN_CENTER, "DERROTA");
    al_draw_text(fonte_titulo, al_map_rgb(255, 50, 50), LARGURA_TELA / 2, ALTURA_TELA / 2,
                 ALLEGRO_ALIGN_CENTER, "DERROTA");

    al_identity_transform(&transform);
    al_use_transform(&transform);
    al_draw_text(al_create_builtin_font(), al_map_rgb(255, 255, 255), LARGURA_TELA / 2, ALTURA_TELA / 2 + 100,
                 ALLEGRO_ALIGN_CENTER, "Clique para continuar");
}

void desenhar_mensagem_vitoria(ALLEGRO_FONT *fonte_titulo, int jogador_vencedor) {
    al_draw_filled_rectangle(0, 0, LARGURA_TELA, ALTURA_TELA, al_map_rgba(0, 0, 0, 180));
    float escala_vitoria = 1.0f + 0.3f * sin(al_get_time() * 4.0f);

    ALLEGRO_TRANSFORM transform;
    al_identity_transform(&transform);
    al_translate_transform(&transform, -LARGURA_TELA / 2, -ALTURA_TELA / 2);
    al_scale_transform(&transform, escala_vitoria, escala_vitoria);
    al_translate_transform(&transform, LARGURA_TELA / 2, ALTURA_TELA / 2);
    al_use_transform(&transform);

    al_draw_text(fonte_titulo, al_map_rgb(0, 0, 0), LARGURA_TELA / 2 + 3, ALTURA_TELA / 2 + 3,
                 ALLEGRO_ALIGN_CENTER, "VITORIA!");
    al_draw_text(fonte_titulo, al_map_rgb(255, 215, 0), LARGURA_TELA / 2, ALTURA_TELA / 2,
                 ALLEGRO_ALIGN_CENTER, "VITORIA!");

    al_identity_transform(&transform);
    al_use_transform(&transform);
    desenhar_particulas(LARGURA_TELA / 2, ALTURA_TELA / 2, 50, al_map_rgb(255, 215, 0));
    al_draw_text(al_create_builtin_font(), al_map_rgb(255, 255, 255), LARGURA_TELA / 2, ALTURA_TELA / 2 + 100,
                 ALLEGRO_ALIGN_CENTER, "Clique para continuar");
}

void desenhar_mensagem_empate(ALLEGRO_FONT *fonte_titulo) {
    al_draw_filled_rectangle(0, 0, LARGURA_TELA, ALTURA_TELA, al_map_rgba(0, 0, 0, 180));
    float escala_empate = 1.0f + 0.2f * sin(al_get_time() * 3.0f);

    ALLEGRO_TRANSFORM transform;
    al_identity_transform(&transform);
    al_translate_transform(&transform, -LARGURA_TELA / 2, -ALTURA_TELA / 2);
    al_scale_transform(&transform, escala_empate, escala_empate);
    al_translate_transform(&transform, LARGURA_TELA / 2, ALTURA_TELA / 2);
    al_use_transform(&transform);

    al_draw_text(fonte_titulo, al_map_rgb(0, 0, 0), LARGURA_TELA / 2 + 3, ALTURA_TELA / 2 + 3,
                 ALLEGRO_ALIGN_CENTER, "EMPATE!");
    al_draw_text(fonte_titulo, al_map_rgb(255, 165, 0), LARGURA_TELA / 2, ALTURA_TELA / 2,
                 ALLEGRO_ALIGN_CENTER, "EMPATE!");

    al_identity_transform(&transform);
    al_use_transform(&transform);
    al_draw_text(al_create_builtin_font(), al_map_rgb(255, 255, 255), LARGURA_TELA / 2, ALTURA_TELA / 2 + 100,
                 ALLEGRO_ALIGN_CENTER, "Clique para continuar");
}

void processar_vitoria_derrota(int vencedor, const char *modo_jogo, EstadoTela *estado, time_t *inicio_partida,
                               Historico *historico) {
    if (vencedor != 0 && !vitoria_ativa && !derrota_ativa && !empate_ativo) {
        pausar_timer_jogo(inicio_partida);

        if (vencedor == -1) {
            empate_ativo = true;
        } else if (strcmp(modo_jogo, "PvB") == 0) {
            if (vencedor == 1) {
                vitoria_ativa = true;
                contador_delay_vitoria = 0;
                fade_out_ativo = true;
                contador_fade = 0;
                alpha_fade = 0.0f;
            } else if (vencedor == 2) {
                derrota_ativa = true;
            }
        } else {
            vitoria_ativa = true;
            contador_delay_vitoria = 0;
            fade_out_ativo = true;
            contador_fade = 0;
            alpha_fade = 0.0f;
        }

        time_t fim_partida = time(NULL);
        int tempo_partida = (int) difftime(fim_partida, *inicio_partida);
        adicionar_registro_historico(historico, modo_jogo, vencedor, tempo_partida);
    }
}
void atualizar_estados_vitoria_derrota(EstadoTela *estado, Ponto pontos[], int *jogador_atual, int *ponto_selecionado,
                                       bool *fase_colocacao, int *bolas_colocadas, int *jogadas_sem_progresso,
                                       time_t *inicio_partida, int *vencedor) {
}
void processar_clique_vitoria_derrota(int mx, int my, EstadoTela *estado, Ponto pontos[], int *jogador_atual,
                                      int *ponto_selecionado, bool *fase_colocacao, int *bolas_colocadas,
                                      int *jogadas_sem_progresso, time_t *inicio_partida, int *vencedor) {
    if (vitoria_ativa) {
        *estado = ESTADO_JOGAR;
        vitoria_ativa = false;
        reiniciar_jogo(pontos, jogador_atual, ponto_selecionado,
                       fase_colocacao, bolas_colocadas, jogadas_sem_progresso,
                       inicio_partida);
        *vencedor = 0;
        retomar_timer_jogo(inicio_partida);
    }
    if (derrota_ativa) {
        *estado = ESTADO_JOGAR;
        derrota_ativa = false;
        reiniciar_jogo(pontos, jogador_atual, ponto_selecionado,
                       fase_colocacao, bolas_colocadas, jogadas_sem_progresso,
                       inicio_partida);
        *vencedor = 0;
        retomar_timer_jogo(inicio_partida);
    }
    if (empate_ativo) {
        *estado = ESTADO_JOGAR;
        empate_ativo = false;
        reiniciar_jogo(pontos, jogador_atual, ponto_selecionado,
                       fase_colocacao, bolas_colocadas, jogadas_sem_progresso,
                       inicio_partida);
        *vencedor = 0;
        retomar_timer_jogo(inicio_partida);
    }
}
void renderizar_efeitos_vitoria_derrota(ALLEGRO_FONT *fonte_titulo, int vencedor) {
    if (vitoria_ativa) {
        desenhar_mensagem_vitoria(fonte_titulo, vencedor);
    }
    if (derrota_ativa) {
        desenhar_mensagem_derrota(fonte_titulo);
    }
    if (empate_ativo) {
        desenhar_mensagem_empate(fonte_titulo);
    }
}

void desenhar_painel_cartoon(int x, int y, int largura, int altura, ALLEGRO_COLOR cor_fundo, ALLEGRO_COLOR cor_borda) {
    al_draw_filled_rounded_rectangle(x + 4, y + 4, x + largura + 4, y + altura + 4, 8, 8,
                                    al_map_rgba(0, 0, 0, 120));

    al_draw_filled_rounded_rectangle(x, y, x + largura, y + altura, 8, 8, cor_fundo);

    al_draw_rounded_rectangle(x, y, x + largura, y + altura, 8, 8, cor_borda, 4);
    al_draw_rounded_rectangle(x + 2, y + 2, x + largura - 2, y + altura - 2, 6, 6,
                             al_map_rgba(255, 255, 255, 100), 2);
}

void desenhar_tabuleiro_cartoon(Ponto pontos[], int jogador_atual, bool fase_colocacao,
                               int ponto_selecionado, int bolas_colocadas, ALLEGRO_FONT* fonte_menor) {

    int centerX = LARGURA_TELA / 2;
    int centerY = ALTURA_TELA / 2 + 150;
    Ponto pontos_novos[7] = {
        {centerX, centerY - 280, 0, false, "A1", 1.0f, 0.0f, false, 0},
        {centerX - 280, centerY - 80, 0, false, "B1", 1.0f, 0.0f, false, 0},
        {centerX, centerY - 80, 0, false, "B2", 1.0f, 0.0f, false, 0},
        {centerX + 280, centerY - 80, 0, false, "B3", 1.0f, 0.0f, false, 0},
        {centerX - 420, centerY + 160, 0, false, "C1", 1.0f, 0.0f, false, 0},
        {centerX, centerY + 160, 0, false, "C2", 1.0f, 0.0f, false, 0},
        {centerX + 420, centerY + 160, 0, false, "C3", 1.0f, 0.0f, false, 0}
    };

    for (int i = 0; i < 7; i++) {
        pontos_novos[i].jogador = pontos[i].jogador;
        pontos_novos[i].selecionado = pontos[i].selecionado;
        pontos_novos[i].escala = pontos[i].escala;
        pontos_novos[i].rotacao = pontos[i].rotacao;
        pontos_novos[i].animando = pontos[i].animando;
        pontos_novos[i].frame_animacao = pontos[i].frame_animacao;
    }

    ALLEGRO_COLOR cor_linha = al_map_rgb(139, 69, 19);
    float espessura = 8.0f;

    al_draw_line(pontos_novos[0].x, pontos_novos[0].y, pontos_novos[1].x, pontos_novos[1].y, cor_linha, espessura);
    al_draw_line(pontos_novos[0].x, pontos_novos[0].y, pontos_novos[2].x, pontos_novos[2].y, cor_linha, espessura);
    al_draw_line(pontos_novos[0].x, pontos_novos[0].y, pontos_novos[3].x, pontos_novos[3].y, cor_linha, espessura);
    al_draw_line(pontos_novos[1].x, pontos_novos[1].y, pontos_novos[2].x, pontos_novos[2].y, cor_linha, espessura);
    al_draw_line(pontos_novos[2].x, pontos_novos[2].y, pontos_novos[3].x, pontos_novos[3].y, cor_linha, espessura);
    al_draw_line(pontos_novos[1].x, pontos_novos[1].y, pontos_novos[4].x, pontos_novos[4].y, cor_linha, espessura);
    al_draw_line(pontos_novos[2].x, pontos_novos[2].y, pontos_novos[5].x, pontos_novos[5].y, cor_linha, espessura);
    al_draw_line(pontos_novos[3].x, pontos_novos[3].y, pontos_novos[6].x, pontos_novos[6].y, cor_linha, espessura);
    al_draw_line(pontos_novos[4].x, pontos_novos[4].y, pontos_novos[5].x, pontos_novos[5].y, cor_linha, espessura);
    al_draw_line(pontos_novos[5].x, pontos_novos[5].y, pontos_novos[6].x, pontos_novos[6].y, cor_linha, espessura);

    for (int i = 0; i < 7; i++) {
        ALLEGRO_COLOR cor_ponto, cor_borda;
        float raio_base = 35.0f;
        float raio_atual = raio_base * pontos_novos[i].escala;
        float tempo = al_get_time();

        if (pontos_novos[i].selecionado) {
            cor_ponto = al_map_rgb(255, 255, 100);
            cor_borda = al_map_rgb(255, 140, 0);
        } else if (pontos_novos[i].jogador == 1) {
            cor_ponto = al_map_rgb(220, 20, 60);
            cor_borda = al_map_rgb(139, 0, 0);
        } else if (pontos_novos[i].jogador == 2) {
            cor_ponto = al_map_rgb(255, 165, 0);
            cor_borda = al_map_rgb(178, 34, 34);
        } else {
            cor_ponto = al_map_rgb(245, 245, 220);
            cor_borda = al_map_rgb(160, 82, 45);
        }

        al_draw_filled_circle(pontos_novos[i].x + 5, pontos_novos[i].y + 5, raio_atual,
                             al_map_rgba(0, 0, 0, 120));

        al_draw_filled_circle(pontos_novos[i].x, pontos_novos[i].y, raio_atual, cor_ponto);

        al_draw_circle(pontos_novos[i].x, pontos_novos[i].y, raio_atual, cor_borda, 6);

        al_draw_filled_circle(pontos_novos[i].x - 10, pontos_novos[i].y - 10, 6,
                             al_map_rgba(255, 255, 255, 180));

        al_draw_filled_circle(pontos_novos[i].x, pontos_novos[i].y + 55, 12,
                             al_map_rgba(139, 69, 19, 200));
        al_draw_text(fonte_menor, al_map_rgb(255, 255, 255),
                     pontos_novos[i].x, pontos_novos[i].y + 47,
                     ALLEGRO_ALIGN_CENTER, pontos_novos[i].posicao);

        if (pontos_novos[i].selecionado) {
            float alpha_pulso = 0.6f + 0.4f * sin(tempo * 6.0f);
            al_draw_circle(pontos_novos[i].x, pontos_novos[i].y, raio_atual + 18,
                          al_map_rgba_f(1, 1, 0, alpha_pulso), 8);
        }
    }

    for (int i = 0; i < 7; i++) {
        pontos[i].x = pontos_novos[i].x;
        pontos[i].y = pontos_novos[i].y;
    }
}

void desenhar_interface_cartoon(int jogador_atual, bool fase_colocacao, int ponto_selecionado,
                               int bolas_colocadas, Ponto pontos[], time_t inicio_partida,
                               const char* modo_jogo, int vencedor, ALLEGRO_FONT* fonte, ALLEGRO_FONT* fonte_menor, int jogadas_sem_progresso) {

    int centerX = LARGURA_TELA / 2;

    int painel_modo_y = 20;
    int painel_modo_altura = 50;
    int painel_modo_largura = 600;
    int painel_modo_x = centerX - painel_modo_largura / 2;

    desenhar_painel_cartoon(painel_modo_x, painel_modo_y, painel_modo_largura, painel_modo_altura,
                           al_map_rgb(255, 140, 0), al_map_rgb(139, 69, 19));

    const char *modo_texto = (strcmp(modo_jogo, "PvP") == 0) ? "PLAYER VS PLAYER" : "PLAYER VS BOT";
    al_draw_text(fonte, al_map_rgb(255, 255, 255), centerX, painel_modo_y + 15,
                 ALLEGRO_ALIGN_CENTER, modo_texto);

    int painel_status_y = 80;
    int painel_status_altura = 80;
    int painel_status_largura = 800;
    int painel_status_x = centerX - painel_status_largura / 2;

    desenhar_painel_cartoon(painel_status_x, painel_status_y, painel_status_largura, painel_status_altura,
                           al_map_rgb(210, 180, 140), al_map_rgb(139, 69, 19));

    time_t agora = time(NULL);
    int tempo_decorrido;
    if (timer_pausado) {
        tempo_decorrido = (int)difftime(tempo_pausado, inicio_partida);
    } else {
        tempo_decorrido = (int)difftime(agora, inicio_partida);
    }
    char tempo_str[50];
    sprintf(tempo_str, "TEMPO: %02d:%02d", tempo_decorrido / 60, tempo_decorrido % 60);
    al_draw_text(fonte_menor, al_map_rgb(139, 69, 19), centerX, painel_status_y + 10,
                 ALLEGRO_ALIGN_CENTER, tempo_str);

    char status_texto[200];
    ALLEGRO_COLOR cor_status;

    if (vencedor != 0) {
        if (vencedor == 1 || vencedor == 2) {
            sprintf(status_texto, "JOGADOR %d VENCEU!", vencedor);
            cor_status = al_map_rgb(255, 215, 0);
        } else {
            sprintf(status_texto, "EMPATE!");
            cor_status = al_map_rgb(255, 165, 0);
        }
    } else if (fase_colocacao) {
        sprintf(status_texto, "JOGADOR %d - COLOQUE SUA PECA (%d/3)",
                jogador_atual, contar_bolas_jogador(pontos, jogador_atual));
        cor_status = (jogador_atual == 1) ? al_map_rgb(220, 20, 60) : al_map_rgb(255, 165, 0);
    } else {
        if (ponto_selecionado == -1) {
            sprintf(status_texto, "JOGADOR %d - SELECIONE UMA PECA", jogador_atual);
        } else {
            sprintf(status_texto, "JOGADOR %d - MOVA PARA POSICAO VALIDA", jogador_atual);
        }
        cor_status = (jogador_atual == 1) ? al_map_rgb(220, 20, 60) : al_map_rgb(255, 165, 0);
    }

    al_draw_text(fonte, cor_status, centerX, painel_status_y + 40, ALLEGRO_ALIGN_CENTER, status_texto);

    int indicador_y = 200;

    int jogador1_x = centerX - 200;
    al_draw_filled_circle(jogador1_x, indicador_y, 20, al_map_rgb(220, 20, 60));
    al_draw_circle(jogador1_x, indicador_y, 20, al_map_rgb(139, 0, 0), 4);
    al_draw_text(fonte_menor, al_map_rgb(255, 255, 255), jogador1_x + 35, indicador_y - 8, 0, "JOGADOR 1");

    if (jogador_atual == 1 && vencedor == 0) {
        float alpha = 0.6f + 0.4f * sin(al_get_time() * 4.0f);
        al_draw_circle(jogador1_x, indicador_y, 30, al_map_rgba_f(1, 1, 0, alpha), 6);
    }

    int jogador2_x = centerX + 200;
    al_draw_filled_circle(jogador2_x, indicador_y, 20, al_map_rgb(255, 165, 0));
    al_draw_circle(jogador2_x, indicador_y, 20, al_map_rgb(178, 34, 34), 4);

    const char* jogador2_texto = (strcmp(modo_jogo, "PvB") == 0) ? "BOT" : "JOGADOR 2";
    al_draw_text(fonte_menor, al_map_rgb(255, 255, 255), jogador2_x - 35, indicador_y - 8, 0, jogador2_texto);

    if (jogador_atual == 2 && vencedor == 0) {
        float alpha = 0.6f + 0.4f * sin(al_get_time() * 4.0f);
        al_draw_circle(jogador2_x, indicador_y, 30, al_map_rgba_f(1, 0.6f, 0, alpha), 6);
    }

    if (fase_colocacao) {
        char contador_texto[50];
        sprintf(contador_texto, "PECAS COLOCADAS: %d/6", bolas_colocadas);
        al_draw_text(fonte_menor, al_map_rgb(139, 69, 19), centerX, indicador_y + 40,
                     ALLEGRO_ALIGN_CENTER, contador_texto);


    } else {
        MovimentoPossivel movimentos[50];
        int num_movimentos = obter_movimentos_possiveis(pontos, jogador_atual, movimentos);

        if (num_movimentos > 0 && vencedor == 0) {
            char mov_texto[100];
            sprintf(mov_texto, "MOVIMENTOS POSSIVEIS: %d", num_movimentos);
            al_draw_text(fonte_menor, al_map_rgb(34, 139, 34), centerX, indicador_y + 40,
                         ALLEGRO_ALIGN_CENTER, mov_texto);
               char movimentos_texto[50];
    sprintf(movimentos_texto, "MOVIMENTOS FEITOS: %d", jogadas_sem_progresso);
    al_draw_text(fonte_menor, al_map_rgb(139, 69, 19), centerX, indicador_y + 60,
                 ALLEGRO_ALIGN_CENTER, movimentos_texto);

        }
    }
}

void atualizar_animacoes(Ponto pontos[], int num_pontos) {
    for (int i = 0; i < num_pontos; i++) {
        if (pontos[i].animando) {
            pontos[i].frame_animacao++;
            if (pontos[i].frame_animacao < 10) {
                pontos[i].escala = 1.0f + 0.3f * sin(pontos[i].frame_animacao * 0.2f);
            } else if (pontos[i].frame_animacao < 20) {
                pontos[i].rotacao = (pontos[i].frame_animacao - 10) * 0.1f;
            } else {
                pontos[i].escala = 1.0f;
                pontos[i].rotacao = 0.0f;
                pontos[i].animando = false;
                pontos[i].frame_animacao = 0;
            }
        }

        if (pontos[i].selecionado) {
            pontos[i].escala = 1.0f + 0.1f * sin(al_get_time() * 5.0);
        }
    }
}

void desenhar_botao_animado(Retangulo botao, const char *texto, ALLEGRO_FONT *fonte, ALLEGRO_COLOR cor_botao,
                            ALLEGRO_COLOR cor_texto, bool hover) {
    float escala = hover ? 1.05f : 1.0f;
    int largura_escalada = botao.largura * escala;
    int altura_escalada = botao.altura * escala;
    int x_centro = botao.x + botao.largura / 2;
    int y_centro = botao.y + botao.altura / 2;

    ALLEGRO_COLOR cor_borda = hover ? al_map_rgb(255, 255, 255) : al_map_rgb(139, 69, 19);

    al_draw_filled_rounded_rectangle(
        x_centro - largura_escalada / 2 + 3,
        y_centro - altura_escalada / 2 + 3,
        x_centro + largura_escalada / 2 + 3,
        y_centro + altura_escalada / 2 + 3,
        10, 10, al_map_rgba(0, 0, 0, 120));

    al_draw_filled_rounded_rectangle(
        x_centro - largura_escalada / 2,
        y_centro - altura_escalada / 2,
        x_centro + largura_escalada / 2,
        y_centro + altura_escalada / 2,
        10, 10, cor_botao);

    al_draw_rounded_rectangle(
        x_centro - largura_escalada / 2,
        y_centro - altura_escalada / 2,
        x_centro + largura_escalada / 2,
        y_centro + altura_escalada / 2,
        10, 10, cor_borda, 3);

    al_draw_text(fonte, cor_texto, x_centro, y_centro - al_get_font_ascent(fonte) / 2, ALLEGRO_ALIGN_CENTER, texto);
}

bool esta_sobre_botao(int mx, int my, Retangulo botao) {
    return dentro_do_retangulo(mx, my, botao);
}

void desenhar_particulas(int x, int y, int quantidade, ALLEGRO_COLOR cor) {
    for (int i = 0; i < quantidade; i++) {
        float angulo = ((float) rand() / RAND_MAX) * 2 * ALLEGRO_PI;
        float distancia = ((float) rand() / RAND_MAX) * 30;
        float tamanho = ((float) rand() / RAND_MAX) * 3 + 1;

        int px = x + cos(angulo) * distancia;
        int py = y + sin(angulo) * distancia;

        al_draw_filled_circle(px, py, tamanho, cor);
    }
}

void desenhar_painel_com_sombra(int x, int y, int largura, int altura, ALLEGRO_COLOR cor_fundo,
                                ALLEGRO_COLOR cor_borda) {
    al_draw_filled_rectangle(x + 3, y + 3, x + largura + 3, y + altura + 3, al_map_rgba(0, 0, 0, 100));
    al_draw_filled_rectangle(x, y, x + largura, y + altura, cor_fundo);
    al_draw_rectangle(x, y, x + largura, y + altura, cor_borda, 2);
}

int main(void) {
    al_init();
    al_init_image_addon();
    al_init_ttf_addon();
    al_install_mouse();
    al_install_keyboard();
    al_init_primitives_addon();
    srand(time(NULL));

    ALLEGRO_DISPLAY *janela = al_create_display(LARGURA_TELA, ALTURA_TELA);
    if (!janela) {
        fprintf(stderr, "Falha ao criar janela.\n");
        return -1;
    }

    al_set_window_title(janela, "TRI-angle - Jogo Completo");

    ALLEGRO_BITMAP *fundo = al_load_bitmap("fundo.png");
    if (!fundo) {
        printf("Aviso: fundo.png nao encontrado, usando cor solida.\n");
    }

    ALLEGRO_BITMAP *fundo_tabuleiro = al_load_bitmap("fundo_tabuleiro.png");
    if (!fundo_tabuleiro) {
        printf("Aviso: fundo_tabuleiro.png nao encontrado, usando cor solida.\n");
    }

    ALLEGRO_BITMAP *fundo_como_jogar = al_load_bitmap("fundo_score.png");
    if (!fundo_como_jogar) {
        printf("Aviso: fundo_score.png nao encontrado, usando cor solida.\n");
    }

    ALLEGRO_FONT *fonte = al_load_ttf_font("Minecrafter.Alt.ttf", 32, 0);
    if (!fonte) {
        printf("Aviso: Minecrafter.Alt.ttf nao encontrada, usando fonte padrao.\n");
        fonte = al_create_builtin_font();
        if (!fonte) {
            fprintf(stderr, "Falha ao carregar fonte padrao.\n");
            al_destroy_display(janela);
            return -1;
        }
    }

    ALLEGRO_FONT *fonte_menor = al_load_ttf_font("Minecrafter.Alt.ttf", 24, 0);
    if (!fonte_menor) {
        printf("Aviso: Fonte menor nao encontrada, usando fonte padrao.\n");
        fonte_menor = al_create_builtin_font();
        if (!fonte_menor) {
            fprintf(stderr, "Falha ao carregar fonte menor padrao.\n");
            al_destroy_font(fonte);
            al_destroy_display(janela);
            return -1;
        }
    }

    ALLEGRO_FONT *fonte_titulo = al_load_ttf_font("Minecrafter.Alt.ttf", 48, 0);
    if (!fonte_titulo) {
        printf("Aviso: Fonte titulo nao encontrada, usando fonte padrao.\n");
        fonte_titulo = al_create_builtin_font();
        if (!fonte_titulo) {
            fprintf(stderr, "Falha ao carregar fonte titulo padrao.\n");
            al_destroy_font(fonte);
            al_destroy_font(fonte_menor);
            al_destroy_display(janela);
            return -1;
        }
    }

    ALLEGRO_EVENT_QUEUE *fila_eventos = al_create_event_queue();
    if (!fila_eventos) {
        fprintf(stderr, "Falha ao criar fila de eventos.\n");
        al_destroy_font(fonte);
        al_destroy_font(fonte_menor);
        al_destroy_font(fonte_titulo);
        al_destroy_display(janela);
        return -1;
    }

    ALLEGRO_TIMER *timer = al_create_timer(1.0 / 60.0);
    if (!timer) {
        fprintf(stderr, "Falha ao criar timer.\n");
        al_destroy_event_queue(fila_eventos);
        al_destroy_font(fonte);
        al_destroy_font(fonte_menor);
        al_destroy_font(fonte_titulo);
        al_destroy_display(janela);
        return -1;
    }

    al_register_event_source(fila_eventos, al_get_mouse_event_source());
    al_register_event_source(fila_eventos, al_get_display_event_source(janela));
    al_register_event_source(fila_eventos, al_get_keyboard_event_source());
    al_register_event_source(fila_eventos, al_get_timer_event_source(timer));

    al_start_timer(timer);

    EstadoTela estado = ESTADO_MENU;
    EstadoTela estado_anterior_pausa = ESTADO_MENU;
    bool rodando = true;
    int jogador_atual = 1;
    int ponto_selecionado = -1;
    bool fase_colocacao = true;
    int bolas_colocadas = 0;
    int vencedor = 0;
    bool modo_save = true;
    bool mostrar_mensagem_sucesso = false;
    bool mostrar_mensagem_erro = false;
    int tempo_mensagem = 0;
    int jogadas_sem_progresso = 0;
    time_t inicio_partida = time(NULL);
    char modo_jogo[20] = "PvP";
    const int TEMPO_MAX_MENSAGEM = 60;

    int mouse_x = 0, mouse_y = 0;
    bool botoes_hover[10] = {false};
    bool mostrar_particulas = false;
    int particulas_x = 0, particulas_y = 0;
    ALLEGRO_COLOR cor_particulas = al_map_rgb(255, 215, 0);
    bool animacao_vitoria_ativa = false;
    int contador_animacao_vitoria = 0;

    Historico historico;
    carregar_historico(&historico);

    int centerX = LARGURA_TELA / 2;
    int centerY = ALTURA_TELA / 2;

    Retangulo botaoJogar = {centerX - 200, centerY - 150, 400, 60};
    Retangulo botaoComoJogar = {centerX - 200, centerY - 50, 400, 60};
    Retangulo botaoCarregarJogo = {centerX - 200, centerY + 50, 400, 60};
    Retangulo botaoHistorico = {centerX - 200, centerY + 150, 400, 60};
    Retangulo botaoSair = {centerX - 200, centerY + 250, 400, 60};

    Retangulo botaoPvP = {centerX - 250, centerY - 50, 200, 60};
    Retangulo botaoPvB = {centerX + 50, centerY - 50, 200, 60};
    Retangulo botaoVoltarMenuPrincipal = {centerX - 100, centerY + 150, 200, 50};

    Retangulo botaoVoltarSelecaoModo = {centerX - 100, ALTURA_TELA - 80, 200, 50};

    Retangulo botaoContinuar = {centerX - 150, centerY - 80, 300, 50};
    Retangulo botaoReiniciarPausado = {centerX - 150, centerY - 10, 300, 50};
    Retangulo botaoMenuPrincipalPausado = {centerX - 150, centerY + 60, 300, 50};
    Retangulo botaoSalvarPausado = {centerX - 150, centerY + 130, 300, 50};

    Ponto pontos[7] = {
        {756, 128, 0, false, "A1", 1.0f, 0.0f, false, 0},
        {756 - 252, 128 + 256, 0, false, "B1", 1.0f, 0.0f, false, 0},
        {756, 128 + 256, 0, false, "B2", 1.0f, 0.0f, false, 0},
        {756 + 252, 128 + 256, 0, false, "B3", 1.0f, 0.0f, false, 0},
        {756 - 460, 128 + 512, 0, false, "C1", 1.0f, 0.0f, false, 0},
        {756, 128 + 512, 0, false, "C2", 1.0f, 0.0f, false, 0},
        {756 + 460, 128 + 512, 0, false, "C3", 1.0f, 0.0f, false, 0}
    };

    printf("Jogo inicializado com sucesso!\n");

    while (rodando) {
        ALLEGRO_EVENT evento;
        al_wait_for_event(fila_eventos, &evento);

        if (evento.type == ALLEGRO_EVENT_TIMER) {
            atualizar_animacoes(pontos, 7);
            atualizar_estados_vitoria_derrota(&estado, pontos, &jogador_atual, &ponto_selecionado,
                                              &fase_colocacao, &bolas_colocadas, &jogadas_sem_progresso,
                                              &inicio_partida, &vencedor);

            if (animacao_vitoria_ativa) {
                contador_animacao_vitoria++;
                if (contador_animacao_vitoria > 180) {
                    animacao_vitoria_ativa = false;
                    contador_animacao_vitoria = 0;
                }
            }

            for (int i = 0; i < 10; i++) {
                botoes_hover[i] = false;
            }

            if (estado == ESTADO_MENU) {
                botoes_hover[0] = esta_sobre_botao(mouse_x, mouse_y, botaoJogar);
                botoes_hover[1] = esta_sobre_botao(mouse_x, mouse_y, botaoComoJogar);
                botoes_hover[2] = esta_sobre_botao(mouse_x, mouse_y, botaoCarregarJogo);
                botoes_hover[3] = esta_sobre_botao(mouse_x, mouse_y, botaoHistorico);
                botoes_hover[4] = esta_sobre_botao(mouse_x, mouse_y, botaoSair);
            }

            if (mostrar_particulas) {
                mostrar_particulas = false;
            }
        } else if (evento.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
            rodando = false;
        } else if (evento.type == ALLEGRO_EVENT_MOUSE_AXES) {
            mouse_x = evento.mouse.x;
            mouse_y = evento.mouse.y;
        } else if (evento.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN) {
            int mx = evento.mouse.x;
            int my = evento.mouse.y;

            processar_clique_vitoria_derrota(mx, my, &estado, pontos, &jogador_atual, &ponto_selecionado,
                                             &fase_colocacao, &bolas_colocadas, &jogadas_sem_progresso,
                                             &inicio_partida, &vencedor);

            if (estado == ESTADO_MENU) {
                if (dentro_do_retangulo(mx, my, botaoJogar)) {
                    estado = ESTADO_JOGAR;
                } else if (dentro_do_retangulo(mx, my, botaoComoJogar)) {
                    estado = ESTADO_COMO_JOGAR;
                } else if (dentro_do_retangulo(mx, my, botaoCarregarJogo)) {
                    estado_anterior_pausa = estado;
                    estado = ESTADO_SELECAO_SLOT;
                    modo_save = false;
                } else if (dentro_do_retangulo(mx, my, botaoHistorico)) {
                    estado = ESTADO_HISTORICO;
                } else if (dentro_do_retangulo(mx, my, botaoSair)) {
                    rodando = false;
                }
            } else if (estado == ESTADO_JOGAR) {
                if (dentro_do_retangulo(mx, my, botaoPvP)) {
                    estado = ESTADO_JOGAR_PVP;
                    strcpy(modo_jogo, "PvP");
                    reiniciar_jogo(pontos, &jogador_atual, &ponto_selecionado, &fase_colocacao, &bolas_colocadas,
                                   &jogadas_sem_progresso, &inicio_partida);
                    vencedor = 0;
                } else if (dentro_do_retangulo(mx, my, botaoPvB)) {
                    estado = ESTADO_JOGAR_PVB;
                    strcpy(modo_jogo, "PvB");
                    reiniciar_jogo(pontos, &jogador_atual, &ponto_selecionado, &fase_colocacao, &bolas_colocadas,
                                   &jogadas_sem_progresso, &inicio_partida);
                    vencedor = 0;
                } else if (dentro_do_retangulo(mx, my, botaoVoltarMenuPrincipal)) {
                    estado = ESTADO_MENU;
                }
            } else if ((estado == ESTADO_JOGAR_PVP || estado == ESTADO_JOGAR_PVB) && !vitoria_ativa && !derrota_ativa && !empate_ativo) {  // MODIFICADO
                if (vencedor != 0) {
                    processar_vitoria_derrota(vencedor, modo_jogo, &estado, &inicio_partida, &historico);
                } else if (dentro_do_retangulo(mx, my, botaoVoltarSelecaoModo)) {
                    estado = ESTADO_JOGAR;
                    reiniciar_jogo(pontos, &jogador_atual, &ponto_selecionado, &fase_colocacao, &bolas_colocadas,
                                   &jogadas_sem_progresso, &inicio_partida);
                    vencedor = 0;
                } else {
                    for (int i = 0; i < 7; i++) {
                        if (clicou_no_ponto(mx, my, pontos[i])) {
                            if (fase_colocacao) {
                                if (pontos[i].jogador == 0) {
                                    pontos[i].jogador = jogador_atual;
                                    pontos[i].animando = true;
                                    bolas_colocadas++;
                                    printf("Jogador %d colocou peca na posicao %s\n", jogador_atual, pontos[i].posicao);

                                    mostrar_particulas = true;
                                    particulas_x = pontos[i].x;
                                    particulas_y = pontos[i].y;
                                    cor_particulas = (jogador_atual == 1)
                                                         ? al_map_rgb(255, 140, 0)
                                                         : al_map_rgb(255, 165, 0);

                                    if (bolas_colocadas == 6) {
                                        fase_colocacao = false;
                                        printf("Fase de colocacao finalizada. Iniciando fase de movimentacao.\n");
                                    }
                                    if (verifica_vitoria(pontos, jogador_atual)) {
                                        vencedor = jogador_atual;
                                        processar_vitoria_derrota(vencedor, modo_jogo, &estado, &inicio_partida,
                                                                  &historico);
                                        printf("Jogador %d venceu!\n", jogador_atual);
                                    } else {
                                        jogador_atual = (jogador_atual == 1) ? 2 : 1;
                                    }
                                }
                            } else {
                                if (ponto_selecionado == -1) {
                                    if (pontos[i].jogador == jogador_atual) {
                                        ponto_selecionado = i;
                                        pontos[i].selecionado = true;
                                        printf("Jogador %d selecionou peca na posicao %s\n", jogador_atual,
                                               pontos[i].posicao);
                                    }
                                } else {
                                    if (pontos[i].jogador == 0) {
                                        bool eh_pulo;
                                        if (movimento_valido(pontos, ponto_selecionado, i, &eh_pulo)) {
                                            printf("Jogador %d moveu peca de %s para %s", jogador_atual,
                                                   pontos[ponto_selecionado].posicao, pontos[i].posicao);
                                            if (eh_pulo) printf(" (movimento de pulo)");
                                            printf("\n");

                                            pontos[ponto_selecionado].jogador = 0;
                                            pontos[ponto_selecionado].selecionado = false;
                                            pontos[i].jogador = jogador_atual;
                                            pontos[i].animando = true;
                                            ponto_selecionado = -1;
                                            jogadas_sem_progresso++;

                                            mostrar_particulas = true;
                                            particulas_x = pontos[i].x;
                                            particulas_y = pontos[i].y;
                                            cor_particulas = (jogador_atual == 1)
                                                                 ? al_map_rgb(255, 140, 0)
                                                                 : al_map_rgb(255, 165, 0);

                                            if (verifica_vitoria(pontos, jogador_atual)) {
                                                vencedor = jogador_atual;
                                                processar_vitoria_derrota(
                                                    vencedor, modo_jogo, &estado, &inicio_partida, &historico);
                                                printf("Jogador %d venceu!\n", jogador_atual);
                                            } else if (verifica_empate(pontos, jogadas_sem_progresso)) {
                                                vencedor = -1;
                                                processar_vitoria_derrota(vencedor, modo_jogo, &estado, &inicio_partida, &historico);  // ADICIONADO
                                                printf("Jogo terminou em empate!\n");
                                            } else {
                                                jogador_atual = (jogador_atual == 1) ? 2 : 1;
                                            }
                                        }
                                    } else if (i == ponto_selecionado) {
                                        pontos[ponto_selecionado].selecionado = false;
                                        ponto_selecionado = -1;
                                    } else if (pontos[i].jogador == jogador_atual) {
                                        pontos[ponto_selecionado].selecionado = false;
                                        ponto_selecionado = i;
                                        pontos[i].selecionado = true;
                                        printf("Jogador %d selecionou peca na posicao %s\n", jogador_atual,
                                               pontos[i].posicao);
                                    }
                                }
                            }
                            break;
                        }
                    }

                    if (estado == ESTADO_JOGAR_PVB && jogador_atual == 2 && vencedor == 0) {
                        faz_jogada_bot(pontos, &jogador_atual, &ponto_selecionado, &fase_colocacao, &bolas_colocadas,
                                       &jogadas_sem_progresso);
                        if (vencedor == 0 && verifica_vitoria(pontos, 2)) {
                            vencedor = 2;
                            processar_vitoria_derrota(vencedor, modo_jogo, &estado, &inicio_partida, &historico);
                            printf("Bot venceu!\n");
                        } else if (verifica_empate(pontos, jogadas_sem_progresso)) {
                            vencedor = -1;
                            processar_vitoria_derrota(vencedor, modo_jogo, &estado, &inicio_partida, &historico);  // ADICIONADO
                            printf("Jogo terminou em empate!\n");
                        }
                    }
                }
            } else if (estado == ESTADO_COMO_JOGAR) {
                int painel_base_y = 150;
                int altura_total_paineis = 100 + 20 + 100 + 20 + 160 + 20 + 100 + 20 + 100;
                int botao_voltar_y = painel_base_y + altura_total_paineis + 40;

                if (mx >= centerX - 100 && mx <= centerX + 100 &&
                    my >= botao_voltar_y && my <= botao_voltar_y + 50) {
                    estado = ESTADO_MENU;
                }
            } else if (estado == ESTADO_HISTORICO) {
                if (dentro_do_retangulo(mx, my, botaoVoltarMenuPrincipal)) {
                    estado = ESTADO_MENU;
                }
            } else if (estado == ESTADO_PAUSA) {
                if (dentro_do_retangulo(mx, my, botaoContinuar)) {
                    estado = estado_anterior_pausa;
                } else if (dentro_do_retangulo(mx, my, botaoReiniciarPausado)) {
                    reiniciar_jogo(pontos, &jogador_atual, &ponto_selecionado, &fase_colocacao, &bolas_colocadas,
                                   &jogadas_sem_progresso, &inicio_partida);
                    vencedor = 0;
                    estado = estado_anterior_pausa;
                } else if (dentro_do_retangulo(mx, my, botaoMenuPrincipalPausado)) {
                    reiniciar_jogo(pontos, &jogador_atual, &ponto_selecionado, &fase_colocacao, &bolas_colocadas,
                                   &jogadas_sem_progresso, &inicio_partida);
                    vencedor = 0;
                    estado = ESTADO_MENU;
                } else if (dentro_do_retangulo(mx, my, botaoSalvarPausado)) {
                    estado_anterior_pausa = estado;
                    estado = ESTADO_SELECAO_SLOT;
                    modo_save = true;
                }
            } else if (estado == ESTADO_SELECAO_SLOT) {
                for (int i = 0; i < NUM_SLOTS; i++) {
                    Retangulo slot = {centerX - 100, 200 + i * 100, 200, 80};
                    if (dentro_do_retangulo(mx, my, slot)) {
                        if (modo_save) {
                            if (salvar_jogo_slot(i + 1, pontos, jogador_atual, ponto_selecionado, fase_colocacao,
                                                 bolas_colocadas, jogadas_sem_progresso, inicio_partida, modo_jogo)) {
                                mostrar_mensagem_sucesso = true;
                                tempo_mensagem = TEMPO_MAX_MENSAGEM;
                                printf("Jogo salvo no slot %d\n", i + 1);
                            } else {
                                mostrar_mensagem_erro = true;
                                tempo_mensagem = TEMPO_MAX_MENSAGEM;
                                printf("Erro ao salvar jogo no slot %d\n", i + 1);
                            }
                        } else {
                            char modo_carregado[20];
                            if (carregar_jogo_slot(i + 1, pontos, &jogador_atual, &ponto_selecionado, &fase_colocacao,
                                                   &bolas_colocadas, &jogadas_sem_progresso, &inicio_partida,
                                                   modo_carregado)) {
                                strcpy(modo_jogo, modo_carregado);
                                estado = (strcmp(modo_carregado, "PvB") == 0) ? ESTADO_JOGAR_PVB : ESTADO_JOGAR_PVP;
                                mostrar_mensagem_sucesso = true;
                                tempo_mensagem = TEMPO_MAX_MENSAGEM;
                                printf("Jogo carregado do slot %d\n", i + 1);
                            } else {
                                mostrar_mensagem_erro = true;
                                tempo_mensagem = TEMPO_MAX_MENSAGEM;
                                printf("Erro ao carregar jogo do slot %d\n", i + 1);
                            }
                        }
                    }
                }

                if (dentro_do_retangulo(mx, my, (Retangulo){centerX - 100, ALTURA_TELA - 100, 200, 50})) {
                    estado = estado_anterior_pausa;
                }
            }
        } else if (evento.type == ALLEGRO_EVENT_KEY_DOWN) {
            if (evento.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
                if (estado == ESTADO_JOGAR_PVP || estado == ESTADO_JOGAR_PVB) {
                    if (ponto_selecionado != -1) {
                        pontos[ponto_selecionado].selecionado = false;
                        ponto_selecionado = -1;
                    } else {
                        estado_anterior_pausa = estado;
                        estado = ESTADO_PAUSA;
                    }
                } else if (estado == ESTADO_PAUSA) {
                    estado = estado_anterior_pausa;
                } else if (estado == ESTADO_JOGAR) {
                    estado = ESTADO_MENU;
                } else if (estado == ESTADO_COMO_JOGAR || estado == ESTADO_HISTORICO) {
                    estado = ESTADO_MENU;
                }
            }
        }

        if (tempo_mensagem > 0) {
            tempo_mensagem--;
        } else {
            mostrar_mensagem_sucesso = false;
            mostrar_mensagem_erro = false;
        }

        if (al_is_event_queue_empty(fila_eventos)) {
            al_clear_to_color(al_map_rgb(255, 140, 0));

            if (estado == ESTADO_MENU) {
                if (fundo) {
                    al_draw_bitmap(fundo, 0, 0, 0);
                } else {
                    for (int y = 0; y < ALTURA_TELA; y++) {
                        float ratio = (float) y / ALTURA_TELA;
                        ALLEGRO_COLOR cor = al_map_rgb(255 - ratio * 50, 140 + ratio * 60, ratio * 40);
                        al_draw_line(0, y, LARGURA_TELA, y, cor, 1);
                    }
                }

                ALLEGRO_TRANSFORM transform;
                float escala_titulo = 1.0f + 0.05f * sin(al_get_time() * 2.0);

                al_identity_transform(&transform);
                al_translate_transform(&transform, -centerX, -(ALTURA_TELA / 6));
                al_scale_transform(&transform, escala_titulo, escala_titulo);
                al_translate_transform(&transform, centerX, ALTURA_TELA / 6);
                al_use_transform(&transform);

                al_draw_text(fonte_titulo, al_map_rgb(139, 69, 19), centerX + 3, ALTURA_TELA / 6 + 3, ALLEGRO_ALIGN_CENTER,
                             "TRI-angle");
                al_draw_text(fonte_titulo, al_map_rgb(255, 255, 255), centerX, ALTURA_TELA / 6, ALLEGRO_ALIGN_CENTER,
                             "TRI-angle");

                al_identity_transform(&transform);
                al_use_transform(&transform);

                desenhar_botao_animado(botaoJogar, "Jogar", fonte, al_map_rgb(255, 140, 0), al_map_rgb(255, 255, 255),
                                       botoes_hover[0]);
                desenhar_botao_animado(botaoComoJogar, "Como Jogar", fonte, al_map_rgb(210, 180, 140),
                                       al_map_rgb(139, 69, 19), botoes_hover[1]);
                desenhar_botao_animado(botaoCarregarJogo, "Carregar Jogo", fonte, al_map_rgb(205, 133, 63),
                                       al_map_rgb(255, 255, 255), botoes_hover[2]);
                desenhar_botao_animado(botaoHistorico, "Historico", fonte, al_map_rgb(160, 82, 45),
                                       al_map_rgb(255, 255, 255), botoes_hover[3]);
                desenhar_botao_animado(botaoSair, "Sair", fonte, al_map_rgb(178, 34, 34), al_map_rgb(255, 255, 255),
                                       botoes_hover[4]);

                al_draw_text(fonte_menor, al_map_rgb(139, 69, 19), LARGURA_TELA - 20, ALTURA_TELA - 30,
                             ALLEGRO_ALIGN_RIGHT, "v1.0");
            } else if (estado == ESTADO_JOGAR) {
                if (fundo) {
                    al_draw_bitmap(fundo, 0, 0, 0);
                }

                al_draw_text(fonte_titulo, al_map_rgb(139, 69, 19), centerX + 2, ALTURA_TELA / 4 + 2, ALLEGRO_ALIGN_CENTER,
                             "Selecione o modo de jogo");
                al_draw_text(fonte_titulo, al_map_rgb(255, 255, 255), centerX, ALTURA_TELA / 4, ALLEGRO_ALIGN_CENTER,
                             "Selecione o modo de jogo");

                desenhar_botao_animado(botaoPvP, "PvP", fonte, al_map_rgb(220, 20, 60), al_map_rgb(255, 255, 255),
                                       esta_sobre_botao(mouse_x, mouse_y, botaoPvP));
                desenhar_botao_animado(botaoPvB, "P vs Bot", fonte, al_map_rgb(255, 165, 0), al_map_rgb(255, 255, 255),
                                       esta_sobre_botao(mouse_x, mouse_y, botaoPvB));
                desenhar_botao_animado(botaoVoltarMenuPrincipal, "Voltar", fonte_menor, al_map_rgb(178, 34, 34),
                                       al_map_rgb(255, 255, 255),
                                       esta_sobre_botao(mouse_x, mouse_y, botaoVoltarMenuPrincipal));
            } else if (estado == ESTADO_JOGAR_PVP || estado == ESTADO_JOGAR_PVB || estado == ESTADO_PAUSA) {
                if (fundo_tabuleiro) {
                    al_draw_bitmap(fundo_tabuleiro, 0, 0, 0);
                }

                if (estado != ESTADO_PAUSA) {
                    desenhar_interface_cartoon(jogador_atual, fase_colocacao, ponto_selecionado,
                                              bolas_colocadas, pontos, inicio_partida, modo_jogo,
                                              vencedor, fonte, fonte_menor,jogadas_sem_progresso);

                    desenhar_tabuleiro_cartoon(pontos, jogador_atual, fase_colocacao, ponto_selecionado,
                                              bolas_colocadas, fonte_menor);

                    if (mostrar_particulas) {
                        desenhar_particulas(particulas_x, particulas_y, 30, cor_particulas);
                    }

                    desenhar_botao_animado(botaoVoltarSelecaoModo, "VOLTAR", fonte_menor, al_map_rgb(178, 34, 34),
                                          al_map_rgb(255, 255, 255),
                                          esta_sobre_botao(mouse_x, mouse_y, botaoVoltarSelecaoModo));
                }

                if (estado == ESTADO_PAUSA) {
                    al_draw_filled_rectangle(0, 0, LARGURA_TELA, ALTURA_TELA, al_map_rgba(0, 0, 0, 150));
                    al_draw_text(fonte_titulo, al_map_rgb(255, 215, 0), centerX, centerY - 150, ALLEGRO_ALIGN_CENTER,
                                 "JOGO PAUSADO");

                    desenhar_botao_animado(botaoContinuar, "Continuar (ESC)", fonte_menor, al_map_rgb(34, 139, 34),
                                           al_map_rgb(255, 255, 255),
                                           esta_sobre_botao(mouse_x, mouse_y, botaoContinuar));
                    desenhar_botao_animado(botaoReiniciarPausado, "Reiniciar Jogo", fonte_menor,
                                           al_map_rgb(255, 140, 0), al_map_rgb(255, 255, 255),
                                           esta_sobre_botao(mouse_x, mouse_y, botaoReiniciarPausado));
                    desenhar_botao_animado(botaoMenuPrincipalPausado, "Menu Principal", fonte_menor,
                                           al_map_rgb(178, 34, 34), al_map_rgb(255, 255, 255),
                                           esta_sobre_botao(mouse_x, mouse_y, botaoMenuPrincipalPausado));
                    desenhar_botao_animado(botaoSalvarPausado, "Salvar Jogo", fonte_menor, al_map_rgb(160, 82, 45),
                                           al_map_rgb(255, 255, 255),
                                           esta_sobre_botao(mouse_x, mouse_y, botaoSalvarPausado));
                }
            } else if (estado == ESTADO_COMO_JOGAR) {
                for (int y = 0; y < ALTURA_TELA; y++) {
                    float ratio = (float) y / ALTURA_TELA;
                    ALLEGRO_COLOR cor = al_map_rgb(255 - ratio * 50, 140 + ratio * 60, ratio * 40);
                    al_draw_line(0, y, LARGURA_TELA, y, cor, 1);
                }

                if (fundo_como_jogar) {
                    al_draw_tinted_bitmap(fundo_como_jogar, al_map_rgba(255, 255, 255, 180), 0, 0, 0);
                }

                al_draw_filled_rectangle(0, 0, LARGURA_TELA, ALTURA_TELA, al_map_rgba(0, 0, 0, 30));

                float escala_titulo = 1.0f + 0.05f * sin(al_get_time() * 2.0);
                ALLEGRO_TRANSFORM transform;
                al_identity_transform(&transform);
                al_translate_transform(&transform, -centerX, -50);
                al_scale_transform(&transform, escala_titulo, escala_titulo);
                al_translate_transform(&transform, centerX, 50);
                al_use_transform(&transform);

                al_draw_text(fonte_titulo, al_map_rgb(139, 69, 19), centerX + 3, 53, ALLEGRO_ALIGN_CENTER,
                             "COMO JOGAR TRI-ANGLE");
                al_draw_text(fonte_titulo, al_map_rgb(255, 215, 0), centerX, 50, ALLEGRO_ALIGN_CENTER,
                             "COMO JOGAR TRI-ANGLE");

                al_identity_transform(&transform);
                al_use_transform(&transform);

                int largura_painel = 1100;
                int x_painel = (LARGURA_TELA - largura_painel) / 2;
                int espaco_entre_paineis = 20;

                int painel_modos_y = 150;
                int altura_painel_modos = 100;
                desenhar_painel_cartoon(x_painel, painel_modos_y, largura_painel, altura_painel_modos,
                                       al_map_rgb(255, 140, 0), al_map_rgb(139, 69, 19));

                al_draw_text(fonte, al_map_rgb(139, 69, 19), x_painel + 80, painel_modos_y + 15, 0, "MODO PVP");
                al_draw_text(fonte, al_map_rgb(255, 255, 255), x_painel + 350, painel_modos_y + 15, 0,
                             "JOGADOR VS JOGADOR");
                al_draw_text(fonte, al_map_rgb(139, 69, 19), x_painel + 80, painel_modos_y + 55, 0, "MODO PVB");
                al_draw_text(fonte, al_map_rgb(255, 255, 255), x_painel + 350, painel_modos_y + 55, 0,
                             "JOGADOR VS BOT");

                int painel_fase1_y = painel_modos_y + altura_painel_modos + espaco_entre_paineis;
                int altura_painel_fase1 = 100;
                desenhar_painel_cartoon(x_painel, painel_fase1_y, largura_painel, altura_painel_fase1,
                                       al_map_rgb(220, 20, 60), al_map_rgb(139, 0, 0));

                al_draw_text(fonte, al_map_rgb(255, 255, 255), x_painel + 80, painel_fase1_y + 15, 0, "FASE 1");
                al_draw_text(fonte, al_map_rgb(255, 255, 255), x_painel + 350, painel_fase1_y + 15, 0,
                             "COLOCACAO DAS PECAS");
                al_draw_text(fonte, al_map_rgb(255, 255, 255), x_painel + 80, painel_fase1_y + 55, 0,
                             "CADA JOGADOR COLOCA 3 BOLAS NO TABULEIRO");

                int painel_fase2_y = painel_fase1_y + altura_painel_fase1 + espaco_entre_paineis;
                int altura_painel_fase2 = 160;
                desenhar_painel_cartoon(x_painel, painel_fase2_y, largura_painel, altura_painel_fase2,
                                       al_map_rgb(34, 139, 34), al_map_rgb(0, 100, 0));

                int y_pos = painel_fase2_y + 15;
                int linha_altura = 35;
                al_draw_text(fonte, al_map_rgb(255, 255, 255), x_painel + 80, y_pos, 0, "FASE 2");
                al_draw_text(fonte, al_map_rgb(255, 255, 255), x_painel + 350, y_pos, 0, "MOVIMENTACAO");
                y_pos += linha_altura;
                al_draw_text(fonte, al_map_rgb(255, 255, 255), x_painel + 80, y_pos, 0,
                             "1 MOVA UMA BOLA PARA CASA EM ALINHAMENTO");
                y_pos += linha_altura;
                al_draw_text(fonte, al_map_rgb(255, 255, 255), x_painel + 80, y_pos, 0,
                             "2 PULE SOBRE OUTRA PECA ADJACENTE EM ");
                y_pos += linha_altura;
                al_draw_text(fonte, al_map_rgb(255, 255, 255), x_painel + 80, y_pos, 0, "ALINHAMENTO PARA CASA VAZIA");

                int painel_objetivos_y = painel_fase2_y + altura_painel_fase2 + espaco_entre_paineis;
                int altura_painel_objetivos = 100;
                desenhar_painel_cartoon(x_painel, painel_objetivos_y, largura_painel, altura_painel_objetivos,
                                       al_map_rgb(255, 215, 0), al_map_rgb(184, 134, 11));

                y_pos = painel_objetivos_y + 15;
                al_draw_text(fonte, al_map_rgb(139, 69, 19), x_painel + 80, y_pos, 0, "OBJETIVO");
                al_draw_text(fonte, al_map_rgb(139, 69, 19), x_painel + 350, y_pos, 0,
                             "FORMAR UMA LINHA COM 3 BOLAS");
                y_pos += linha_altura;
                al_draw_text(fonte, al_map_rgb(139, 69, 19), x_painel + 350, y_pos, 0,
                             "HORIZONTAL  VERTICAL  DIAGONAL");

                int painel_regras_y = painel_objetivos_y + altura_painel_objetivos + espaco_entre_paineis;
                int altura_painel_regras = 100;
                desenhar_painel_cartoon(x_painel, painel_regras_y, largura_painel, altura_painel_regras,
                                       al_map_rgb(160, 82, 45), al_map_rgb(101, 67, 33));

                y_pos = painel_regras_y + 15;
                al_draw_text(fonte, al_map_rgb(255, 255, 255), x_painel + 80, y_pos, 0, "REGRAS");
                al_draw_text(fonte, al_map_rgb(255, 255, 255), x_painel + 350, y_pos, 0,
                             "O JOGO NAO PERMITE CAPTURA DE PECAS");
                y_pos += linha_altura;
                al_draw_text(fonte, al_map_rgb(255, 255, 255), x_painel + 350, y_pos, 0, "EMPATES SAO POSSIVEIS");

                int botao_voltar_y = painel_regras_y + altura_painel_regras + 40;
                Retangulo botaoVoltarComoJogar = {centerX - 100, botao_voltar_y, 200, 50};

                desenhar_botao_animado(botaoVoltarComoJogar, "VOLTAR", fonte,
                                       esta_sobre_botao(mouse_x, mouse_y, botaoVoltarComoJogar)
                                           ? al_map_rgb(255, 80, 80)
                                           : al_map_rgb(178, 34, 34),
                                       al_map_rgb(255, 255, 255),
                                       esta_sobre_botao(mouse_x, mouse_y, botaoVoltarComoJogar));
            } else if (estado == ESTADO_HISTORICO) {
                if (fundo_como_jogar) {
                    al_draw_bitmap(fundo_como_jogar, 0, 0, 0);
                }

                al_draw_filled_rectangle(0, 0, LARGURA_TELA, ALTURA_TELA, al_map_rgba(0, 0, 0, 120));

                al_draw_text(fonte_titulo, al_map_rgb(139, 69, 19), centerX + 2, 52, ALLEGRO_ALIGN_CENTER,
                             "HISTORICO DE PARTIDAS");
                al_draw_text(fonte_titulo, al_map_rgb(255, 215, 0), centerX, 50, ALLEGRO_ALIGN_CENTER,
                             "HISTORICO DE PARTIDAS");

                char info[200];
                int largura_painel = 1100;
                int x_painel = (LARGURA_TELA - largura_painel) / 2;

                int painel_pvp_y = 110;
                desenhar_painel_cartoon(x_painel, painel_pvp_y, largura_painel, 120,
                                       al_map_rgb(220, 20, 60), al_map_rgb(139, 0, 0));

                al_draw_text(fonte_menor, al_map_rgb(255, 255, 255), x_painel + 20, painel_pvp_y + 15, 0,
                             "PLAYER vs PLAYER");

                sprintf(info, "%d partidas jogadas", historico.partidas_pvp);
                al_draw_text(fonte_menor, al_map_rgb(255, 255, 255), x_painel + 20, painel_pvp_y + 40, 0, info);

                sprintf(info, "Vitorias P1: %d  |  Vitorias P2: %d  |  Empates: %d",
                        historico.vitorias_p1_pvp, historico.vitorias_p2_pvp, historico.empates_pvp);
                al_draw_text(fonte_menor, al_map_rgb(255, 255, 255), x_painel + 20, painel_pvp_y + 65, 0, info);

                if (historico.partidas_pvp > 0) {
                    sprintf(info, "MENOR TEMPO: %d:%02d  |  MAIOR TEMPO: %d:%02d",
                            historico.menor_tempo_pvp / 60, historico.menor_tempo_pvp % 60,
                            historico.maior_tempo_pvp / 60, historico.maior_tempo_pvp % 60);
                    al_draw_text(fonte_menor, al_map_rgb(255, 215, 0), x_painel + 20, painel_pvp_y + 90, 0, info);
                }

                int painel_pvb_y = 260;
                desenhar_painel_cartoon(x_painel, painel_pvb_y, largura_painel, 120,
                                       al_map_rgb(255, 165, 0), al_map_rgb(178, 34, 34));

                al_draw_text(fonte_menor, al_map_rgb(255, 255, 255), x_painel + 20, painel_pvb_y + 15, 0,
                             "PLAYER vs BOT");

                sprintf(info, "%d partidas jogadas", historico.partidas_pvb);
                al_draw_text(fonte_menor, al_map_rgb(255, 255, 255), x_painel + 20, painel_pvb_y + 40, 0, info);

                sprintf(info, "Vitorias Jogador: %d  |  Vitorias Bot: %d  |  Empates: %d",
                        historico.vitorias_jogador_pvb, historico.vitorias_bot_pvb, historico.empates_pvb);
                al_draw_text(fonte_menor, al_map_rgb(255, 255, 255), x_painel + 20, painel_pvb_y + 65, 0, info);

                if (historico.partidas_pvb > 0) {
                    sprintf(info, "MENOR TEMPO: %d:%02d  |  MAIOR TEMPO: %d:%02d",
                            historico.menor_tempo_pvb / 60, historico.menor_tempo_pvb % 60,
                            historico.maior_tempo_pvb / 60, historico.maior_tempo_pvb % 60);
                    al_draw_text(fonte_menor, al_map_rgb(255, 215, 0), x_painel + 20, painel_pvb_y + 90, 0, info);
                }

                int painel_hist_y = 410;
                desenhar_painel_cartoon(x_painel, painel_hist_y, largura_painel, 220,
                                       al_map_rgb(34, 139, 34), al_map_rgb(0, 100, 0));

                al_draw_text(fonte_menor, al_map_rgb(255, 255, 255), x_painel + 20, painel_hist_y + 20, 0,
                             "ULTIMAS PARTIDAS");

                al_draw_text(fonte_menor, al_map_rgb(255, 215, 0), x_painel + 40, painel_hist_y + 60, 0, "Modo");
                al_draw_text(fonte_menor, al_map_rgb(255, 215, 0), x_painel + 200, painel_hist_y + 60, 0,
                             "Resultado");
                al_draw_text(fonte_menor, al_map_rgb(255, 215, 0), x_painel + 400, painel_hist_y + 60, 0, "Tempo");
                al_draw_text(fonte_menor, al_map_rgb(255, 215, 0), x_painel + 520, painel_hist_y + 60, 0, "Data");

                al_draw_line(x_painel + 30, painel_hist_y + 85, x_painel + largura_painel - 30, painel_hist_y + 85,
                             al_map_rgb(255, 215, 0), 1);

                int inicio = (historico.total_registros > 5) ? historico.total_registros - 5 : 0;
                for (int i = inicio; i < historico.total_registros; i++) {
                    RegistroHistorico *reg = &historico.registros[i];
                    int linha_y = painel_hist_y + 105 + (i - inicio) * 22;

                    ALLEGRO_COLOR cor_resultado;
                    char resultado[50];
                    if (reg->vencedor == 1) {
                        strcpy(resultado, "Vitoria P1");
                        cor_resultado = al_map_rgb(255, 215, 0);
                    } else if (reg->vencedor == 2) {
                        strcpy(resultado, "Vitoria Bot");
                        cor_resultado = al_map_rgb(255, 140, 0);
                    } else {
                        strcpy(resultado, "Empate");
                        cor_resultado = al_map_rgb(255, 255, 255);
                    }

                    al_draw_text(fonte_menor, al_map_rgb(255, 255, 255), x_painel + 40, linha_y, 0, reg->modo);
                    al_draw_text(fonte_menor, cor_resultado, x_painel + 200, linha_y, 0, resultado);

                    sprintf(info, "%d:%02d", reg->tempo_segundos / 60, reg->tempo_segundos % 60);
                    al_draw_text(fonte_menor, al_map_rgb(255, 255, 255), x_painel + 400, linha_y, 0, info);

                    al_draw_text(fonte_menor, al_map_rgb(255, 255, 255), x_painel + 520, linha_y, 0, reg->data);
                }

                desenhar_botao_animado(botaoVoltarMenuPrincipal, "VOLTAR", fonte_menor,
                                       esta_sobre_botao(mouse_x, mouse_y, botaoVoltarMenuPrincipal)
                                           ? al_map_rgb(255, 80, 80)
                                           : al_map_rgb(178, 34, 34),
                                       al_map_rgb(255, 255, 255),
                                       esta_sobre_botao(mouse_x, mouse_y, botaoVoltarMenuPrincipal));
            } else if (estado == ESTADO_SELECAO_SLOT) {
                if (fundo_como_jogar) {
                    al_draw_bitmap(fundo_como_jogar, 0, 0, 0);
                }

                al_draw_text(fonte_titulo, al_map_rgb(139, 69, 19), centerX + 2, 102,
                             ALLEGRO_ALIGN_CENTER,
                             modo_save ? "Selecione slot para salvar" : "Selecione slot para carregar");
                al_draw_text(fonte_titulo, al_map_rgb(255, 255, 255), centerX, 100,
                             ALLEGRO_ALIGN_CENTER,
                             modo_save ? "Selecione slot para salvar" : "Selecione slot para carregar");

                for (int i = 0; i < NUM_SLOTS; i++) {
                    Retangulo slot = {centerX - 100, 200 + i * 100, 200, 80};
                    bool hover = esta_sobre_botao(mouse_x, mouse_y, slot);

                    desenhar_botao_animado(slot, "", fonte, al_map_rgb(160, 82, 45), al_map_rgb(255, 255, 255), hover);

                    char slot_info[50];
                    sprintf(slot_info, "Slot %d", i + 1);
                    al_draw_text(fonte, al_map_rgb(255, 255, 255), slot.x + slot.largura / 2,
                                 slot.y + 25, ALLEGRO_ALIGN_CENTER, slot_info);

                    char filename[20];
                    sprintf(filename, "savegame%d.dat", i + 1);
                    FILE *f = fopen(filename, "rb");
                    if (f) {
                        al_draw_text(fonte_menor, al_map_rgb(34, 139, 34), slot.x + slot.largura / 2,
                                     slot.y + 55, ALLEGRO_ALIGN_CENTER, "Jogo Salvo");
                        fclose(f);
                    } else {
                        al_draw_text(fonte_menor, al_map_rgb(220, 20, 60), slot.x + slot.largura / 2,
                                     slot.y + 55, ALLEGRO_ALIGN_CENTER, "Vazio");
                    }
                }

                desenhar_botao_animado((Retangulo){centerX - 100, ALTURA_TELA - 100, 200, 50}, "Voltar", fonte,
                                       al_map_rgb(178, 34, 34), al_map_rgb(255, 255, 255),
                                       esta_sobre_botao(mouse_x, mouse_y, (Retangulo){
                                                            centerX - 100, ALTURA_TELA - 100, 200, 50
                                                        }));
            }

            renderizar_efeitos_vitoria_derrota(fonte_titulo, vencedor);

            if (mostrar_mensagem_sucesso) {
                al_draw_filled_rounded_rectangle(centerX - 200, ALTURA_TELA - 220, centerX + 200, ALTURA_TELA - 180, 10,
                                                 10, al_map_rgba(34, 139, 34, 200));
                al_draw_text(fonte_menor, al_map_rgb(255, 255, 255), centerX, ALTURA_TELA - 200,
                             ALLEGRO_ALIGN_CENTER, "Operacao realizada com sucesso!");
            }
            if (mostrar_mensagem_erro) {
                al_draw_filled_rounded_rectangle(centerX - 200, ALTURA_TELA - 220, centerX + 200, ALTURA_TELA - 180, 10,
                                                 10, al_map_rgba(178, 34, 34, 200));
                al_draw_text(fonte_menor, al_map_rgb(255, 255, 255), centerX, ALTURA_TELA - 200,
                             ALLEGRO_ALIGN_CENTER, "Erro na operacao!");
            }

            al_flip_display();
        }
    }

    if (fundo) al_destroy_bitmap(fundo);
    if (fundo_tabuleiro) al_destroy_bitmap(fundo_tabuleiro);
    if (fundo_como_jogar) al_destroy_bitmap(fundo_como_jogar);

    al_destroy_font(fonte);
    al_destroy_font(fonte_menor);
    al_destroy_font(fonte_titulo);
    al_destroy_timer(timer);
    al_destroy_event_queue(fila_eventos);
    al_destroy_display(janela);
    return 0;
}
