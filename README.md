# vm

Implementação em C do gerenciador de memória virtual (Silberschatz, *Operating System Concepts*, 10ª ed., projeto P-51): traduz endereços lógicos de 16 bits em físicos, com tabela de páginas, 128 frames, substituição de páginas `fifo` ou `lru` e uma TLB de 16 entradas (também `fifo` ou `lru`), consultada com uma thread por entrada. Atividade de Infraestrutura de Software, CESAR School.

Autor: Rafael Carrilho (`rac4`)

## Estado da implementação

Feito: tradução de endereços, tabela de páginas, 128 frames, substituição `fifo` e `lru` de página, TLB com `fifo` e `lru`, busca na TLB com uma thread por entrada e tratamento de erros.

## Como compilar

```
make
```

Gera o executável `vm`. Para limpar: `make clean`. O Makefile compila com `gcc -pthread -Wall -Wextra -std=c11`.

## Como executar

```
./vm addresses.txt lru fifo
```

O 1º argumento é o arquivo de endereços lógicos (um por linha, de 0 a 65535), o 2º é o algoritmo de substituição de páginas (`fifo` ou `lru`) e o 3º é o algoritmo da TLB (`fifo` ou `lru`).

O programa procura o `BACKING_STORE.bin` (o armazenamento de apoio, 256 páginas de 256 bytes) primeiro na **pasta onde é executado** e, se não achar, em `testes/oficiais/BACKING_STORE.bin`. A pasta `testes/oficiais/` tem os arquivos do professor e não está no repositório.

O resultado vai para `correct.txt`, na pasta atual, e nada é impresso na tela quando tudo dá certo. São uma linha por endereço e 5 linhas de resumo:

```
Virtual address: <endereco> TLB: <entrada> Physical address: <fisico> Value: <valor>
...
Number of Translated Addresses = <total>
Page Faults = <total>
Page Fault Rate = <taxa com 3 casas>
TLB Hits = <total>
TLB Hit Rate = <taxa com 3 casas>
```

O campo `TLB:` é o índice (0 a 15) da entrada da TLB usada.

## Como funciona a tradução

O endereço vira página (`/ 256`) e deslocamento (`% 256`). O programa consulta a TLB; se não achar, consulta a tabela de páginas. Se a página não está na memória (*page fault*), ela é lida do `BACKING_STORE.bin` para um frame. Os 128 primeiros page faults ocupam os frames 0 a 127 em ordem; depois é preciso trocar uma página:

- `fifo`: sai a página do frame mais antigo, na ordem circular dos frames;
- `lru`: sai a página do frame com o acesso mais antigo (todo acesso atualiza o instante do frame).

A consulta à TLB cria 16 threads, uma por entrada. Cada uma só compara a página procurada com a sua entrada e grava o resultado na sua própria struct. A thread principal espera todas com `pthread_join` e só depois lê os resultados. Como ninguém escreve na TLB durante a busca, não há `mutex`.

Na inserção, a TLB usa:

- `fifo`: ordem circular das entradas;
- `lru`: a primeira entrada vazia e, se todas estiverem ocupadas, a de acesso mais antigo.

A entrada de uma página que sai da memória é invalidada. O endereço físico é `frame * 256 + deslocamento`.

## Tratamento de erros

O programa encerra com código `1` e uma mensagem em `stderr`, fechando os arquivos antes de sair, nos casos:

- número de argumentos diferente de 3, ou 2º e 3º argumentos diferentes de `fifo`/`lru`;
- arquivo de endereços que não abre, `BACKING_STORE.bin` que não abre em nenhum dos dois lugares, ou saída que não pode ser criada;
- arquivo de endereços vazio ou com linha em branco, texto, lixo depois do número, dois números na mesma linha ou linha com mais de 255 caracteres (`Arquivo invalido`);
- endereço negativo, maior que 65535 ou grande demais para um `long` (`Endereco invalido`);
- `BACKING_STORE.bin` incompleto, ou falha ao gravar e renomear a saída;
- falha ao criar as threads da TLB.

O `correct.txt` é escrito primeiro em `correct.txt.tmp` e só é renomeado no fim. Em qualquer erro o temporário é removido, então não sobra um `correct.txt` incompleto. Termina com código `0` quando todos os endereços são traduzidos.

## Organização do código

| Arquivo | Responsabilidade |
|---|---|
| `main.c` | todo o programa: leitura dos endereços (`lerEndereco`), páginas (`carregarPagina`, `escolherVitimaLRU`), TLB (`buscarTLB`, `verificarEntradaTLB`, `escolherVitimaTLB`, `inserirTLB`) e o `main` |
| `Makefile` | build do `vm` |
| `evidencias.log` | registro do terminal durante o desenvolvimento (`script`) |

## Como foi validado

Com `make clean && make`, sem avisos, e `testes/oficiais/addresses.txt` (1000 endereços):

| Páginas | TLB | Page Faults | TLB Hits | Conferência |
|---|---|---|---|---|
| `lru` | `fifo` | 539 | 54 | `diff` vazio contra o `correct.txt` do professor |
| `fifo` | `fifo` | 538 | 54 | sem arquivo de referência do professor |
| `lru` | `lru` | 539 | 55 | simulação em `awk`, linha a linha do campo `TLB:` |
| `fifo` | `lru` | 538 | 55 | simulação em `awk`, linha a linha do campo `TLB:` |

- Erros de uso e de arquivo de endereços: sem o 3º argumento, algoritmo inválido, arquivo inexistente, vazio, texto, `123abc`, duas colunas, linha em branco, linha longa, negativo, `65536` e número gigante dão mensagem e código `1`. Os limites `0` e `65535`, o fim de linha do Windows e a falta de quebra de linha final são aceitos.
- `BACKING_STORE.bin`: rodando de outra pasta com o disco ali dá o mesmo resultado do professor; sem o disco em nenhum dos dois lugares ou com o disco cortado em 1000 bytes dá erro. O disco da pasta atual tem prioridade sobre o de `testes/oficiais/`.
- Nenhum `correct.txt.tmp` sobrou em nenhum dos casos de erro.

Detalhes, tentativas e erros do desenvolvimento estão no relatório.

## Limitações conhecidas

- Só o `lru fifo` tem arquivo de referência do professor; as outras combinações foram conferidas por uma simulação que segue a mesma especificação do código.
- A falha de `pthread_create` e de `pthread_join` está tratada no código, mas não foi testada, porque não consegui provocá-la.
- Não rodei `valgrind` nem detector de corrida (`helgrind`, `-fsanitize=thread`). A ausência de corrida vem do desenho: as threads só leem a TLB e o `join` acontece antes de qualquer escrita.
- São 16 threads criadas por endereço (16 mil no teste de 1000 endereços). Funciona, mas não é eficiente.
- O programa não avisa qual `BACKING_STORE.bin` usou.
- Linha em branco no arquivo de endereços é erro, porque o formato é um endereço por linha.

## Ambiente

Desenvolvido e testado em WSL (Ubuntu). Compila com `gcc -pthread -Wall -Wextra -std=c11 main.c -o vm`.
