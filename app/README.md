# app

O aplicativo. Hoje está na **v0**: um teste de fogo do toolchain, que é o degrau antes de
começar a interface desenhada em [`../docs/interface.md`](../docs/interface.md).

## O que a v0 faz

Inicializa o D3D9 em 720p e pinta a tela inteira com uma cor que percorre o círculo de matiz,
apresentando quadro a quadro. Escreve também um log em `game:\collectionui.log`.

São dois canais de propósito, porque cada um responde uma pergunta diferente:

| canal | responde |
|---|---|
| a tela | **funcionou?** — dá para ver do sofá, sem ferramenta nenhuma |
| o log | **por que não?** — puxa-se por FTP, a mesma conexão que subiu o `.xex` |

Cada falha possível tem aparência própria:

- **console não sai do dashboard** → o `.xex` não rodou
- **tela preta, parada** → rodou, mas o dispositivo D3D não iniciou — o log diz o `hr`
- **tela de uma cor só, congelada** → iniciou, mas o `Present` não está acontecendo
- **cor mudando** → os três passos funcionaram

O `main` nunca retorna: no Xbox 360, sair de `main` tira o título do ar, então uma falha entra
em laço vazio em vez de derrubar o console — assim o log fica legível.

## Compilar

```bash
./toolchain/build.sh app                 # Debug
./toolchain/build.sh app CONFIG=Release  # Release
```

Sai em `build/<CONFIG>/bin/collectionui.xex`. O Debug é bem maior (1,3 MB contra 331 KB) porque
leva o D3D de depuração, que valida as chamadas — vale a pena enquanto se está construindo.

## Instalar no console

Copiar o `.xex` para uma pasta do HD (por FTP, com o FreeStyle ligado) e abrir pela lista de
aplicativos. Para ler o log depois, puxar `collectionui.log` da mesma pasta.
