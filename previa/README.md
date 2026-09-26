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

Os dois botões no topo da página são do desenvolvimento, não do app: um cria coleções de exemplo
para não ter de montar tudo à mão a cada teste, e o outro zera. As coleções ficam no `localStorage`
do navegador.
