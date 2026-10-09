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
```

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

## Navegação

| tecla | botão | o que faz |
|---|---|---|
| setas | d-pad | navegar |
| `Enter` | `A` | abrir coleção, ou marcar no escolhedor |
| `Esc` | `B` | voltar |
| `X` | `X` | nova coleção |
| `M` | `☰` | renomear/apagar, ou escolher jogos de dentro da coleção |
| `Q` / `E` | `LB`/`RB` | pular por letra |
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
