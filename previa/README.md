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

## As três abas

| | |
|---|---|
| **Todos os jogos** | o que o console desenha hoje — é a prévia de verdade |
| **Coleções** | desenhado, ainda sem código em C++ |
| **Índice A-Z** | desenhado, ainda sem código em C++ |

As duas últimas ficam marcadas em amarelo na própria tela, para ninguém confundir o que existe com
o que está planejado. Elas mostram, na linguagem visual do console, o que foi decidido em
[`../docs/interface.md`](../docs/interface.md) e ainda falta implementar.
