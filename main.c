// editor_texto_mpi.c
// Editor de texto colaborativo usando MPI + OpenMP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <omp.h>

#define MAX_LINHAS 1000
#define MAX_TEXTO 256
#define TAG_EDIT 1
#define TAG_MENSAGEM_PRIVADA 2
#define TAG_LOG 3
#define TAG_SAIR 4
#define MASTER 0

char documento[MAX_LINHAS][MAX_TEXTO];
int linhas_em_uso[MAX_LINHAS];

void gerar_documento() {
    #pragma omp parallel for
    for (int i = 0; i < MAX_LINHAS; i++) {
        sprintf(documento[i], "Linha %d: texto gerado automaticamente.", i);
        linhas_em_uso[i] = -1;
    }
}

void mostrar_documento() {
    for (int i = 0; i < 20; i++) {
        printf("[%d] %s\n", i, documento[i]);
    }
}

void registrar_log(const char* nome, int linha, const char* texto) {
    FILE *log = fopen("log.txt", "a");
    if (log) {
        fprintf(log, "[%s] editou linha %d: %s\n", nome, linha, texto);
        fclose(log);
    }
}

int main(int argc, char** argv) {
    int rank, size;
    char nome_usuario[50];

    MPI_Init(&argc, &argv);

    // Permite simular rank via argumento (para execução por painel tmux separado)
    if (argc > 1) {
        rank = atoi(argv[1]);
    } else {
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    sprintf(nome_usuario, "Usuario_%d", rank);

    if (rank == MASTER) {
        gerar_documento();
    }

    MPI_Bcast(documento, MAX_LINHAS * MAX_TEXTO, MPI_CHAR, MASTER, MPI_COMM_WORLD);
    MPI_Bcast(linhas_em_uso, MAX_LINHAS, MPI_INT, MASTER, MPI_COMM_WORLD);

    int sair = 0;
    while (!sair) {
        printf("\n[%s] Menu:\n", nome_usuario);
        printf("1 - Visualizar documento\n");
        printf("2 - Editar linha\n");
        printf("3 - Enviar mensagem privada\n");
        printf("4 - Sair\n> ");

        int opcao;
        scanf("%d", &opcao);

        if (opcao == 1) {
            mostrar_documento();

        } else if (opcao == 2) {
            int linha;
            char novo_texto[MAX_TEXTO];
            printf("Digite o numero da linha para editar: ");
            scanf("%d", &linha);
            getchar(); // limpar o buffer

            if (linhas_em_uso[linha] == -1) {
                linhas_em_uso[linha] = rank;
                printf("Digite o novo texto: ");
                fgets(novo_texto, MAX_TEXTO, stdin);
                novo_texto[strcspn(novo_texto, "\n")] = 0;

                strcpy(documento[linha], novo_texto);
                registrar_log(nome_usuario, linha, novo_texto);

                MPI_Bcast(documento, MAX_LINHAS * MAX_TEXTO, MPI_CHAR, rank, MPI_COMM_WORLD);
                MPI_Bcast(linhas_em_uso, MAX_LINHAS, MPI_INT, rank, MPI_COMM_WORLD);

                linhas_em_uso[linha] = -1;
            } else {
                printf("Linha ocupada por outro usuario!\n");
            }

        } else if (opcao == 3) {
            int destino;
            char msg_privada[MAX_TEXTO];
            printf("Digite o rank do destinatario (0 a %d): ", size - 1);
            scanf("%d", &destino);
            getchar();
            printf("Digite a mensagem: ");
            fgets(msg_privada, MAX_TEXTO, stdin);

            MPI_Send(msg_privada, strlen(msg_privada) + 1, MPI_CHAR, destino, TAG_MENSAGEM_PRIVADA, MPI_COMM_WORLD);

        } else if (opcao == 4) {
            sair = 1;
        }

        // Verifica se recebeu mensagem privada
        MPI_Status status;
        int flag;
        MPI_Iprobe(MPI_ANY_SOURCE, TAG_MENSAGEM_PRIVADA, MPI_COMM_WORLD, &flag, &status);
        if (flag) {
            char buffer[MAX_TEXTO];
            MPI_Recv(buffer, MAX_TEXTO, MPI_CHAR, status.MPI_SOURCE, TAG_MENSAGEM_PRIVADA, MPI_COMM_WORLD, &status);
            printf("\n[MENSAGEM PRIVADA DE Usuario_%d]: %s\n", status.MPI_SOURCE, buffer);
        }
    }

    MPI_Finalize();
    return 0;
}
