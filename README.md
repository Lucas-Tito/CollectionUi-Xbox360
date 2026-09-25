# CollectionUi-Xbox360
Quero fazer programa, um app mesmo, pro xbox 360, pra fazer uma expiriência de coleção de jogos similar ao da steam no modo big picture. Dá pra usar os dados do xbox vault pra saber as ferramentas que a gente precisa usar.

A ideia é primeiro medir o esforço (e se é possível), fazer um prototipo da ui daí começar o desenvolvimento, um .xex já basta.

---

## Onde isto está

O levantamento respondeu as duas perguntas acima: **é possível**, e o caminho está mapeado. O
toolchain compila `.xex` no Linux, a interface foi desenhada e aprovada num protótipo, e os dados
do console já são lidos e documentados.

| | |
|---|---|
| Viabilidade | respondida — estimativa de 2 a 4 semanas de fins de semana |
| Dados do console | medidos e documentados, não supostos |
| Lançar jogo | resolvido, com implementação de referência |
| Interface | desenhada, com o mapeamento de botões fechado |
| Toolchain | **compila `.xex` de ponta a ponta no Linux** |
| Primeiro `.xex` no console | **rodou** — 25/09/2026, D3D em 720p, laço de quadros e log em arquivo |
| Falta | a interface, em C++ |

## Os documentos

| | |
|---|---|
| [`docs/viabilidade.md`](docs/viabilidade.md) | o parecer técnico: toolchain, de onde vêm os dados, formatos do FreeStyle, esforço por fase e riscos |
| [`docs/decisoes.md`](docs/decisoes.md) | tudo que foi batido, rodada a rodada, com o que foi recusado e por quê |
| [`docs/interface.md`](docs/interface.md) | o desenho das telas e o mapeamento de botões |
| [`toolchain/README.md`](toolchain/README.md) | como compilar um `.xex` do Linux, e as três armadilhas do caminho |
| [`prototipo/README.md`](prototipo/README.md) | o protótipo navegável da interface e como reconstruí-lo |

## O que não está aqui

- **O XDK.** É software proprietário da Microsoft, 6,6 GB, e `/sdk/` é ignorado pelo git. O
  `toolchain/` traz a receita, não o conteúdo.
- **Os dados do console.** A cópia do FreeStyle (bancos e 1 GB de arte) fica em
  `~/Documentos/CollectionUI-dados`, fora do repositório.

