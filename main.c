#include <stdio.h>
#include <string.h>

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


int proximaEntradaTLB = 0;

int proximoFrame = 0;

int getPaginaAtual(int endereco) {
    return endereco / PAGE_SIZE;
}

int getOffsetAtual(int endereco) {
    return endereco % PAGE_SIZE;
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

int buscarTLB(int pagina) {

    for (int i = 0; i < TLB_SIZE; i++) {
        if (paginasTLB[i] == pagina) {
            return i;
        }
    }

    return -1;
}

int inserirTLB(int pagina, int frame) {

    int entrada = proximaEntradaTLB;

    paginasTLB[entrada] = pagina;
    framesTLB[entrada] = frame;

    proximaEntradaTLB++;

    if (proximaEntradaTLB == TLB_SIZE) {
        proximaEntradaTLB = 0;
    }

    return entrada;
}

int main(int argc, char *argv[]) {

    if (argc != 4) {
        fprintf(stderr, "Informe o arquivo de enderecos e o algoritmo\n");
        return 1;
    }

    if (strcmp(argv[2], "fifo") != 0 && strcmp(argv[2], "lru") != 0) {
        fprintf(stderr, "Algoritmo invalido\n");
        return 1;
    }

    if (strcmp(argv[3], "fifo") != 0) {
        fprintf(stderr, "Algoritmo da TLB invalido\n");
        return 1;
    }

    FILE *arquivo = fopen(argv[1], "r");

    if (arquivo == NULL) {
        fprintf(stderr, "Erro ao abrir arquivo\n");
        return 1;
    }

    FILE *disco = fopen("testes/oficiais/BACKING_STORE.bin", "rb");

    if (disco == NULL) {
        fprintf(stderr, "Erro ao abrir disco\n");
        fclose(arquivo);
        return 1;
    }

    FILE *saida = fopen("correct.txt.tmp", "w");

    if (saida == NULL) {
        fprintf(stderr, "Erro ao criar arquivo de saida\n");
        fclose(arquivo);
        fclose(disco);
        return 1;
    }

    int endereco;
    int final;
    int quantidade = 0;
    int pageFaults = 0;
    int tlbHits = 0;


    criarTabela();
    while ((final = fscanf(arquivo, "%d", &endereco)) == 1) {

        if (endereco < 0 || endereco > 65535) {
            fprintf(stderr, "Endereco invalido\n");
            fclose(arquivo);
            fclose(disco);
            fclose(saida);
            remove("correct.txt.tmp");
            return 1;
        }

        int pagina = getPaginaAtual(endereco);
        int offset = getOffsetAtual(endereco);

        int entradaTLB = buscarTLB(pagina);
        int frame;

        if (entradaTLB != -1) {

            frame = framesTLB[entradaTLB];
            tlbHits++;

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
                    fclose(saida);
                    remove("correct.txt.tmp");
                    return 1;
                }

                proximoFrame++;

                if (proximoFrame == NUM_FRAMES) {
                    proximoFrame = 0;
                }
                pageFaults++;
            }

            frame = tabelaPaginas[pagina];
            entradaTLB = inserirTLB(pagina, frame);
        }

        ultimoAcesso[frame] = quantidade;
        int enderecoFisico = frame * PAGE_SIZE + offset;
        int valor = memoriaFisica[frame][offset];

        fprintf(saida, "Virtual address: %d TLB: %d Physical address: %d Value: %d\n",
            endereco, entradaTLB, enderecoFisico, valor);

        quantidade++;
    }

    if (final != EOF || ferror(arquivo) || quantidade == 0) {
        fprintf(stderr, "Arquivo invalido\n");
        fclose(arquivo);
        fclose(disco);
        fclose(saida);
        remove("correct.txt.tmp");
        return 1;
    }

    double pageFaultRate = (double) pageFaults / quantidade;
    double tlbHitRate = (double) tlbHits / quantidade;

    fprintf(saida, "Number of Translated Addresses = %d\n", quantidade);
    fprintf(saida, "Page Faults = %d\n", pageFaults);
    fprintf(saida, "Page Fault Rate = %.3f\n", pageFaultRate);
    fprintf(saida, "TLB Hits = %d\n", tlbHits);
    fprintf(saida, "TLB Hit Rate = %.3f\n", tlbHitRate);

    fclose(arquivo);
    fclose(disco);

    int erroSaida = ferror(saida);

    if (fclose(saida) != 0) {
        erroSaida = 1;
    }

    if (erroSaida != 0) {
        fprintf(stderr, "Erro ao escrever arquivo\n");
        remove("correct.txt.tmp");
        return 1;
    }

    if (rename("correct.txt.tmp", "correct.txt") != 0) {
        fprintf(stderr, "Erro ao renomear arquivo\n");
        remove("correct.txt.tmp");
        return 1;
    }

    return 0;
}