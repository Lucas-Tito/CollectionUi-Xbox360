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

# com as coleções do console (o colecoes.txt tirado por FTP), que vencem as do navegador:
python3 previa/montar.py --vault vault.txt --colecoes colecoes.txt
```

O `--vault` é opcional e existe por um motivo só: o `games.json` sai do `content.db`, que **não
tem** homebrew nem ROM de emulador. Sem ele as abas "Homebrew" e "Emulador" não aparecem, e são
justamente duas das que se quer olhar.

Reaproveita a extração do protótipo de propósito: são os mesmos jogos e as mesmas capas já
recortadas, e duplicar isso seria mais uma coisa para divergir.

## O contrato

As constantes no topo do `<script>` são **cópias** de `app/main.cpp` — geometria, cores, escalas de
fonte. Mudou lá, muda aqui. A prévia não inventa layout; ela reproduz. (O recorte de 46,8% da
frente do encarte não está aqui: ele é aplicado offline, pelo `prototipo/extrair.py`, nas imagens
que esta página carrega já recortadas.)

### O texto é o do atlas, desenhado como a GPU desenha

A página traz o próprio `app/media/Arial_16.xpr`, lido pelo `previa/atlas.py` na montagem: a
textura 4_4_4_4, os glifos e a tabela de tradução. O `texto()` refaz o `DrawText` da ATG glifo a
glifo (cursor no floor da origem, deslocamento, largura, avanço, meio pixel do vertex shader) e o
pixel shader dela pixel a pixel: amostra bilinear, **depois** o corte (acima de 0,5 é a letra,
abaixo é a sombra preta com alfa), vezes a cor, teste de alfa e mistura. Por isso a sombra que o
atlas traz aparece aqui como na TV, e os botões do rodapé são os glifos coloridos do próprio
atlas (máscara 0), na mesma string que o C++ monta.

Conferido contra a foto do console tirada pelo roteiro (17/11): o rodapé e o menu ficam a no máximo
10 de 255, com 96 a 99% dos pixels a até 2. Antes, com a Arial do navegador esticada, a largura
batia mas a forma, o peso e a sombra não.

O que o atlas não tem sai como o glifo 0 (quadradinho) e é listado na barra de cima, em vermelho.
A pontuação tipográfica inteira está fora dele: nem `’` `‘` `“` `–` `—` `…`, nem `®` `©` `™`.

### As cores são os números do C++

Até 17/11 não eram. O `bdeac35` clareou o fundo e o card do console por uma curva de 1/1,3 (uma
rampa de gama suposta, nunca medida) e deixou na prévia os números de antes, então a prévia era
mais escura que o console no mesmo pixel. Agora a prévia usa os números do C++, e o fundo é o quad
com cor por vértice que o C++ desenha, `t = (x/1280 + y/720) / 2`, não o gradiente do canvas ao
longo da diagonal (que dá outro tom no meio da tela). Na foto, o fundo bate até 1 de 255.

O que a TV faz com a imagem depois (gama, nível de preto) a prévia não reproduz, nem a foto do
roteiro mostra: as duas são o que o app desenha.

### Os dados

A prévia só é o espelho com os dados do console. `--colecoes` embute o `colecoes.txt` dele, que
vence o que o navegador guardou; `--vault` traz homebrew e ROM. A biblioteca (jogos e capas) ainda
vem da extração do `prototipo/extrair.py` sobre a cópia em `CollectionUI-dados`, que é de quando o
console tinha 120 itens: o `montar.py` diz quantos TitleIds das coleções ficaram sem item.

## Proposta: por cima, nunca dentro

Como a prévia é o espelho, proposta entra **por cima**, num arquivo à parte, e gera outra página:

```bash
python3 previa/montar.py --vault ~/Downloads/vault.txt \
        --proposta previa/soltos.js --saida previa/build/soltos.html
```

O `.js` é carregado depois do `<script>` da prévia e reescreve as funções que precisa. Assim há uma
implementação só do desenho, a prévia continua mostrando só o que o `.xex` faz, e a proposta vive
num arquivo cujo nome diz o que ela é — some junto quando a decisão for tomada.

Hoje há uma: **`soltos.js`**, a aba à esquerda do "Todos" com os jogos que não estão em coleção
nenhuma, e o arquivamento de um jogo em várias coleções de uma vez.

## Espelho, não proposta

Esta página é o **espelho do que o `.xex` desenha hoje**. Não há mais nada aqui marcado como
"proposta": tudo o que ela mostra está no C++, e o que foi comparado no caminho e não venceu saiu
da página e ficou no histórico — os cinco desenhos de barra de categoria no commit `f3d8d8b`, os
fundos e cards alternativos e o tique de "escolhida" junto com eles, os cinco desenhos de modal no
`c783678`.

Sai caro manter as duas coisas na mesma tela: nada distinguia "o console faz assim" de "isto é só
uma ideia", e quem critica a partir daqui acaba aprovando ou reprovando a tela errada. Para
comparar desenho **antes** de decidir, a saída é uma página avulsa, como a `modais.html` foi.

Duas coisas continuam sendo substituto assumido, e nenhuma delas se vê na tela:

- **A categoria de cada jogo sai do caminho da pasta**, porque o `games.json` não tem a coluna
  `ContentItemTab` que o `biblioteca.cpp` lê. Dá o mesmo resultado neste acervo.
- **O teclado** é um `<input>` de HTML; no console é o teclado do sistema, que desenha por cima do
  app. O balão de aviso do sistema (`XNotifyQueueUI`) também não é coisa que o canvas desenhe, e
  por isso aparece na barra da página.

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

## O modal — `modais.html`

Página **avulsa**, abre direto no navegador, sem `montar.py` e sem dados: `previa/modais.html`.
**Já está no C++** (decisão 145): a página virou o espelho do que o `DesenharMenu` desenha, e
não mais uma proposta. Mostra os **cinco casos** em que o modal aparece — opções da coleção, opções do jogo, a confirmação
de apagar, o exportar que o Back abre e a escolha do tipo —, todos no desenho que foi escolhido. Setas movem o
foco, `C` troca a confirmação pelos dois botões lado a lado, `E` tira o exportar do Back, clicar
amplia.

O modal é uma função só no C++ — o `DesenharMenu` — servindo os quatro casos, e é daí que vinham as
queixas: 460 px fixos (um item só sobra caixa vazia, um nome comprido é cortado), título do mesmo
tamanho dos itens e foco por contorno fino.

**O escolhido:** painel centralizado com rótulo e nome no cabeçalho, linhas de borda a borda,
largura seguindo o conteúdo e véu leve (172), que deixa a grade aparecer por trás. A linha em foco
é **neutra** — faixa branca a 9% com a barra verde. A destrutiva **não tem cor própria parada**:
ela é uma linha como as outras até receber o foco, e só então a faixa, a barra e o texto ficam
vermelhos. Cor parada em item de menu vira enfeite; o que o vermelho precisa marcar é o instante
em que o `A` faria aquilo.

Os outros quatro desenhos comparados no caminho (hoje, pílula, seco, lateral, cartão) e os cinco
realces estão no commit `c783678`, se precisar rever.

### O `Exportar para o Vault` sai do menu

O `exportar.cpp` recebe `g_jogos`: ele exporta a **biblioteca inteira** e todas as coleções. Dentro
de um menu intitulado com o nome de uma coleção, ele diz o contrário do que faz. O **Back não é
usado em lugar nenhum do app** — não há um `XINPUT_GAMEPAD_BACK` no `main.cpp` — e o glifo dele
está no atlas, com os mesmos 54 px do A e do Start. A página põe `Back Exportar` no rodapé da
tela de coleções e tira a linha do menu; o `E` desfaz, para comparar.

O Back **não exporta na hora**: abre um modal, *Exportar — Biblioteca e coleções para o Xbox Vault*,
com "Exportar" e "Cancelar". O rodapé não tem largura para a frase inteira, e o `game:\vault.txt`
é reescrito por cima do anterior — um Back sem querer, sem nada perguntando, faria isso calado.
Nada ali é destrutivo, então nada fica vermelho.

Duas consequências, se isto for para o C++: o menu da coleção fica com três linhas e a destrutiva
passa a ser a **última**, e o `☰` sem nenhuma coleção criada deixa de ter o que oferecer — hoje ele
abre só com o "Exportar", e passaria a só tocar o som de volta.

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

## O que esta página já pegou

Coisas que só apareceram depois que a medida passou a ser a do atlas:

- **Dez nomes da biblioteca têm caractere que o atlas não desenha** e saem com um quadradinho na
  TV: `Army of Two™: TFD`, `Dante's Inferno™`, `Far Cry® 3`, `Rainbow Six® Vegas 2`,
  `Rayman® Legends`, `Need for Speed™ Carbon`, `Spider-Man™: FoF`,
  `Tom Clancy's Splinter Cell® Blacklist™`, `Tom Clancy’s Splinter Cell® Conviction` (com
  apóstrofo tipográfico) e `BLAZBLUE　CONTINUUM SHIFT EXTEND` (espaço ideográfico). São `™`, `®`,
  `’` e `U+3000`.
- **A marca de "coleção escolhida" é o mesmo verde do anel de foco**, com 2 px contra 3. Escolhendo
  origens, distinguir a marcada da focada depende da espessura do traço.
