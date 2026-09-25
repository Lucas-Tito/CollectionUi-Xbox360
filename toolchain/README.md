# Toolchain

Como compilar um `.xex` a partir do Linux, sem VM Windows e sem Visual Studio. Provado de ponta a
ponta em 25/09/2026: o `imagexex -dump` do próprio XDK lê o resultado como *title module*, com load
address `82000000`.

## Pré-requisitos

- **Docker** (roda sem sudo)
- **cabextract**
- O **XDK 2.0.21256.x** — não está aqui e nunca estará: `/sdk/` é ignorado pelo git.

## Preparar uma vez

```bash
mkdir -p sdk && cd sdk
cabextract -d . "XBOX360 SDK 21256.3.exe"     # 5,1 GB, ~15 s
cp ../toolchain/dockerignore .dockerignore
docker build -t collectionui-xdk:light --build-arg XDK=XDK -f ../toolchain/Dockerfile .
```

A primeira construção compila um wine mínimo do zero e demora. Depois fica em cache.

## Compilar

```bash
./toolchain/build.sh caminho/do/projeto
```

O `build.sh` roda o `make` no container e devolve a posse dos arquivos ao usuário. O resultado sai
em `build/Debug/bin/<nome>.xex`.

## Três armadilhas, e por que cada correção existe

Nenhuma delas está documentada no `xdk-docker`, e as três custaram uma rodada de depuração.

**1. A poda do wine está velha.** O `light.Dockerfile` do upstream clona o wine do *master* e depois
roda `scripts/keep-needed-files.sh`, que tem uma **lista fixa** de DLLs escrita numa versão antiga.
O wine de hoje passou a quebrar o `setupapi` em cima do `cfgmgr32.dll`, que não está na lista; sem
ele o `wineboot` não consegue criar o prefixo e o build morre antes de compilar qualquer coisa, com
`unimplemented function setupapi.dll.SetupDiCreateDeviceInfoList`. **Nosso Dockerfile não poda.**
Custo: a imagem vai de 677 MB para 1,56 GB. Vale.

**2. Os headers de C e C++ não estão onde o Makefile do exemplo procura.** Neste SDK, o
`include/xbox` tem só os headers do Xbox (`xtl.h`, `d3d9.h` — 149 arquivos). Os padrão (`excpt.h`,
`stdio.h`, `<string>`, `<vector>` — 166 arquivos) vivem em
`TechPreview/Jul12Compiler/include/xbox`. Sem os dois, o `xtl.h` falha na primeira linha com
`Cannot open include file: 'excpt.h'`. O Dockerfile copia o segundo conjunto para
`/xdk/include/crt`, e o `Makefile` daqui aponta `INCLUDE` para os dois.

**3. Não tente fixar o `WINEPREFIX` na imagem.** É tentador criar o prefixo uma vez no build para
economizar segundos por execução, e era o que eu queria para poder rodar o container com `--user` e
não sujar o projeto com arquivos de root. **Não funciona:** o wine recusa um prefixo que não
pertença ao dono do processo (`'/wine-prefix' is not owned by you`), e um prefixo criado assim sai
sem o diretório `Temp` do usuário, o que derruba o `cl.exe` com
`D8037: cannot create temporary il file`. A solução é deixar o wine criar o prefixo sozinho a cada
execução, rodar como root e corrigir a posse no fim — que é o que o `build.sh` faz.

## Os arquivos

| | |
|---|---|
| `Dockerfile` | imagem com wine + XDK. Sobre o `light` do upstream, sem a poda, com D3D/XGraphics/XJSON/XNet, `fxc.exe`, `xbcp.exe` e os headers de CRT |
| `dockerignore` | copiar para `sdk/.dockerignore`; corta `art`, `Source` e `doc` do contexto (6,6 GB → 2,9 GB) |
| `Makefile` | modelo de projeto, com `INCLUDE` apontando para os dois diretórios |
| `build.sh` | compila e conserta a posse |
