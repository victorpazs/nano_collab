#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>  // Para função select() - entrada não-bloqueante
#include <sys/time.h>    // Para struct timeval
#include <mpi.h>         // Biblioteca para comunicação entre processos distribuídos
#include <omp.h>         // Biblioteca para paralelização OpenMP

#define MAX_LINHAS 100
#define MAX_TEXTO 256
#define MASTER 0       // Processo mestre que gerencia o documento

// Códigos de Cor ANSI para o terminal
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_MAGENTA "\x1b[35m" // Lilás/Magenta
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_CYAN    "\x1b[36m" // Ciano/Azul claro

// Tags para a comunicação MPI - identificam o tipo de mensagem sendo enviada
#define TAG_PEDIDO_BLOQUEIO     1    // Usuário solicita bloqueio de linha para edição
#define TAG_RESPOSTA_BLOQUEIO   2    // Mestre responde se o bloqueio foi concedido
#define TAG_ENVIAR_NOVO_TEXTO   3    // Usuário envia texto editado para o mestre
#define TAG_MENSAGEM_PRIVADA    4    // Mensagem entre usuários
#define TAG_SAIR                5    // Usuário notifica que está saindo
#define TAG_ATUALIZACAO         6    // Mestre envia atualizações do documento
#define TAG_FINALIZAR           7    // Sinal para encerramento seguro

// Estruturas globais compartilhadas
char documento[MAX_LINHAS][MAX_TEXTO];  // Documento colaborativo com 100 linhas
int linhas_em_uso[MAX_LINHAS];          // Controle de bloqueio: -1=livre, rank=bloqueada
int rank_global, size_global;           // Identificador e total de processos MPI

// Sistema de mensagens/chat
#define MAX_MENSAGENS 50
typedef struct {
    int remetente;
    char conteudo[MAX_TEXTO];
    char timestamp[20];
} Mensagem;

Mensagem chat_mensagens[MAX_MENSAGENS];  // Buffer circular de mensagens
int chat_count = 0;                      // Contador de mensagens
int chat_inicio = 0;                     // Índice do início do buffer circular

// Declaração das funções principais
void gerar_documento_inicial();
void mostrar_documento();
void registrar_log(int rank_usuario, int linha, const char* texto);
void loop_mestre();
void loop_trabalhador();
int verificar_mensagens_e_atualizacoes();
void visualizacao_tempo_real(); // Nova função para visualização em tempo real
void adicionar_mensagem_chat(int remetente, const char* conteudo); // Adiciona mensagem ao chat
void visualizar_mensagens_chat(); // Exibe histórico de mensagens
void listar_usuarios_disponiveis(); // Lista usuários para envio de mensagem

int main(int argc, char** argv) {
    int provided;
    // Inicialização MPI com suporte a threads (necessário para OpenMP)
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank_global);  // Obtém ID do processo atual
    MPI_Comm_size(MPI_COMM_WORLD, &size_global);  // Obtém total de processos

    // Verifica se há pelo menos 2 processos (1 mestre + 1 trabalhador)
    if (size_global < 2) {
        if (rank_global == MASTER) {
            fprintf(stderr, "Erro: Este programa precisa de pelo menos 2 processos.\n");
        }
        MPI_Finalize();
        return 1;
    }

    // Processo mestre inicializa o documento e cria arquivo de log
    if (rank_global == MASTER) {
        printf("[MESTRE] Iniciando e gerando documento...\n");
        gerar_documento_inicial();
        FILE* f = fopen("log_editor.txt", "w");
        if(f) fclose(f);
    }

    // Sincroniza o documento inicial entre todos os processos
    MPI_Bcast(documento, MAX_LINHAS * MAX_TEXTO, MPI_CHAR, MASTER, MPI_COMM_WORLD);
    MPI_Bcast(linhas_em_uso, MAX_LINHAS, MPI_INT, MASTER, MPI_COMM_WORLD);

    // Executa função específica baseada no tipo de processo
    if (rank_global == MASTER) {
        loop_mestre();      // Gerencia documento e coordena colaboração
    } else {
        loop_trabalhador(); // Interface de usuário para edição
    }

    MPI_Barrier(MPI_COMM_WORLD);  // Sincroniza todos os processos antes de finalizar
    MPI_Finalize();
    return 0;
}

// Gera o documento inicial com texto padrão usando paralelização OpenMP
void gerar_documento_inicial() {
    #pragma omp parallel for  // Paraleliza a inicialização das linhas
    for (int i = 0; i < MAX_LINHAS; i++) {
        sprintf(documento[i], "Linha %d: texto inicial gerado automaticamente.", i);
        linhas_em_uso[i] = -1;  // Marca todas as linhas como livres
    }
}

// Exibe o documento atual com interface colorida e status de bloqueio
void mostrar_documento() {
    printf(ANSI_COLOR_MAGENTA "\n  +--------------------------------------------------------------------------+\n" ANSI_COLOR_RESET);
    printf(ANSI_COLOR_MAGENTA "  |                           Visualizacao do Documento                        |\n" ANSI_COLOR_RESET);
    printf(ANSI_COLOR_MAGENTA "  +--------------------------------------------------------------------------+\n" ANSI_COLOR_RESET);
    
    // Mostra apenas as primeiras 20 linhas para melhor visualização
    for (int i = 0; i < 20; i++) {
        char linha_completa[MAX_TEXTO + 50];
        char status_linha[50] = "";
        
        // Indica se a linha está bloqueada por algum usuário
        if (linhas_em_uso[i] != -1) {
            sprintf(status_linha, ANSI_COLOR_RED " (Bloqueada por Usuario_%d)" ANSI_COLOR_RESET, linhas_em_uso[i]);
        }
        sprintf(linha_completa, "%s%s", documento[i], status_linha);
        
        printf(ANSI_COLOR_MAGENTA "  | " ANSI_COLOR_YELLOW "[%02d]" ANSI_COLOR_RESET " %-70s" ANSI_COLOR_MAGENTA "|\n", i, linha_completa);
    }
    printf(ANSI_COLOR_MAGENTA "  +--------------------------------------------------------------------------+\n" ANSI_COLOR_RESET);
}

// Registra todas as edições em arquivo de log com timestamp
void registrar_log(int rank_usuario, int linha, const char* texto) {
    FILE *log = fopen("log_editor.txt", "a");
    if (log) {
        time_t agora = time(NULL);
        struct tm *t = localtime(&agora);
        char timestamp[20];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
        fprintf(log, "[%s] [Usuario_%d] editou a linha %d: \"%s\"\n", timestamp, rank_usuario, linha, texto);
        fclose(log);
    }
}

// Loop principal do processo mestre - coordena todas as operações colaborativas
void loop_mestre() {
    int trabalhadores_ativos = size_global - 1;
    MPI_Status status;
    int buffer_int[2];      // Buffer para receber dados inteiros
    char buffer_texto[MAX_TEXTO];  // Buffer para receber texto
    
    MPI_Request requests[size_global * 2];  // Para comunicação não-bloqueante
    int req_count;

    // Loop principal de coordenação
    while (trabalhadores_ativos > 0) {
        // Recebe solicitações de qualquer trabalhador
        MPI_Recv(buffer_int, 2, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        
        int remetente = status.MPI_SOURCE;
        int tag = status.MPI_TAG;
        int linha_req = buffer_int[0];

        switch (tag) {
            case TAG_PEDIDO_BLOQUEIO:
                // Processa solicitação de bloqueio de linha para edição
                printf("[MESTRE] Recebido pedido de Usuario_%d para bloquear a linha %d\n", remetente, linha_req);
                int resposta = 0;
                // Verifica se a linha é válida e está disponível
                if (linha_req >= 0 && linha_req < MAX_LINHAS && linhas_em_uso[linha_req] == -1) {
                    linhas_em_uso[linha_req] = remetente;  // Bloqueia para o usuário
                    resposta = 1;  // Aprovado
                }
                MPI_Send(&resposta, 1, MPI_INT, remetente, TAG_RESPOSTA_BLOQUEIO, MPI_COMM_WORLD);
                break;
            
            case TAG_ENVIAR_NOVO_TEXTO:
                // Recebe texto editado e distribui para todos os usuários
                MPI_Recv(buffer_texto, MAX_TEXTO, MPI_CHAR, remetente, TAG_ENVIAR_NOVO_TEXTO, MPI_COMM_WORLD, &status);
                printf("[MESTRE] Recebido novo texto para linha %d. Distribuindo para todos.\n", linha_req);
                
                strcpy(documento[linha_req], buffer_texto);  // Atualiza documento
                registrar_log(remetente, linha_req, buffer_texto);  // Registra no log
                linhas_em_uso[linha_req] = -1;  // Libera o bloqueio da linha
                
                // Distribui atualização para todos os trabalhadores usando comunicação não-bloqueante
                req_count = 0;
                for (int i = 1; i < size_global; i++) {
                    MPI_Isend(documento, MAX_LINHAS * MAX_TEXTO, MPI_CHAR, i, TAG_ATUALIZACAO, MPI_COMM_WORLD, &requests[req_count++]);
                    MPI_Isend(linhas_em_uso, MAX_LINHAS, MPI_INT, i, TAG_ATUALIZACAO, MPI_COMM_WORLD, &requests[req_count++]);
                }
                break;

            case TAG_SAIR:
                // Usuário notifica que está saindo do editor
                printf("[MESTRE] Usuario_%d saiu. %d restantes.\n", remetente, trabalhadores_ativos - 1);
                trabalhadores_ativos--;
                break;
        }
    }
    
    // Quando todos saem, envia sinal de finalização
    printf("[MESTRE] Todos os trabalhadores saíram. Enviando sinal para finalizar.\n");
    req_count = 0;
    int dummy = 0;
    for (int i = 1; i < size_global; i++) {
        MPI_Isend(&dummy, 1, MPI_INT, i, TAG_FINALIZAR, MPI_COMM_WORLD, &requests[req_count++]);
    }
    MPI_Waitall(req_count, requests, MPI_STATUSES_IGNORE);
}

// Loop principal dos processos trabalhadores - interface de usuário
void loop_trabalhador() {
    char nome_usuario[50];
    sprintf(nome_usuario, "Usuario_%d", rank_global);
    MPI_Status status; 
    int usuario_ativo = 1;

    while (1) {
        // Verifica mensagens recebidas (atualizações, mensagens privadas, finalização)
        if (verificar_mensagens_e_atualizacoes()) {
            break;  // Recebeu sinal de finalização
        }

        if (!usuario_ativo) {
            sleep(1);  // Usuário saiu, aguarda finalização
            continue;
        }

        // Interface do menu principal
        printf("\n[%s] Menu:\n", nome_usuario);
        printf("1. Visualizar documento em tempo real\n");
        printf("2. Editar linha\n");
        printf("3. Enviar mensagem privada\n");
        printf("4. Visualizar mensagens recebidas\n");
        printf("5. Sair\n> ");

        int opcao;
        if (scanf(" %d", &opcao) != 1) { 
            while (getchar() != '\n');
            printf("Opção inválida. Por favor, digite um número.\n");
            continue;
        }

        if (opcao == 1) {
            // Opção 1: Visualização em tempo real do documento
            visualizacao_tempo_real();
            
        } else if (opcao == 2) {
            // Opção 2: Editar uma linha específica
            printf("Digite o numero da linha para editar (0 a %d): ", MAX_LINHAS - 1);
            int linha_para_editar;
            scanf(" %d", &linha_para_editar);

            // Solicita bloqueio da linha ao mestre
            int pedido[2] = {linha_para_editar, 0};
            MPI_Send(pedido, 2, MPI_INT, MASTER, TAG_PEDIDO_BLOQUEIO, MPI_COMM_WORLD);

            // Aguarda resposta do mestre
            int resposta;
            MPI_Recv(&resposta, 1, MPI_INT, MASTER, TAG_RESPOSTA_BLOQUEIO, MPI_COMM_WORLD, &status);

            if (resposta == 1) {
                // Bloqueio concedido - permite edição
                printf(ANSI_COLOR_GREEN "Permissão concedida! Digite o novo texto:\n> " ANSI_COLOR_RESET);
                char novo_texto[MAX_TEXTO];
                getchar(); 
                fgets(novo_texto, MAX_TEXTO, stdin);
                novo_texto[strcspn(novo_texto, "\n")] = 0;  // Remove quebra de linha

                // Envia o novo texto para o mestre
                int dados_texto[2] = {linha_para_editar, 0};
                MPI_Send(dados_texto, 2, MPI_INT, MASTER, TAG_ENVIAR_NOVO_TEXTO, MPI_COMM_WORLD);
                MPI_Send(novo_texto, strlen(novo_texto) + 1, MPI_CHAR, MASTER, TAG_ENVIAR_NOVO_TEXTO, MPI_COMM_WORLD);
                
                printf(ANSI_COLOR_GREEN "Alteração enviada. O documento será atualizado em breve.\n" ANSI_COLOR_RESET);

            } else {
                // Bloqueio negado - linha ocupada por outro usuário
                printf(ANSI_COLOR_RED "Acesso negado! A linha pode estar em uso.\n" ANSI_COLOR_RESET);
            }
            
        } else if (opcao == 3) {
            // Opção 3: Enviar mensagem privada para outro usuário
            listar_usuarios_disponiveis(); // Lista usuários disponíveis
            printf("\nDigite o rank do destinatário: ");
            int destino;
            scanf(" %d", &destino);
            
            if (destino > 0 && destino < size_global && destino != rank_global) {
                char msg[MAX_TEXTO];
                printf("Digite sua mensagem para Usuario_%d:\n> ", destino);
                getchar(); 
                fgets(msg, sizeof(msg), stdin);
                
                // Remove quebra de linha
                msg[strcspn(msg, "\n")] = 0;
                
                // Envia mensagem diretamente para o usuário destinatário
                MPI_Send(msg, strlen(msg) + 1, MPI_CHAR, destino, TAG_MENSAGEM_PRIVADA, MPI_COMM_WORLD);
                printf(ANSI_COLOR_GREEN "Mensagem enviada para Usuario_%d!\n" ANSI_COLOR_RESET, destino);
            } else {
                printf(ANSI_COLOR_RED "Rank de destino inválido.\n" ANSI_COLOR_RESET);
            }
            
        } else if (opcao == 4) {
            // Opção 4: Visualizar mensagens recebidas (chat)
            visualizar_mensagens_chat();
            
        } else if (opcao == 5) {
            // Opção 5: Sair do editor
            int msg_sair[2] = {0, 0};
            MPI_Send(msg_sair, 2, MPI_INT, MASTER, TAG_SAIR, MPI_COMM_WORLD);
            usuario_ativo = 0; 
            printf("Você saiu. Aguardando o encerramento seguro do programa...\n");
        }
    }
    printf("[%s] Saindo...\n", nome_usuario);
}

// Função para verificar mensagens assíncronas (atualizações e mensagens privadas)
int verificar_mensagens_e_atualizacoes() {
    int flag;
    MPI_Status status;
    int houve_atualizacao_doc = 0;
    
    // Buffers para armazenar mensagens privadas recebidas
    char buffer_msgs_privadas[10][MAX_TEXTO];
    int remetentes_msgs[10];
    int contador_msgs = 0;

    while(1) {
        // Verifica se recebeu sinal de finalização do mestre
        MPI_Iprobe(MASTER, TAG_FINALIZAR, MPI_COMM_WORLD, &flag, &status);
        if (flag) {
            int dummy;
            MPI_Recv(&dummy, 1, MPI_INT, MASTER, TAG_FINALIZAR, MPI_COMM_WORLD, &status);
            return 1;  // Retorna 1 para sinalizar finalização
        }
        
        // Verifica se há atualizações do documento
        MPI_Iprobe(MASTER, TAG_ATUALIZACAO, MPI_COMM_WORLD, &flag, &status);
        if (flag) {
            // Recebe documento e status de bloqueios atualizados
            MPI_Recv(documento, MAX_LINHAS * MAX_TEXTO, MPI_CHAR, MASTER, TAG_ATUALIZACAO, MPI_COMM_WORLD, &status);
            MPI_Recv(linhas_em_uso, MAX_LINHAS, MPI_INT, MASTER, TAG_ATUALIZACAO, MPI_COMM_WORLD, &status);
            houve_atualizacao_doc = 1;
            continue; 
        }

        // Verifica se há mensagens privadas de outros usuários
        MPI_Iprobe(MPI_ANY_SOURCE, TAG_MENSAGEM_PRIVADA, MPI_COMM_WORLD, &flag, &status);
        if (flag && status.MPI_SOURCE != MASTER) {
            char buffer_msg_temp[MAX_TEXTO];
            MPI_Recv(buffer_msg_temp, MAX_TEXTO, MPI_CHAR, status.MPI_SOURCE, TAG_MENSAGEM_PRIVADA, MPI_COMM_WORLD, &status);
            
            // Adiciona mensagem ao chat
            adicionar_mensagem_chat(status.MPI_SOURCE, buffer_msg_temp);
            
            // Também mantém para exibição imediata (compatibilidade)
            if (contador_msgs < 10) {
                strcpy(buffer_msgs_privadas[contador_msgs], buffer_msg_temp);
                remetentes_msgs[contador_msgs] = status.MPI_SOURCE;
                contador_msgs++;
            }
            continue; 
        }
        
        break; // Não há mais mensagens pendentes
    }

    // Exibe notificação se houve atualização do documento
    if (houve_atualizacao_doc) {
        printf(ANSI_COLOR_YELLOW "\n>>> O estado do sistema foi atualizado. <<<\n" ANSI_COLOR_RESET);
        mostrar_documento();
    }

    // Exibe mensagens privadas recebidas com interface formatada
    if (contador_msgs > 0) {
        for (int i = 0; i < contador_msgs; i++) {
            printf("\n\n" ANSI_COLOR_MAGENTA "  +--------------------------------------------------------------------------+\n");
            printf("  | >>> MENSAGEM PRIVADA RECEBIDA de Usuario_%d                               |\n", remetentes_msgs[i]);
            printf("  +--------------------------------------------------------------------------+\n" ANSI_COLOR_RESET);
            
            // Formata a mensagem dentro da caixa com quebra de linha automática
            char* texto_completo = buffer_msgs_privadas[i];
            char* linha_atual = strtok(texto_completo, "\n");

            while (linha_atual != NULL) {
                const int LARGURA_CAIXA = 72;
                const char* ponteiro_linha = linha_atual;
                while (strlen(ponteiro_linha) > 0) {
                    int tamanho_a_imprimir = strlen(ponteiro_linha);
                    if (tamanho_a_imprimir > LARGURA_CAIXA) {
                        tamanho_a_imprimir = LARGURA_CAIXA;
                        // Tenta quebrar em espaço para não cortar palavras
                        while (tamanho_a_imprimir > 0 && ponteiro_linha[tamanho_a_imprimir] != ' ') {
                            tamanho_a_imprimir--;
                        }
                        if (tamanho_a_imprimir == 0) {
                            tamanho_a_imprimir = LARGURA_CAIXA;
                        }
                    }
                    
                    printf(ANSI_COLOR_MAGENTA "  | " ANSI_COLOR_RESET "%.*s", tamanho_a_imprimir, ponteiro_linha);
                    for (int k = tamanho_a_imprimir; k < LARGURA_CAIXA; k++) {
                        printf(" ");
                    }
                    printf(ANSI_COLOR_MAGENTA " |\n" ANSI_COLOR_RESET);

                    ponteiro_linha += tamanho_a_imprimir;
                    if (*ponteiro_linha == ' ') {
                        ponteiro_linha++;
                    }
                }
                linha_atual = strtok(NULL, "\n");
            }

            printf(ANSI_COLOR_MAGENTA "  +--------------------------------------------------------------------------+\n\n" ANSI_COLOR_RESET);
        }
    }

    return 0;  // Continua execução normal
}

// Nova função para visualização em tempo real do documento
void visualizacao_tempo_real() {
    char nome_usuario[50];
    sprintf(nome_usuario, "Usuario_%d", rank_global);
    char input;
    int primeira_exibicao = 1;
    
    printf(ANSI_COLOR_GREEN "\n=== MODO VISUALIZAÇÃO EM TEMPO REAL ===\n" ANSI_COLOR_RESET);
    printf("Pressione 'Q' seguido de ENTER para sair da visualização\n\n");
    
    // Configura entrada não-bloqueante para o terminal
    system("stty cbreak");
    
    while (1) {
        // Verifica se há atualizações do documento ou mensagens
        int houve_atualizacao = 0;
        int flag;
        MPI_Status status;
        
        // Verifica atualizações do documento
        MPI_Iprobe(MASTER, TAG_ATUALIZACAO, MPI_COMM_WORLD, &flag, &status);
        if (flag) {
            // Recebe documento e status de bloqueios atualizados
            MPI_Recv(documento, MAX_LINHAS * MAX_TEXTO, MPI_CHAR, MASTER, TAG_ATUALIZACAO, MPI_COMM_WORLD, &status);
            MPI_Recv(linhas_em_uso, MAX_LINHAS, MPI_INT, MASTER, TAG_ATUALIZACAO, MPI_COMM_WORLD, &status);
            houve_atualizacao = 1;
        }
        
        // Verifica mensagens privadas
        MPI_Iprobe(MPI_ANY_SOURCE, TAG_MENSAGEM_PRIVADA, MPI_COMM_WORLD, &flag, &status);
        if (flag && status.MPI_SOURCE != MASTER) {
            char buffer_msg[MAX_TEXTO];
            MPI_Recv(buffer_msg, MAX_TEXTO, MPI_CHAR, status.MPI_SOURCE, TAG_MENSAGEM_PRIVADA, MPI_COMM_WORLD, &status);
            
            // Adiciona mensagem ao chat
            adicionar_mensagem_chat(status.MPI_SOURCE, buffer_msg);
            
            printf(ANSI_COLOR_YELLOW "\n>>> Nova mensagem de Usuario_%d: %s\n" ANSI_COLOR_RESET, status.MPI_SOURCE, buffer_msg);
            printf("Pressione 'Q' seguido de ENTER para sair da visualização\n\n");
        }
        
        // Verifica sinal de finalização
        MPI_Iprobe(MASTER, TAG_FINALIZAR, MPI_COMM_WORLD, &flag, &status);
        if (flag) {
            int dummy;
            MPI_Recv(&dummy, 1, MPI_INT, MASTER, TAG_FINALIZAR, MPI_COMM_WORLD, &status);
            break;
        }
        
        // Exibe ou re-exibe o documento se houve atualização ou é primeira vez
        if (houve_atualizacao || primeira_exibicao) {
            // Limpa a tela para re-exibir o documento atualizado
            if (!primeira_exibicao) {
                printf("\033[2J\033[H"); // Clear screen and move cursor to top
                printf(ANSI_COLOR_GREEN "=== DOCUMENTO ATUALIZADO ===\n" ANSI_COLOR_RESET);
            }
            
            mostrar_documento();
            printf(ANSI_COLOR_YELLOW "\n[%s] Visualização em tempo real - Digite 'Q' + ENTER para sair\n" ANSI_COLOR_RESET, nome_usuario);
            printf("> ");
            fflush(stdout);
            primeira_exibicao = 0;
        }
        
        // Verifica entrada do usuário (não-bloqueante)
        fd_set readfds;
        struct timeval timeout;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100ms
        
        if (select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout) > 0) {
            if (FD_ISSET(STDIN_FILENO, &readfds)) {
                if (scanf(" %c", &input) == 1) {
                    if (input == 'Q' || input == 'q') {
                        break;
                    }
                    // Limpa o buffer se não foi 'Q'
                    while (getchar() != '\n');
                }
            }
        }
        
        // Pequena pausa para não sobrecarregar a CPU
        usleep(50000); // 50ms
    }
    
    // Restaura configuração normal do terminal
    system("stty cooked");
    printf(ANSI_COLOR_GREEN "\nSaindo da visualização em tempo real...\n" ANSI_COLOR_RESET);
}

// Adiciona mensagem ao chat com timestamp
void adicionar_mensagem_chat(int remetente, const char* conteudo) {
    int indice;
    
    // Se o buffer está cheio, usa estrutura circular
    if (chat_count < MAX_MENSAGENS) {
        indice = chat_count;
        chat_count++;
    } else {
        indice = chat_inicio;
        chat_inicio = (chat_inicio + 1) % MAX_MENSAGENS;
    }
    
    // Adiciona a mensagem
    chat_mensagens[indice].remetente = remetente;
    strncpy(chat_mensagens[indice].conteudo, conteudo, MAX_TEXTO - 1);
    chat_mensagens[indice].conteudo[MAX_TEXTO - 1] = '\0';
    
    // Gera timestamp
    time_t agora = time(NULL);
    struct tm *t = localtime(&agora);
    strftime(chat_mensagens[indice].timestamp, sizeof(chat_mensagens[indice].timestamp), "%H:%M:%S", t);
}

// Exibe histórico de mensagens do chat
void visualizar_mensagens_chat() {
    char nome_usuario[50];
    sprintf(nome_usuario, "Usuario_%d", rank_global);
    
    printf(ANSI_COLOR_MAGENTA "\n  +--------------------------------------------------------------------------+\n" ANSI_COLOR_RESET);
    printf(ANSI_COLOR_MAGENTA "  |                              HISTÓRICO DE MENSAGENS                     |\n" ANSI_COLOR_RESET);
    printf(ANSI_COLOR_MAGENTA "  +--------------------------------------------------------------------------+\n" ANSI_COLOR_RESET);
    
    if (chat_count == 0) {
        printf(ANSI_COLOR_YELLOW "  | Nenhuma mensagem recebida ainda.                                         |\n" ANSI_COLOR_RESET);
        printf(ANSI_COLOR_MAGENTA "  +--------------------------------------------------------------------------+\n" ANSI_COLOR_RESET);
        return;
    }
    
    // Calcula quantas mensagens mostrar (máximo 20 para caber na tela)
    int mensagens_para_mostrar = chat_count < 20 ? chat_count : 20;
    int inicio_exibicao;
    
    if (chat_count <= MAX_MENSAGENS) {
        // Buffer ainda não deu volta completa
        inicio_exibicao = chat_count - mensagens_para_mostrar;
    } else {
        // Buffer circular completo, mostra as mais recentes
        inicio_exibicao = (chat_inicio - mensagens_para_mostrar + MAX_MENSAGENS) % MAX_MENSAGENS;
    }
    
    // Exibe as mensagens
    for (int i = 0; i < mensagens_para_mostrar; i++) {
        int indice = (inicio_exibicao + i) % MAX_MENSAGENS;
        Mensagem* msg = &chat_mensagens[indice];
        
        // Formato: [HH:MM:SS] Usuario_X: mensagem
        printf(ANSI_COLOR_MAGENTA "  | " ANSI_COLOR_YELLOW "[%s]" ANSI_COLOR_GREEN " Usuario_%d: " ANSI_COLOR_RESET, 
               msg->timestamp, msg->remetente);
        
        // Quebra linha longa se necessário
        const char* conteudo = msg->conteudo;
        int chars_restantes = 70 - 20; // 70 largura total - 20 chars do timestamp/usuario
        
        if (strlen(conteudo) <= chars_restantes) {
            printf("%-*s", chars_restantes, conteudo);
        } else {
            printf("%-*.*s", chars_restantes, chars_restantes, conteudo);
        }
        
        printf(ANSI_COLOR_MAGENTA " |\n" ANSI_COLOR_RESET);
        
        // Se mensagem é muito longa, mostra continuação
        if (strlen(conteudo) > chars_restantes) {
            const char* resto = conteudo + chars_restantes;
            while (strlen(resto) > 0) {
                int chars_linha = strlen(resto) > 68 ? 68 : strlen(resto);
                printf(ANSI_COLOR_MAGENTA "  | " ANSI_COLOR_RESET "%-*.*s", 68, chars_linha, resto);
                printf(ANSI_COLOR_MAGENTA " |\n" ANSI_COLOR_RESET);
                resto += chars_linha;
            }
        }
    }
    
    printf(ANSI_COLOR_MAGENTA "  +--------------------------------------------------------------------------+\n" ANSI_COLOR_RESET);
    
    if (chat_count > mensagens_para_mostrar) {
        printf(ANSI_COLOR_YELLOW "  Mostrando as %d mensagens mais recentes de %d total.\n" ANSI_COLOR_RESET, 
               mensagens_para_mostrar, chat_count);
    }
    
    printf("\nPressione ENTER para voltar ao menu...");
    while (getchar() != '\n'); // Limpa buffer
    getchar(); // Aguarda ENTER
}

// Lista usuários disponíveis para envio de mensagem
void listar_usuarios_disponiveis() {
    printf(ANSI_COLOR_CYAN "\n=== USUÁRIOS DISPONÍVEIS PARA ENVIO ===\n" ANSI_COLOR_RESET);
    printf("Usuários conectados no sistema:\n");
    
    for (int i = 1; i < size_global; i++) {
        if (i != rank_global) {
            printf(ANSI_COLOR_GREEN "  [%d] Usuario_%d\n" ANSI_COLOR_RESET, i, i);
        }
    }
    
    printf(ANSI_COLOR_YELLOW "\nSeu rank atual: %d (Usuario_%d)\n" ANSI_COLOR_RESET, rank_global, rank_global);
 }