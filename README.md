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

O programa lê o `BACKING_STORE.bin` (o armazenamento de apoio, 256 páginas de 256 bytes) somente na **pasta onde é executado**. Os arquivos do professor ficam em `testes/oficiais/`, fora do repositório; para rodar com eles, execute a partir dessa pasta ou copie o `BACKING_STORE.bin` para a pasta atual.

As traduções são impressas na **saída padrão**, uma linha por endereço, na ordem do arquivo, sem estatísticas e sem outras mensagens:

```
Virtual address: <endereco> TLB: <entrada> Physical address: <fisico> Value: <valor>
```

O mesmo conteúdo é gravado também em `correct.txt` (na pasta atual, aberto direto com `fopen`, sem arquivo temporário), seguido de 5 linhas de resumo que **não** vão para a saída padrão:

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

- primeiro a menor entrada livre (posições 0 a 15 são preenchidas em ordem; a entrada invalidada por uma expulsão é reaproveitada antes de qualquer substituição);
- com a TLB cheia, `fifo`: a entrada inserida há mais tempo (ordem de inserção);
- com a TLB cheia, `lru`: a entrada de acesso mais antigo (todo acesso conta, inclusive os hits).

A entrada de uma página que sai da memória é invalidada. O endereço físico é `frame * 256 + deslocamento`.

## Tratamento de erros

O programa encerra com código `1` e uma mensagem em `stderr`, fechando os arquivos antes de sair, nos casos:

- número de argumentos diferente de 3, ou 2º e 3º argumentos diferentes de `fifo`/`lru`;
- arquivo de endereços que não abre, `BACKING_STORE.bin` que não abre na pasta atual, ou `correct.txt` que não pode ser criado;
- arquivo de endereços vazio ou com linha em branco, texto, lixo depois do número, dois números na mesma linha ou linha com mais de 255 caracteres (`Arquivo invalido`);
- endereço negativo, maior que 65535 ou grande demais para um `long` (`Endereco invalido`);
- `BACKING_STORE.bin` incompleto, ou falha ao gravar a saída;
- falha ao criar as threads da TLB.

A saída não passa por arquivo temporário. Por isso, se o erro aparecer no meio do arquivo de endereços, as linhas já traduzidas ficam na saída padrão e o `correct.txt` fica incompleto, sem as 5 linhas de resumo. Termina com código `0` quando todos os endereços são traduzidos.

## Organização do código

| Arquivo | Responsabilidade |
|---|---|
| `main.c` | todo o programa: leitura dos endereços (`lerEndereco`), páginas (`carregarPagina`, `escolherVitimaLRU`), TLB (`buscarTLB`, `verificarEntradaTLB`, `escolherVitimaTLB`, `inserirTLB`) e o `main` |
| `Makefile` | build do `vm` |
| `evidencias.log` | registro do terminal durante o desenvolvimento (`script`) |

## Adaptação ao Greep

O enunciado do Greep difere do original do Classroom: saída na saída padrão, sem estatísticas, preenchimento da TLB da posição 0 à 15 com entradas livres primeiro, e `BACKING_STORE.bin` só na pasta de execução. A primeira entrega violou o seccomp do corretor; depois de remover o arquivo temporário, o `rename` e o `remove`, o erro sumiu (a syscall exata não foi confirmada). O corretor passou então a exigir o `correct.txt`, apesar do enunciado pedir só a saída padrão, e por isso o programa escreve nos dois lugares. Com isso, o Greep deu 100% nos testes públicos (resultado informado pelo autor). Commits: `2eec2db` e `f803a9d`. Detalhes na seção 11 do relatório e no final do `evidencias.log`.

## Como foi validado

Com `make clean && make`, sem avisos, e `testes/oficiais/addresses.txt` (1000 endereços), na versão final (`f803a9d`), executado no WSL pelo autor com os arquivos oficiais do professor (resultados informados pelo autor após a execução):

| Páginas | TLB | Page Faults | TLB Hits | Saída |
|---|---|---:|---:|---|
| `lru` | `fifo` | 539 | 54 | 0; `diff` completo do `correct.txt` contra o gabarito: `IGUAL` |
| `lru` | `lru` | 539 | 55 | 0 |
| `fifo` | `fifo` | 538 | 54 | 0 |
| `fifo` | `lru` | 538 | 55 | 0 |

Todas as combinações traduziram 1000 endereços. Só o `lru fifo` tem gabarito do professor. Os números são os mesmos da versão entregue originalmente (`5d4d2d1`), que também foi conferida por simulação em `awk`. O `evidencias.log` traz ainda testes com dados sintéticos de outro ambiente (container Linux), identificados como tal.

- Erros de uso e de arquivo de endereços: sem o 3º argumento, algoritmo inválido, arquivo inexistente, vazio, texto, `123abc`, duas colunas, linha em branco, linha longa, negativo, `65536` e número gigante dão mensagem e código `1`. Os limites `0` e `65535`, o fim de linha do Windows e a falta de quebra de linha final são aceitos.
- `BACKING_STORE.bin`: sem o disco na pasta atual ou com o disco cortado em 1000 bytes dá erro.

Detalhes, tentativas e erros do desenvolvimento estão no relatório.

## Limitações conhecidas

- Só o `lru fifo` tem arquivo de referência do professor; as outras combinações foram conferidas por uma simulação que segue a mesma especificação do código.
- A falha de `pthread_create` e de `pthread_join` está tratada no código, mas não foi testada, porque não consegui provocá-la.
- Não rodei `valgrind` nem detector de corrida (`helgrind`, `-fsanitize=thread`). A ausência de corrida vem do desenho: as threads só leem a TLB e o `join` acontece antes de qualquer escrita.
- São 16 threads criadas por endereço (16 mil no teste de 1000 endereços). Funciona, mas não é eficiente.
- O programa grava o `correct.txt` mesmo com a saída padrão já impressa; a duplicação existe só por causa do corretor do Greep.
- Linha em branco no arquivo de endereços é erro, porque o formato é um endereço por linha.

## Ambiente

Desenvolvido e testado em WSL (Ubuntu). Compila com `gcc -pthread -Wall -Wextra -std=c11 main.c -o vm`.
