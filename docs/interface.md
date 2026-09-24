# Interface

Decisões de tela e de controle, fechadas em 23/09/2026 a partir do protótipo navegável. O
protótipo roda com os dados reais do console (120 jogos, capas extraídas dos `.assets`) e existe
para decidir o desenho **antes** de escrever C++, onde cada iteração custa compilar, subir por FTP
e reiniciar.

## O que foi recusado

A primeira versão foi um Big Picture cinematográfico: arte de fundo em tela cheia, desfoque,
tipografia condensada gigante, painel de metadados e trilho de capas com a focada ampliada.
Veredito do dono do projeto: **"exagerada demais pro 360"**. Fica registrado para não voltar por
distração.

O que sobrou é tela chapada, borda fina, e verde só no foco.

## As duas telas

**1. Coleções.** Quadrados com nome e contagem de jogos. É a primeira tela do app.

**2. Jogos.** A grade da coleção aberta.

Não há terceira tela. Ficha de jogo, nota e duração **não existem** neste desenho — ver
[O que ficou de fora](#o-que-ficou-de-fora).

## Tela de coleções

- **Começa vazia**, com uma linha só: *"Nenhuma coleção ainda"*. Sem parágrafo explicativo e sem
  convite na área de conteúdo.
- **Não existe quadrado de "+ Nova coleção".** Com muitas coleções ele acabaria perdido no fim da
  lista. Criar é **tecla**, não item de tela, e quem documenta isso é o rodapé.
- **Ordem alfabética**, sempre. Sem reordenar à mão e sem "as mais usadas primeiro": a posição de
  uma coleção não muda sozinha.
- O menu (☰) traz **Renomear** e **Apagar**, e só. Apagar é ação destrutiva e por isso mora dentro
  do menu, não num botão solto.

## Tela de jogos

- **5 por linha**, ordem alfabética.
- A ordenação **ignora o artigo inicial** (`the`, `a`, `o`, `os`, `as`, `um`, `uma`), então *The
  Darkness* cai no **D**. Se um dia se quiser literal, é uma função só (`chave()`).
- Nome **cortado em 24 caracteres** com reticências, numa linha. O número está numa constante
  (`MAX_NOME`).
- **Salto por letra** de dois jeitos, porque os dois foram pedidos: segurar `↓`, e os gatilhos
  `LB`/`RB`. Durante o salto, a letra aparece grande no centro da tela.
- **Índice alfabético fixo na borda direita**, de A a Z mais `#`. Ele acende a letra onde o foco
  está e apaga as letras que a coleção não contém — numa coleção de 23 jogos dá para ver de
  relance que não há nada em D nem em N. Existe para que o salto por letra **seja visível antes de
  ser apertado**, em vez de ser função escondida.

## Botões

O **rodapé é o único lugar** que documenta controle, e mostra símbolo e ação lado a lado. Uma dica
só aparece quando faz alguma coisa: com a lista de coleções vazia, o rodapé mostra apenas
`Ⓧ Nova coleção`, porque não há o que abrir nem o que apagar.

`A` confirma e `B` volta em todas as telas, que é a convenção do Xbox e ninguém espera diferente.

| tela | controle | teclado | ação |
|---|---|---|---|
| ambas | d-pad / analógico | `← ↑ ↓ →` | Navegar |
| Coleções | `A` | `Enter` | Abrir a coleção |
| Coleções | `X` | `X` (ou `N`) | Nova coleção |
| Coleções | `☰` (Menu/Start) | `M` | Renomear ou apagar |
| Jogos | `A` | `Enter` | Jogar |
| Jogos | `B` | `Esc` (ou `Backspace`) | Voltar às coleções |
| Jogos | `LB` / `RB` | `Q` / `E` | Pular por letra |
| Jogos | segurar `↓` | segurar `↓` | Pular por letra |

**`X` não faz nada dentro de uma coleção**, por decisão: só `A` para jogar e `B` para voltar.

O protótipo em HTML aceita as teclas da coluna do meio e também o mouse — clicar num quadrado ou
numa capa move o foco, clicar de novo confirma, e clicar numa letra do índice salta para ela. O
botão `?` no canto superior esquerdo abre essa mesma tabela; ele existe **só no protótipo** e não
faz parte do desenho do app.

## O que ficou de fora

- **Metacritic, tempo de jogo e ficha técnica.** O enriquecimento que vem do
  [xbox-vault](https://github.com/Lucas-Tito/xbox-vault) não aparece em lugar nenhum do desenho
  atual. Foi consequência de enxugar a interface, e é uma perda real: 96 dos 120 jogos têm nota e
  91 têm duração. O lugar natural, se voltar, é uma ficha fora do caminho de quem só quer jogar.
- **Escolher jogo a jogo ao criar a coleção.** No protótipo, criar uma coleção pede um nome e um
  gênero, e ela nasce com os jogos daquele gênero. É atalho de protótipo, não desenho final: a
  tela de escolher os jogos ainda não existe.

## Em aberto

- **O corte do nome.** Em 24 caracteres, dois jogos da biblioteca real viram
  "Call of Duty: Modern Wa…" e ficam indistinguíveis lado a lado. As saídas são aumentar o corte
  ou deixar o nome em duas linhas.
- **A ficha do jogo**, se o enriquecimento do vault vai voltar e por qual botão.
- **A tela de escolher jogos** ao montar uma coleção.
