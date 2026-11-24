#include <stdio.h>

#define PAGE_SIZE 256
#define NUM_PAGINAS 256
#define NUM_FRAMES 256

int tabelaPaginas[NUM_PAGINAS];
signed char memoriaFisica[NUM_FRAMES][PAGE_SIZE];

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

    tabelaPaginas[pagina] = frame;

    return 0;
}

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Informe o arquivo de enderecos\n");
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

    int endereco;
    int final;
    int quantidade = 0;


    criarTabela();
    while ((final = fscanf(arquivo, "%d", &endereco)) == 1) {

        if (endereco < 0 || endereco > 65535) {
            fprintf(stderr, "Endereco invalido\n");
            fclose(arquivo);
            fclose(disco);
            return 1;
        }

        int pagina = getPaginaAtual(endereco);
        int offset = getOffsetAtual(endereco);

        printf("Address: %d Page: %d Offset: %d\n",
               endereco, pagina, offset);

        quantidade++;
    }

    if (final != EOF || ferror(arquivo) || quantidade == 0) {
        fprintf(stderr, "Arquivo invalido\n");
        fclose(arquivo);
        fclose(disco);
        return 1;
    }

    fclose(arquivo);
    fclose(disco);

    return 0;
}