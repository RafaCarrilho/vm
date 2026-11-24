#include <stdio.h>

#define PAGE_SIZE 256

int getPaginaAtual(int endereco) {
    return endereco / PAGE_SIZE;
}

int getOffsetAtual(int endereco) {
    return endereco % PAGE_SIZE;
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

    int endereco;
    int final;
    int quantidade = 0;

    while ((final = fscanf(arquivo, "%d", &endereco)) == 1) {

        if (endereco < 0 || endereco > 65535) {
            fprintf(stderr, "Endereco invalido\n");
            fclose(arquivo);
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
        return 1;
    }

    fclose(arquivo);

    return 0;
}