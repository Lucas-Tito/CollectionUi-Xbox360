# Prévia

Reprodução da tela do console **no PC**, para ver o resultado antes de levar o pendrive até o
Xbox. Não é o protótipo de desenho — aquele está em [`../prototipo/`](../prototipo/) e serviu para
**decidir** a interface. Esta aqui serve para **conferir** o que o código já desenha.

A diferença importa: o protótipo é HTML com CSS, e a tela do console é um `canvas` de 1280×720
onde a função de desenho **espelha o `Desenhar()` do C++ chamada por chamada** — mesmo `fillRect`
no lugar do `Clear`, mesmo retângulo texturizado, mesmo contorno de foco, mesmo corte de nome.

## Como gerar

```bash
python3 prototipo/extrair.py     # uma vez: lê content.db e recorta as capas
python3 previa/montar.py         # gera previa/build/previa.html

# com homebrew e ROM (sem capa), para as abas de categoria existirem:
python3 previa/montar.py --vault ~/Downloads/vault.txt
```

O `--vault` é opcional e existe por um motivo só: o `games.json` sai do `content.db`, que **não
tem** homebrew nem ROM de emulador. Sem ele as abas "Homebrew" e "Emulador" não aparecem, e são
justamente duas das que se quer olhar.

Reaproveita a extração do protótipo de propósito: são os mesmos jogos e as mesmas capas já
recortadas, e duplicar isso seria mais uma coisa para divergir.

## O contrato

As constantes no topo do `<script>` são **cópias** de `app/main.cpp` — geometria, cores, o recorte
de 46,8% da frente. Mudou lá, muda aqui. A prévia não inventa layout; ela reproduz.

O corte do nome usa `measureText` para reproduzir o `ATGFONT_TRUNCATED`, que corta por **largura
em pixels** e não por contagem de caracteres.

A fonte não bate exatamente: no console é o `Arial_16` da ATG, um atlas de bitmap de 27 px de
altura, e aqui é uma sans do navegador aproximada em tamanho. Comprimento de texto pode variar
alguns pixels.

## O app inteiro, não só o que já existe

A prévia mostra o aplicativo **como foi decidido** em
[`../docs/interface.md`](../docs/interface.md), e é aqui que se itera antes de escrever C++ — não
o contrário. A primeira versão desta prévia espelhava o estado do código e por isso abria na grade
de jogos; está errado. **A tela inicial é a de coleções**, como sempre foi o desenho.

O que está aqui e ainda não está no console: a tela de coleções, o índice alfabético, a tela de
escolher jogos, o menu do `☰`. O que está no console e é fiel aqui: a grade de 5 por linha, a
geometria toda, o corte de nome por largura e o rodapé.

### Coleções compostas

A prévia tem os três tipos de coleção e a escada que decide quem pode ser origem de quem:

```
jogos  <  uniao  <  intersecao
```

União só aceita coleção de jogos — é o que o C++ já faz. **Cruzamento** aceita jogos ou união,
nunca outro cruzamento. Sendo ordem estrita, ciclo é impossível por construção e não há detecção
em lugar nenhum. O botão "Criar coleções de exemplo" monta o caso que motivou tudo: três coleções
de co-op, uma junção delas, e o cruzamento dessa junção com "Não zerado".

Duas coisas que a prévia existe para não deixar passar:

- **Nada de `∩` nem `∪`.** O atlas `Arial_16.xpr` vai até U+20AC e manda todo o resto para o glifo
  "desconhecido" — os símbolos sairiam como quadradinho na TV, mas apareceriam certinho aqui. A
  fórmula sai por extenso: *em todas: Co-op, Não zerado*. Vale para o `✓` (U+2713), e o `•` de
  U+2022 também está fora; o `·` de U+00B7, que o rodapé usa, está dentro (glifo 107).
- **A contagem ao vivo enquanto se escolhe.** O rodapé mostra `2 escolhidas · 22 jogos`. Um
  cruzamento esvazia fácil, e descobrir o `( 0 )` só depois de gravar é tarde demais. Nasceu
  aqui e **já está no C++**.

O que aqui ainda é proposta: a marca da coleção escolhida é um **tique** desenhado, e no console
hoje é o anel verde do `DrawScreenSpaceRect`. O tique se distingue melhor do anel de foco, que
também é verde — mas ainda não foi para o C++, e por isso vive só nesta página.

### Categorias da FreeStyle, no `LB`/`RB`

**Proposta; nada disto está no C++.** A tela de coleções ganha uma barra de categorias, e `LB`/`RB`
andam por ela. A primeira é sempre **Todos**, que é a tela de hoje, sem nenhuma mudança.

**O app sempre abre em Todos.** A categoria ativa mora na memória e em lugar nenhum além dela: não
vai para o `colecoes.txt`, não vai para a configuração, não sobrevive a sair do app. Voltar e
encontrar metade do acervo sumido, sem lembrar de ter apertado `RB` na sessão passada, parece
defeito — e o jeito de nunca pagar esse preço é não ter onde guardar.

A categoria **não muda o que a coleção tem**: ela encolhe o *universo* sobre o qual se pergunta.
A mesma "Co-op" tem 28 jogos em Todos e 15 em Xbox 360, e a contagem no card acompanha.

#### De onde sai a categoria

Do próprio `content.db`, numa coluna por item: **`ContentItems.ContentItemTab`**. Não é por pasta
nem por `ContentType` — o banco que o `biblioteca.cpp` já abre tem a resposta pronta, e é **uma
coluna a mais no `SELECT`**. Sem `settings.db`, sem juntar com `ScanPaths`, sem olhar nome de
pasta: a pasta pode se chamar qualquer coisa.

Os valores são os da tabela `Tabs` do `settings.db`, que é fixa:

| TabId | TabName |
|---|---|
| 0 | Automatic |
| 1 | XBLA |
| 2 | Xbox 360 |
| 3 | Xbox Classic |
| 4 | Homebrew |
| 5 | Emulator |
| 6 | Other |

O `ContentType` entra **só** no caso `0`, que é a FreeStyle dizendo "deduza": `0x7000` → 2,
`0xD0000` → 1, `0x5000` → 3. No dump desta casa são 5 itens em 120 (Splinter Cell, MGS V, CoD
World at War, The Cave). Sozinho ele erraria: os seis homebrews têm `0x7000`, o mesmo de um jogo
de varejo.

Uma consequência que vem da FreeStyle e não de nós: os 32 indies (`ContentType` `0x2`) estão
gravados com `Tab` **1, XBLA** — ela não tem aba "Indie", e inventar uma seria divergir dela. A
**ordem** também é dela, por `TabId`, com **XBLA antes de Xbox 360**.

**Aqui na prévia é por caminho**, porque o `games.json` não tem essa coluna — o `extrair.py` foi
escrito antes de ela importar. É um substituto que dá o mesmo resultado neste acervo, não o
desenho da coisa.

Falta confirmar com que `Tab` a FreeStyle gravou os seis homebrews: a cópia de `content.db` que
está aqui é anterior à pasta `\HOMEBREW\` existir.

#### A regra, inteira

> Uma aba existe quando **alguma coleção tem jogo nela**. Numa aba, coleção sem nada dela sai da
> tela.

As duas metades são uma coisa só, e juntas garantem que **nenhuma tecla leva a uma tela vazia**.
Repare que não é "o console tem jogo de 360": é "alguma coleção tem". A diferença aparece num
acervo com 215 jogos de 360 e nenhum marcado — a aba existiria e levaria a nada.

E no limite: com **uma** categoria só, "Todos" e ela são a mesma lista, então a barra inteira
some. Um console que só tem jogo de Xbox 360 fica com a tela de hoje, sem saber que isto existe.

Onde a aba **não** vale, de propósito:

- **Escolhendo origens** de uma junção ou cruzamento. Ali se escolhe *coleção*, e esconder uma
  porque a aba corrente a esvazia tiraria do usuário uma origem legítima — a composta vale para as
  outras abas também.
- **Adicionando jogos.** Marcar só o que a aba mostra é marcar às cegas: o jogo entraria e sumiria
  ao voltar para Todos.
- **Dentro da coleção**, `LB`/`RB` continuam sendo o salto por letra. A aba se escolhe na home e é
  herdada; por isso ela aparece no **subtítulo** — uma coleção de 40 jogos mostrando 6 sem dizer
  por quê parece defeito.

Coleção nova é vazia, e vazia não está em aba nenhuma: criar volta para Todos.

#### O desenho (tecla `T` ainda cicla os que ficaram para trás)

**Escolhido: `pilula`** — fileira de rótulos com a categoria ativa num bloco verde preenchido. É o
que se lê do sofá sem depender de uma barra de 3 px.

O canto arredondado **não custa nada de novo**: o `g_canto` do `main.cpp` já é uma máscara de
16×16 gerada no arranque, e é com ela que os cards de coleção já são arredondados hoje. A pílula
tem 32 px de altura e raio 16 — exatamente o `CANTO_N` —, então as duas pontas são a mesma textura,
virada.

Os outros quatro ficam no `T`, de registro do que foi comparado:

| nome | o que é | por que não |
|---|---|---|
| `steam` | fileira de rótulos, o ativo sublinhado pela barra verde | o sublinhado some a três metros |
| `seletor` | um rótulo só, entre setas, com os pontinhos embaixo | não dá panorama |
| `coluna` | trilho vertical à esquerda | a grade cai de 5 para 4 por linha |
| `nenhuma` | a tela de hoje, para comparar | — |

As setas do `seletor` e o tique são **desenhados**, não caracteres: o atlas `Arial_16` não garante
`‹`/`›` (U+2039/U+203A), e o que ele não tem vira quadradinho na TV.

## Desenhos de modal — `modais.html`

Página **avulsa**, abre direto no navegador, sem `montar.py` e sem dados: `previa/modais.html`.
Compara o modal de hoje com quatro alternativas, nos quatro casos em que ele aparece de verdade
(opções da coleção, opções do jogo, a confirmação de apagar e a escolha do tipo). Setas trocam o
caso e o foco, `C` liga a confirmação em botões lado a lado, clicar amplia.

O modal é uma função só no C++ — o `DesenharMenu` — servindo os quatro casos, e é daí que vêm as
queixas: 460 px fixos (um item só sobra caixa vazia, um nome comprido é cortado), título do mesmo
tamanho dos itens, foco por contorno fino, e **"Apagar esta coleção" com exatamente a aparência de
"Renomear"**.

Duas coisas tornam esta página mais fiel que a prévia:

- **Mede com o atlas de verdade.** Os avanços de cada caractere saem do `app/media/Arial_16.xpr`,
  que é o arquivo que o app carrega. A sans do navegador é ~25% mais estreita, e com ela um nome
  comprido parece caber quando no console seria cortado. Caractere sem glifo no atlas aparece
  denunciado num aviso na própria página.
- **Nada é negrito**, porque o atlas tem um peso só.

## Navegação

| tecla | botão | o que faz |
|---|---|---|
| setas | d-pad | navegar |
| `Enter` | `A` | abrir coleção, ou marcar no escolhedor |
| `Esc` | `B` | voltar |
| `X` | `X` | nova coleção |
| `M` | `☰` | renomear/apagar, ou escolher jogos de dentro da coleção |
| `Q` / `E` | `LB`/`RB` | trocar de categoria (na home) ou pular por letra (dentro) |
| `T` | — | trocar o desenho da barra de categorias (só na prévia) |
| segurar `↓` | segurar `↓` | pular por letra |

## O que é da prévia e não do app

A barra acima do canvas tem três coisas que **não existem no console**: o `?`, que abre a tabela
de botões; "Criar coleções de exemplo", para não montar tudo à mão a cada teste; e "Zerar".

A regra que separa as duas coisas é simples e vale manter: **o canvas é a tela do console.** O que
for desenhado ali é algo que o console desenharia; o que é ferramenta de desenvolvimento fica na
página, fora do quadro. Por isso o `?` do protótipo antigo, que ficava dentro do app, aqui vive na
barra do navegador — lá a página inteira era o app, aqui não.

No console, o único lugar que fala de botão é o rodapé da própria tela. A janela do `?` diz isso
nela mesma, para não confundir quem abrir a prévia sem contexto.

As coleções ficam no `localStorage` do navegador.
