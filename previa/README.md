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

A categoria **não muda o que a coleção tem**: ela encolhe o *universo* sobre o qual se pergunta.
A mesma "Co-op" tem 28 jogos em Todos e 15 em Xbox 360, e a contagem no card acompanha.

#### De onde sai a categoria

A FreeStyle tem uma tabela fixa de abas, no `settings.db`:

| TabId | TabName |
|---|---|
| 0 | Automatic |
| 1 | XBLA |
| 2 | Xbox 360 |
| 3 | Xbox Classic |
| 4 | Homebrew |
| 5 | Emulator |
| 6 | Other |

e cada **raiz de varredura** aponta para uma delas, em `ScanPaths.ScanPathRetailTabId`. Neste
console: `\JOGOS\` → 2, `\XBLA\` → 1, `\ORIGINAL XBOX\` → 3, `\EMULADORES\` → 5,
`\Content\0000000000000000\584E07D2\` → 1.

**Quem decide é a raiz, não o `ContentType`.** Os seis homebrews deste console têm `ContentType`
`0x7000`, o mesmo de um jogo de varejo; só a raiz `\HOMEBREW\` os separa. Então a conta certa no
C++ é o `content.db` (`ContentItemScanPathId`, que o `SELECT` de hoje ainda não lê) **juntado** com
o `settings.db` (`ScanPaths` → `Tabs`). O `ContentType` sozinho erra.

Duas consequências que vêm da FreeStyle e não de nós: os 32 indies (`ContentType` `0x2`) caem em
**XBLA**, porque é para lá que a raiz deles aponta — a FreeStyle não tem aba "Indie", e inventar
uma seria divergir dela; e a **ordem** é a da tabela, por `TabId`, com **XBLA antes de Xbox 360**.

A prévia não tem o `settings.db`, então deriva do **caminho** — que é de onde a varredura saiu, e
dá o mesmo resultado neste acervo.

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

#### Os cinco desenhos (tecla `T`)

| nome | o que é | custo no console |
|---|---|---|
| `steam` | fileira de rótulos, o ativo sublinhado pela barra verde | texto + um `fillRect` de 3 px |
| `pilula` | fileira de pílulas, a ativa preenchida | canto arredondado = textura 9-slice |
| `seletor` | um rótulo só, entre setas, com os pontinhos embaixo | o mais barato; nunca estoura a largura |
| `coluna` | trilho vertical à esquerda | a grade cai de 5 para 4 por linha |
| `nenhuma` | a tela de hoje, para comparar | — |

O `steam` é o desenho da biblioteca da Steam no Big Picture e é o **default da prévia**: é o que dá
panorama e o único que não cobra nem textura nem layout.

As setas do `seletor` e o tique são **desenhados**, não caracteres: o atlas `Arial_16` não garante
`‹`/`›` (U+2039/U+203A), e o que ele não tem vira quadradinho na TV.

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
