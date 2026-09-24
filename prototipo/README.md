# Protótipo da interface

Protótipo navegável em HTML da tela de coleção, alimentado com os **dados reais do console** —
os 120 jogos e as capas extraídas dos `.assets` do FreeStyle. Existe para decidir o desenho antes
de escrever C++, onde cada iteração custa compilar, subir por FTP e reiniciar.

O desenho que ele fixa está em [`../docs/interface.md`](../docs/interface.md).

## Como reconstruir

Precisa da cópia dos dados do console em `~/Documentos/CollectionUI-dados` (ver o `LEIA-ME.md`
de lá) e do **Pillow**:

```bash
python3 prototipo/extrair.py                      # lê content.db + GameData -> build/
python3 prototipo/montar.py                       # junta tudo num .html autocontido
```

O resultado é `prototipo/build/collectionui.html`, que abre por duplo clique — sem servidor e sem
pasta ao lado, porque as capas entram embutidas.

Para incluir nota do Metacritic e tempo de jogo, aponte para os `data/*.json` do
[xbox-vault](https://github.com/Lucas-Tito/xbox-vault):

```bash
python3 prototipo/extrair.py --vault /caminho/do/xbox-vault/data
```

Casam 96 dos 120 por title id mais nome normalizado. A interface atual **não usa** esses campos
(ver *O que ficou de fora* em `../docs/interface.md`), mas eles saem de graça e ficam no
`games.json` para quando a ficha voltar.

## Os arquivos

| | |
|---|---|
| `template.html` | a interface: tela de coleções, grade de 5 por linha, escolhedor de jogos, índice alfabético. É o fonte que se edita. |
| `extrair.py` | lê `content.db`, parseia o container **FSDA** e recorta a capa de dentro do encarte |
| `montar.py` | embute as capas em `data:` URI e escreve o `.html` final |
| `build/` | gerado, fora do git |

## O recorte da capa

O detalhe que dá mais trabalho: o asset de tipo **128** do FSDA não é a capa, é o **encarte
inteiro** — contracapa, lombada e frente, 900×600. A frente são os **46,8% da direita**, número
calibrado à mão contra a biblioteca real. Está na constante `CROP_FRENTE`.

Os DDS de dentro são **DXT5 e decodificam direto**, sem swizzle — o que também vale para o app
nativo depois: a arte do console sai limpa para qualquer lugar.
