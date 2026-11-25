#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

struct buscaTLB {
    int pagina;
    int entrada;
    int encontrou;
};


#define PAGE_SIZE 256
#define NUM_PAGINAS 256
#define NUM_FRAMES 128
#define TLB_SIZE 16

int tabelaPaginas[NUM_PAGINAS];
int tabelaFrames[NUM_FRAMES];
int ultimoAcesso[NUM_FRAMES];
signed char memoriaFisica[NUM_FRAMES][PAGE_SIZE];
int paginasTLB[TLB_SIZE];
int framesTLB[TLB_SIZE];
int ultimoAcessoTLB[TLB_SIZE];
int insercaoTLB[TLB_SIZE];


int contadorInsercaoTLB = 0;

int proximoFrame = 0;

int getPaginaAtual(int endereco) {
    return endereco / PAGE_SIZE;
}

int getOffsetAtual(int endereco) {
    return endereco % PAGE_SIZE;
}

int lerEndereco(FILE *arquivo, int *endereco) {

    char linha[256];
    char *fim;

    if (fgets(linha, sizeof(linha), arquivo) == NULL) {
        return EOF;
    }

    if (strchr(linha, '\n') == NULL && !feof(arquivo)) {
        return 0;
    }

    errno = 0;
    long valor = strtol(linha, &fim, 10);

    if (fim == linha) {
        return 0;
    }

    while (isspace((unsigned char)*fim)) {
        fim++;
    }

    if (*fim != '\0') {
        return 0;
    }

    if (errno == ERANGE || valor < 0 || valor > 65535) {
        return -2;
    }

    *endereco = (int) valor;

    return 1;
}

void criarTabela(void) {
    for (int i = 0; i < NUM_PAGINAS; i++) {
        tabelaPaginas[i] = -1;
    }

    for (int i = 0; i < NUM_FRAMES; i++) {
        tabelaFrames[i] = -1;
        ultimoAcesso[i] = -1;
    }
    for (int i = 0; i < TLB_SIZE; i++) {
        paginasTLB[i] = -1;
        framesTLB[i] = -1;
        ultimoAcessoTLB[i] = -1;
        insercaoTLB[i] = -1;
    }
}

int carregarPagina(FILE *disco, int pagina, int frame) {

    int posicao = pagina * PAGE_SIZE;

    int resultado = fseek(disco, posicao, SEEK_SET);

    if (resultado != 0) {
        return 1;
    }

    int bytesLidos = fread(memoriaFisica[frame], 1, PAGE_SIZE, disco);

    if (bytesLidos != PAGE_SIZE) {
        return 1;
    }

    int paginaAntiga = tabelaFrames[frame];

    if (paginaAntiga != -1) {
        tabelaPaginas[paginaAntiga] = -1;

        for (int i = 0; i < TLB_SIZE; i++) {
            if (paginasTLB[i] == paginaAntiga) {
                paginasTLB[i] = -1;
                framesTLB[i] = -1;
            }
        }
    }

    tabelaPaginas[pagina] = frame;
    tabelaFrames[frame] = pagina;

    return 0;
}

int escolherVitimaLRU(void) {

    int vitima = 0;

    for (int i = 1; i < NUM_FRAMES; i++) {
        if (ultimoAcesso[i] < ultimoAcesso[vitima]) {
            vitima = i;
        }
    }

    return vitima;
}

int escolherVitimaTLB(int *tempos) {

    int vitima = 0;

    for (int i = 0; i < TLB_SIZE; i++) {
        if (paginasTLB[i] == -1) {
            return i;
        }

        if (tempos[i] < tempos[vitima]) {
            vitima = i;
        }
    }

    return vitima;
}

void *verificarEntradaTLB(void *arg) {

    struct buscaTLB *busca = arg;

    busca->encontrou = paginasTLB[busca->entrada] == busca->pagina;

    return NULL;
}

int buscarTLB(int pagina) {

    pthread_t threads[TLB_SIZE];
    struct buscaTLB buscas[TLB_SIZE];

    int criadas = 0;

    for (int i = 0; i < TLB_SIZE; i++) {

        buscas[i].pagina = pagina;
        buscas[i].entrada = i;
        buscas[i].encontrou = 0;

        if (pthread_create(&threads[i], NULL, verificarEntradaTLB, &buscas[i]) != 0) {
            break;
        }

        criadas++;
    }

    for (int i = 0; i < criadas; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            fprintf(stderr, "Erro ao aguardar thread\n");
            exit(1);
        }
    }

    if (criadas != TLB_SIZE) {
        return -2;
    }

    for (int i = 0; i < TLB_SIZE; i++) {
        if (buscas[i].encontrou == 1) {
            return i;
        }
    }

    return -1;
}

int inserirTLB(int pagina, int frame, char *algoritmo) {

    int entrada;

    if (strcmp(algoritmo, "lru") == 0) {
        entrada = escolherVitimaTLB(ultimoAcessoTLB);
    } else {
        entrada = escolherVitimaTLB(insercaoTLB);
    }

    insercaoTLB[entrada] = contadorInsercaoTLB++;
    paginasTLB[entrada] = pagina;
    framesTLB[entrada] = frame;

    return entrada;
}

int main(int argc, char *argv[]) {

    if (argc != 4) {
        fprintf(stderr, "Uso: ./vm <arquivo> <fifo|lru> <fifo|lru>\n");
        return 1;
    }

    if (strcmp(argv[2], "fifo") != 0 && strcmp(argv[2], "lru") != 0) {
        fprintf(stderr, "Algoritmo invalido\n");
        return 1;
    }

    if (strcmp(argv[3], "fifo") != 0 && strcmp(argv[3], "lru") != 0) {
        fprintf(stderr, "Algoritmo da TLB invalido\n");
        return 1;
    }

    FILE *arquivo = fopen(argv[1], "r");

    if (arquivo == NULL) {
        fprintf(stderr, "Erro ao abrir arquivo\n");
        return 1;
    }

    FILE *disco = fopen("BACKING_STORE.bin", "rb");

    if (disco == NULL) {
        fprintf(stderr, "Erro ao abrir disco\n");
        fclose(arquivo);
        return 1;
    }

    int endereco;
    int final;
    int quantidade = 0;
    int pageFaults = 0;


    criarTabela();
    while ((final = lerEndereco(arquivo, &endereco)) == 1) {

        int pagina = getPaginaAtual(endereco);
        int offset = getOffsetAtual(endereco);

        int entradaTLB = buscarTLB(pagina);
        int frame;

        if (entradaTLB == -2) {
            fprintf(stderr, "Erro ao criar threads\n");
            fclose(arquivo);
            fclose(disco);
            return 1;
        }

        if (entradaTLB != -1) {

            frame = framesTLB[entradaTLB];

        } else {

            if (tabelaPaginas[pagina] == -1) {

                int frameEscolhido = proximoFrame;

                if (pageFaults >= NUM_FRAMES && strcmp(argv[2], "lru") == 0) {
                    frameEscolhido = escolherVitimaLRU();
                }

                int resultado = carregarPagina(disco, pagina, frameEscolhido);

                if (resultado != 0) {
                    fprintf(stderr, "Erro ao carregar pagina\n");
                    fclose(arquivo);
                    fclose(disco);
                    return 1;
                }

                proximoFrame++;

                if (proximoFrame == NUM_FRAMES) {
                    proximoFrame = 0;
                }
                pageFaults++;
            }

            frame = tabelaPaginas[pagina];
            entradaTLB = inserirTLB(pagina, frame, argv[3]);
        }

        ultimoAcessoTLB[entradaTLB] = quantidade;
        ultimoAcesso[frame] = quantidade;
        int enderecoFisico = frame * PAGE_SIZE + offset;
        int valor = memoriaFisica[frame][offset];

        printf("Virtual address: %d TLB: %d Physical address: %d Value: %d\n",
            endereco, entradaTLB, enderecoFisico, valor);

        quantidade++;
    }

    if (final != EOF || ferror(arquivo) || quantidade == 0) {
        if (final == -2) {
            fprintf(stderr, "Endereco invalido\n");
        } else {
            fprintf(stderr, "Arquivo invalido\n");
        }
        fclose(arquivo);
        fclose(disco);
        return 1;
    }

    fclose(arquivo);
    fclose(disco);

    if (fflush(stdout) != 0 || ferror(stdout)) {
        fprintf(stderr, "Erro ao escrever saida\n");
        return 1;
    }

    return 0;
}