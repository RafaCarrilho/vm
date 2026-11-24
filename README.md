# vm

Implementação em C do gerenciador de memória virtual (Silberschatz, *Operating System Concepts*, 10ª ed., projeto P-51): traduz endereços lógicos de 16 bits em físicos, com tabela de páginas, 128 frames, substituição de páginas `fifo` ou `lru` e uma TLB de 16 entradas. Atividade de Infraestrutura de Software, CESAR School.

Autor: Rafael Carrilho (`rac4`)

## Estado da implementação

Feito: tradução de endereços, tabela de páginas, 128 frames, substituição `fifo` e `lru` de página, TLB com `fifo`.

**Pendente:** TLB com `lru` (hoje o programa recusa `lru` no 3º argumento) e a busca na TLB com uma thread por entrada. Este README será atualizado quando esses itens forem concluídos.

## Como compilar

```
make
```

Gera o executável `vm`. Para limpar: `make clean`. O Makefile compila com `gcc -pthread -Wall -Wextra -std=c11` (o `-pthread` já está lá por causa das threads previstas).

## Como executar

```
./vm addresses.txt lru fifo
```

O 1º argumento é o arquivo de endereços lógicos (um por linha, de 0 a 65535), o 2º é o algoritmo de substituição de páginas (`fifo` ou `lru`) e o 3º é o da TLB (hoje só `fifo`).

O programa lê, **a partir da pasta onde é executado**, o `testes/oficiais/BACKING_STORE.bin` (o armazenamento de apoio, 256 páginas de 256 bytes; caminho fixo no código). A pasta `testes/oficiais/` tem os arquivos do professor e não está no repositório.

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

A TLB insere em ordem circular, e a entrada de uma página que sai da memória é invalidada. O endereço físico é `frame * 256 + deslocamento`.

## Tratamento de erros

O programa encerra com código `1` e uma mensagem em `stderr`, fechando os arquivos antes de sair, nos casos:

- número de argumentos diferente de 3, 2º argumento diferente de `fifo`/`lru`, 3º diferente de `fifo`;
- arquivo de endereços ou `BACKING_STORE.bin` que não abre, ou saída que não pode ser criada;
- arquivo de endereços vazio ou com algo que não é número, ou endereço fora de 0 a 65535;
- falha ao ler uma página ou ao gravar e renomear a saída.

O `correct.txt` é escrito primeiro em `correct.txt.tmp` e só é renomeado no fim. Em qualquer erro o temporário é removido, então não sobra um `correct.txt` incompleto. Termina com código `0` quando todos os endereços são traduzidos.

## Organização do código

| Arquivo | Responsabilidade |
|---|---|
| `main.c` | todo o programa: `carregarPagina`, `escolherVitimaLRU`, `buscarTLB`, `inserirTLB` e o `main` (argumentos, laço de tradução, saída) |
| `Makefile` | build do `vm` |
| `evidencias.log` | registro do terminal durante o desenvolvimento (`script`) |

## Como foi validado

- `make clean && make` sem avisos e `./vm testes/oficiais/addresses.txt lru fifo`: o `correct.txt` é **idêntico** ao do professor (`diff` vazio), com 539 page faults e 54 TLB hits. O 539 também foi conferido por uma simulação independente em `awk`.
- `fifo fifo`: 538 page faults e 54 TLB hits. O professor só deu o arquivo de `lru fifo`, então não há comparação direta.
- Argumentos inválidos (sem o 3º, página `xyz`, TLB `lru`) e arquivo de endereços inválido (`abc`, ou `16916` seguido de `-5`): mensagem, código `1` e nenhum temporário sobrando.

Ainda **não** testados: TLB com `lru`, threads, `BACKING_STORE.bin` ausente e `valgrind`. Detalhes e decisões estão no relatório.

## Limitações conhecidas

- A TLB só aceita `fifo`; o `lru` e as threads estão pendentes.
- O caminho do `BACKING_STORE.bin` é fixo e relativo à pasta onde o programa roda.
- O arquivo de endereços é lido com `fscanf("%d")`: dois números na mesma linha seriam aceitos e um número gigante tem comportamento indefinido (nenhum dos dois foi testado).
- A mensagem de uso cita só o arquivo e o algoritmo, não o 3º argumento.

## Ambiente

Desenvolvido e testado em WSL (Ubuntu). Compila com `gcc -pthread -Wall -Wextra -std=c11 main.c -o vm`.
