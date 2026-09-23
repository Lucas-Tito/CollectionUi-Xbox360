# Viabilidade e esforço

Levantamento feito em 15/09/2026, antes de escrever código, para responder as duas perguntas do
README: **é possível?** e **quanto custa?**

Premissas confirmadas com o dono do projeto:

- Console **RGH/JTAG**, rodando **FreeStyle Dash**, que já lança qualquer `.xex`.
- O app **não** substitui o dashboard: é algo que a pessoa abre quando quer, como um jogo.
- Voltar para o app ao sair de um jogo **não** faz parte da experiência.
- O que o app mostra é a **biblioteca instalada** — o modelo do Steam Big Picture —, não o
  catálogo completo do console. Ver [De onde vêm os dados](#de-onde-vêm-os-dados).

## Resposta curta

É possível, e o caminho já está trilhado: existe homebrew nativo de Xbox 360 recebendo commit
**esta semana** (o [X-Store](https://github.com/951261/X-Store)), existe uma biblioteca de apoio
mantida ([XexUtils](https://github.com/ClementDreptin/XexUtils)), existe **Dear ImGui portado para
o console** e existe como **compilar de Linux, dentro de Docker**, sem VM Windows.

O que não existe é caminho legalmente limpo: o único toolchain maduro é o XDK oficial da
Microsoft, software proprietário que circula vazado. O substituto livre existe e se propõe
exatamente a isso, mas está em estágio inicial. Detalhe em [Toolchain](#toolchain).

Estimativa: **2 a 4 semanas de fins de semana** até algo usável no sofá. Esse número é baixo
porque a maior parte dos dados **já está no console** e porque a biblioteca instalada são dezenas
ou poucas centenas de itens, não 6.900.

## Pré-requisito: rodar código não assinado

Já resolvido — RGH/JTAG é permanente e o FreeStyle lança `.xex` direto da lista de aplicativos.
Não é preciso publicar o app na seção Jogos do dashboard original (existe a técnica do
[GameShortcut](https://github.com/ClementDreptin/GameShortcut) via BLAST, mas ela só serve para
quem usa o dash de fábrica).

Para registro: o outro jeito de rodar homebrew é o **BadUpdate/ABadAvatar**, exploit só de
software, não persistente — teria que ser refeito pelo USB antes de cada sessão. Não é o caso
aqui.

## Toolchain

### A. XDK + VS2010 — o caminho real

É o que FreeStyle Dash, Aurora, XeXMenu, Dashlaunch e X-Store usam. Produz `.xex` que roda **sob o
kernel do Xbox**, e por isso tem acesso ao `xam.xex` — inclusive a `XamLoaderLaunchTitle` /
`XLaunchNewImage`, que é **como se lança um jogo**. Sem isso não há launcher.

Gráficos são Direct3D 9 com duas diferenças que importam: não há pipeline fixo (tudo é shader) e
texturas são tiled por padrão, com formatos `_LIN_` para escrever linear como no PC.

O ecossistema que reduz o trabalho, quase todo de
[ClementDreptin](https://github.com/ClementDreptin):

- **[xdk-docker](https://github.com/ClementDreptin/xdk-docker)** — Dockerfiles que montam uma
  imagem com o XDK + wine + make. Resolve estarmos em Linux. Ver
  [Sobre o Docker](#sobre-o-docker-resposta-direta) para as limitações.
- **[imgui-xbox360](https://github.com/ClementDreptin/imgui-xbox360)** — backends de Dear ImGui
  para o D3D9 do 360 + XInput. Trava em **Dear ImGui v1.86** (a v1.87 derrubou suporte a
  compilador pré-C++11/VS2010) e o `DisplaySize` é fixo em 720p, porque as outras resoluções saem
  do scaler de hardware.
- **[XexUtils](https://github.com/ClementDreptin/XexUtils)** — a biblioteca padrão que falta:
  HTTP/HTTPS, JSON, filesystem e devices, input com repeat, Xam, Dashlaunch, STFS, detour.
- **[XboxPlayground](https://github.com/ClementDreptin/XboxPlayground)** — app mínimo, serve de
  template.
- **[ModdingResources](https://github.com/ClementDreptin/ModdingResources)** — a documentação que
  não existe em outro lugar. O `COMPILING.md` do X-Store aponta para cá.

E o [X-Store](https://github.com/951261/X-Store) como referência de app real, com `docs/` de
arquitetura e runtime. Ressalva: a UI dele é **texto em console** (`dprintf`), não gráfica.

**O custo deste caminho é o XDK**, sem o qual nada acima compila.

### B. libxenon — não serve para este projeto

É a opção livre e legal do Free60, e seria a resposta óbvia, mas o modelo de execução mata o caso
de uso: roda **bare-metal**, carregado pelo XeLL, sem o kernel do Xbox e sem `xam`. Na prática o
binário é um `.elf32` que vai na raiz do USB como `xenon.elf`, os dashboards não reconhecem, e
**não há como lançar um jogo de varejo** — não existe kernel embaixo para receber o pedido. Ótimo
para emuladores e ports, inútil para um launcher.

### C. OpenXeChain — o caminho legal, ainda não pronto

É o substituto livre do XDK e roda em POSIX: um [fork do LLVM](https://github.com/OpenXeChain/llvm)
que targeta o 360, newlib, [xecorelib](https://github.com/OpenXeChain/xecorelib) com os imports de
`xam`/kernel (inclusive o launch de título) e o
[SynthXEX](https://github.com/OpenXeChain/SynthXEX), que constrói o XEX2 final — feito a partir de
conhecimento público e hex editor, sem decompilar código da Microsoft.

O próprio README do SynthXEX diz: *"early development and MANY features are missing (most notable
exports)"*, e a última atividade é de outubro de 2025. É onde o projeto deveria querer estar em um
ano; apostar nele agora é assumir que você vai debugar o toolchain além do seu app.

## De onde vêm os dados

**Medido em 23/09/2026, com os arquivos reais.** O pendrive com a instalação do FreeStyle foi
aberto no PC e inspecionado. O que segue não é leitura de fonte nem suposição: é o que está no
console.

### Qual instalação está em uso

Havia duas no pendrive, e a distinção importa porque só uma tem dados vivos:

| pasta | `default.xex` | último jogo lançado | veredito |
|---|---|---|---|
| `Freestyle.780/` | 16.531.456 bytes | **07/09/2026 14:15** | **é a ativa** |
| `FreeStyle/` | 16.728.064 bytes | 09/09/2020 22:02 | abandonada há seis anos |

A prova vem de dentro do banco (`RecentlyPlayedTitles.RecentlyPlayedTitleDateTime`, timestamp
unix), não do sistema de arquivos: **os timestamps do FAT mentem aqui**, porque o Xbox 360 grava
`2005-11-22` quando está com o relógio zerado, e essa data aparece espalhada pelas duas pastas.

### O banco: `content.db`

Fica em `<instalação>/Data/Databases/`, ao lado de `settings.db`. **Não** se chama `fsd2data.db` —
esse era o nome no fonte do FSD 2, e a suposição anterior estava errada.

**Não é criptografado:** os primeiros bytes são `SQLite format 3`.

Tabelas: `ContentItems`, `ContentTypes`, `Favorites`, `HttpQueue`, `MountedDevices`,
`RecentlyPlayedTitles`, `TitleUpdates`, `UserRatings` — exatamente as do fonte de 2011, com os
mesmos nomes de coluna. **A arqueologia no FSD 2 valeu:** o FSD 3 só acrescentou dois campos.

`ContentItems` — o schema real, 120 linhas nesta instalação:

```sql
CREATE TABLE ContentItems (
  ContentItemId          INTEGER PRIMARY KEY,
  ContentItemScanPathId  INTEGER,
  ContentItemFileType    INTEGER,   -- 1 = XEX solto, 3 = container (STFS/GOD)
  ContentItemContentType INTEGER,
  ContentItemTab         INTEGER,
  ContentItemPath        TEXT,      -- relativo ao dispositivo: \JOGOS\COD Black Ops\default.xex
  ContentItemDirectory   TEXT,
  ContentItemFileName    TEXT,
  ContentItemTitleId     INTEGER,   -- decimal; 1096157269 = 0x41560817
  ContentItemMediaId     INTEGER,
  ContentItemDiscNum     INTEGER,
  ContentItemDiscsInSet  INTEGER,
  ContentItemName        TEXT,
  ContentItemDescription TEXT,
  ContentItemDeveloper   TEXT,
  ContentItemPublisher   TEXT,
  ContentItemGenre       TEXT,
  ContentItemRating      TEXT,      -- nota do Marketplace, ex. "4.25"
  ContentItemRaters      TEXT,      -- nº de avaliadores, ex. "582947"
  ContentItemReleaseDate TEXT,
  ContentItemHash        TEXT,      -- novo no FSD 3
  ContentItemKinectFlag  INTEGER,   -- novo no FSD 3
  UNIQUE (ContentItemPath)
)
```

O `ContentItemFileType` separa exatamente os dois ramos do `LaunchGame()` (ver
[O lançamento de jogo](#o-lançamento-de-jogo-resolvido)).

As outras tabelas, com o que havia nesta instalação:

- `RecentlyPlayedTitles` — **218 linhas**, com `...DateTime` em timestamp unix e `...Order`. A
  fileira "jogados recentemente" sai daqui pronta.
- `TitleUpdates` — 71 linhas. `Favorites` e `UserRatings` — vazias.
- `MountedDevices` — mapeia o GUID do dispositivo para o nome: `Flash:`, `OnBoardMU:`, `Hdd1:`,
  `HddX:`, `SysExt:`. É o que permite montar o caminho absoluto, já que `ContentItemPath` é
  relativo ao dispositivo, com o vínculo vindo de `ContentItemScanPathId`.

### A arte: `GameData/` e o container FSDA

**O FSD 3 tirou a arte do banco.** No fonte do FSD 2 havia uma tabela `Assets` com BLOB de PNG/JPG;
ela não existe mais. A arte está em `Data/GameData/<id em hex>/`, uma pasta por jogo:

```
000000C4.assets      container com as imagens
GameCoverInfo.bin    JSON (apesar da extensão) com as capas disponíveis no XboxUnity
GameAssetInfo.bin    XML
GameOfferInfo.bin    XML
PluginData/
```

O `.assets` é um container próprio, **big-endian**, com header `FSDA`:

```
+0   char[4]   "FSDA"
+4   uint32    versão (1)
+8   uint32    0
+12  uint32    máscara de bits dos tipos presentes (ex.: 0xAF)
+16  uint32    número de entradas da tabela
+20  uint32    2
+24  tabela de entradas, 16 bytes cada:
              uint32 tipo, uint32 offset, uint32 tamanho, uint32 tamanho (repetido)
```

Cada `offset` aponta para um **DDS** dentro do próprio arquivo — e aqui está o achado que muda o
projeto: **tudo já está em DXT5**, textura comprimida que a GPU do 360 amostra direto.

Os tipos, medidos nos 120 jogos desta instalação:

| tipo | o que é | dimensão mais comum | cobertura |
|---|---|---|---|
| **128** | **capa grande** | **900×600** | **120/120** |
| 2 | fundo | **1920×1080** | 51/120 |
| 1 | ícone | 64×64 | 109/120 |
| 64 | não identificado | 420×320 | 37/120 |
| 8 | capa pequena | 220×300 | 30/120 |
| 4 | banner | 420×96 | 29/120 |

São **991 MB de arte para 120 jogos**, uns 8 MB por jogo. **Todos os 120 têm capa grande**, e
quase metade tem fundo em Full HD.

Uma observação para quem for implementar o parser: além das entradas da tabela aparecem DDS
contíguos ao fim do arquivo que **não** estão listados (num caso, dois de 1000×564, que são
screenshots). A máscara de tipos do header acende um bit a mais do que o número de entradas, então
provavelmente há um tipo — o 32 — que agrupa várias imagens sob uma entrada só. Detalhe a resolver
na implementação; não é bloqueante.

O `GameCoverInfo.bin` é JSON e traz o **title id em hexadecimal, como string**:

```json
[{"titleid":"58410B52","name":"Warhammer 40,000: Kill Team","official":true,
  "url":"http://assets.xboxunity.net/api/boxart/2371", ...}]
```

É exatamente o formato que o `data/x360db.json` do xbox-vault usa.

### O que essa medição apagou do plano

1. **Não há pipeline de imagem.** As capas já estão em 900×600 e os fundos em 1080p, no console.
   A preocupação com capa de 240px era do acervo web do vault e não se aplica aqui.
2. **Não há conversão para DDS.** Já é DXT5 — o cache em runtime que estava previsto é
   desnecessário.
3. **Não há incerteza de schema.** O banco foi aberto e lido.


### O lançamento de jogo, resolvido

Era o maior risco do projeto e o fonte do FSD 2 entrega pronto, em
`ContentItemNew::LaunchGame()`:

```cpp
// XEX/XBE solto: lança direto
if (fileType == CONTENT_FILE_TYPE_XEX || fileType == CONTENT_FILE_TYPE_XBE) {
    PrepareForMultiDiscLaunch();
    ConsolidateTitleUpdates();
    XLaunchNewImage((itemRoot + itemPath).c_str(), 0);
}
// STFS/GOD: valida contentType, abre o container e lança por ele
Xbox360Container container;
if (container.OpenContainer(itemRoot + itemPath) != S_OK) return;
container.CloseContainer();
PrepareForMultiDiscLaunch();
ConsolidateTitleUpdates();
container.LaunchGame();
```

E o `xboxtools.cpp` ainda resolve a dúvida da API: sobre o `XamLoaderLaunchTitle`, o comentário
deles é *"not really needed, just use xlaunchnewimage"*.

**Ressalva de licença:** o FSD é **GPLv3**. Ler o fonte para descobrir qual API chamar é uma
coisa; copiar a implementação para o nosso app torna o projeto derivado e obrigatoriamente GPLv3.
É escolha a fazer conscientemente, não por descuido.

**Detalhe de produto que caiu daí:** o `LaunchGame()` chama `FSDSql::updateRecentlyPlayed()` antes
de lançar. Como decidimos abrir o banco só para leitura, um jogo lançado pelo nosso app não entra
nos "recentes" — nem nos do FSD, nem nos nossos, que vêm da mesma tabela. Ou se relaxa a regra
nessa tabela, ou se mantém um registro próprio ao lado. Em aberto.

### O papel do xbox-vault

Com nome, id e capa vindo do console, o [xbox-vault](https://github.com/Lucas-Tito/xbox-vault)
deixa de ser a fonte do catálogo e passa a ser **enriquecimento**: nota do Metacritic, tempo de
jogo do HowLongToBeat com contagem de relatos, modos de jogo, co-op e número de jogadores — coisas
que o FSD não tem e que são justamente o que faz uma tela de detalhe valer a pena.

A chave de junção já existe no repo: **`data/x360db.json` tem `titleId`** em 100% das suas 4.892
entradas (1.579 de Xbox 360 e 3.313 de XBLIG). O `GameCoverInfo.bin` do FSD traz o id em hex como
string, no mesmo formato do vault; o `ContentItemTitleId` do banco vem em decimal e precisa de
conversão (`1096157269` → `41560817`).

**Medido contra a biblioteca real:** dos 120 jogos instalados, **93 casaram por title id, ou 78%**.
Os 27 que ficaram de fora se explicam, e cada grupo tem saída conhecida:

- **Emuladores** (`Super Nintendo`, `Playstation 1`, `Arcade`) — não são jogos; o FSD os indexa
  como conteúdo. Não deveriam casar mesmo.
- **Jogos de Xbox original** rodando por retrocompatibilidade (Mortal Kombat: Armageddon, Star
  Wars Battlefront 2, Quake 2, Digimon Rumble Arena 2…). Estão no `xbox.json` do vault, não no
  `x360db.json` — é só olhar no arquivo certo.
- **Jogos de 360 sem `titleId` no x360db** (Rainbow Six Vegas 2, Army of Two, Minecraft, Terraria).
  Caem no fallback por nome normalizado, que o `norm()` do `tools/taglib.py` já implementa.

Isso é um arquivo compacto indexado por title id, gerado por um script novo em `tools/`. É
trabalho de um dia, e é **opcional para a v1**: o app roda só com o que o FSD dá.

### Se um dia você quiser o catálogo inteiro

Aí voltam os problemas do site: 6.900 títulos, capas em 240px WebP (que o 360 não decodifica) e
2,5 MB de JSON, exigindo pipeline de PC, conversão em lote para DDS e formato binário. É um modo
"explorar o que existe" ao lado do "minha biblioteca", e deve ser tratado como uma segunda fase
com orçamento próprio, não como parte da v1.

## Panorama das dashboards abertas

Levantado no `data/homebrew.json` do xbox-vault mais busca no GitHub. O resultado é mais pobre do
que se imagina:

| dashboard | fonte | serve de base? |
|---|---|---|
| **Freestyle Dash 2.0 RC2.1** | aberto, GPLv3, despejo único de 2011 | **é a única aberta que lança jogo de varejo** |
| Xemini, Xenu, XMENU | abertos | **não** — são libxenon, rodam bare-metal e não lançam jogo de varejo |
| Aurora, FSD 3, XeXMenu, XexDash, Viper360, IngeniouX, XeXLoader, 360Menu | fechados | não |
| Emerald Dash | dito "open source", sem repositório localizável | não |

Ou seja: para Xbox 360 existe **uma** dashboard aberta capaz de lançar jogo, e ela é de 2011. O
contraste com o Xbox original é gritante — lá há NevolutionX, neXgen, UIX/UIX Lite, LithiumX,
Theseus, PrometheOS, XBMC4Xbox e derivados, todos abertos e vivos.

### Veredito sobre forkar

**Não forkar, usar como referência.** Mesmo ignorando que o código é de 2011, os motivos são
estruturais e sobreviveriam a qualquer versão mais nova: a UI do FSD é **XUR**
(`SkinManager::loadScene("Main.xur")`), então o fork troca o trabalho de ImGui por trabalho de
XuiTool em vez de eliminá-lo; são 495 arquivos `.cpp/.h` próprios e 2,8 MB de código; e
arquitetonicamente é um *dashboard* — tem cena de FileBrowser, DualPane, CopyDVD, Achievements,
AvatarRenderer —, de modo que boa parte do esforço seria apagar coisa até sobrar o que queremos.

### Modificar o FreeStyle instalado, sem o fonte

Diferente de forkar: partir dos **arquivos que já estão no console**. Há três camadas, da mais
barata para a mais cara.

**1. Skins (`.xzp` com XUR) — dados, feitos para serem trocados.** O `SkinManager` do FSD carrega a
cena do arquivo de skin (`loadScene("Main.xur")`), e trocar isso não exige fonte nem patch: é o que
a cena de skins faz há anos. Muda a **aparência**, não o comportamento. Autorar XUR pede o XuiTool
do XDK ou o [XUIHelper](https://github.com/SGCSam/XUIHelper) open source.

**2. Plugin DLL injetado no dash em execução — o caminho de verdade.** O
[imgui-xbox360](https://github.com/ClementDreptin/imgui-xbox360) traz um **exemplo de DLL** que faz
exatamente isso, em `examples/dll/main.cpp`:

```cpp
#define DASH_TITLE_ID 0xFFFE07D1          // mira o dashboard
Detour *g_pXuiRenderEndDetour = NULL;      // detoura o fim do render do XUI
// pega o D3DDevice do título em execução, importado de xam.xex pelo ordinal 2095
XuiRenderGetDevice = ResolveExport("xam.xex", 2095);
```

Injeta a DLL, engancha no loop de render e desenha ImGui **por cima** da UI existente. Não se
reescreve o dash: monta-se a interface em cima dele, herdando de graça a varredura, as capas, o
banco e o lançamento. O `XexUtils` já tem `Detour`. O carregamento em RGH/JTAG é pelo **Dashlaunch**
(o [X360PluginManager](https://github.com/ClementDreptin/X360PluginManager) existe para quem está no
BadUpdate e não tem Dashlaunch).

**Ressalva:** aquele `0xFFFE07D1` é o dashboard oficial da Microsoft, não o FSD, que é um título
comum com title id próprio. O mecanismo é o mesmo e o FSD também renderiza via XUI, então o gancho
tem tudo para valer — mas isso é **hipótese a testar**, não fato verificado.

**3. Patch binário do `.xex` — possível, e a cena já faz.** A prova está no próprio
`data/homebrew.json` do xbox-vault: *"Freestyle Dash 3 - Fixed (Unofficial) — build comunitário do
FSD 3.0.775 com as falhas conhecidas corrigidas"*. Alguém corrigiu bugs do FSD 3 **sem o fonte**.
Mas é desempacotar o XEX, achar o código no IDA e remendar assembly: o caminho mais caro e o mais
frágil dos três.

#### O que muda se o projeto virar um plugin

**Ganha:** não precisa varrer disco, nem baixar capa, nem fazer arqueologia do schema do banco — o
código roda *dentro* do processo que mantém tudo isso —, e o risco do lançamento de jogo
desaparece.

**Perde:** deixa de ser "abre como um jogo" e vira uma camada sobre o dash, contrariando a decisão
1; fica acoplado a um binário fechado, com gancho que pode quebrar a cada atualização do FSD; e
depurar DLL injetada é mais chato que depurar app próprio.

**Não muda:** os três caminhos continuam exigindo o **XDK** — skin precisa do XuiTool, plugin
precisa compilar DLL, patch precisa do ferramental.

### O que o Aurora tem de melhor, e vale registrar

O Aurora é fechado, mas seus formatos estão **melhor documentados e mais atuais** que os do FSD 3,
porque o ferramental aberto em volta dele é mantido:

- **[AuroraDbManager](https://github.com/XboxUnity/AuroraDbManager)** traz o schema em C#: a tabela
  `ContentItems` do Aurora tem `TitleId`, `TitleName`, `Directory`, `Executable`, `FileType`,
  `ContentType`, `Description`, `Developer`, `Publisher`, `ReleaseDate`, `DiscNum`, `DiscsInSet`,
  `MediaId`, `GenreFlag`, `GameCapsOnline`, `GameCapsOffline`, `LiveRating`, `LiveRaters`,
  `SystemLink`, `DateAdded`, `Hash` — mais rico que o do FSD 2.
- **[AuroraAssetEditor](https://github.com/XboxUnity/AuroraAssetEditor)** (atualizado em jan/2026)
  documenta os `.asset`, que guardam **D3DTexture** — isto é, a capa **já em formato de textura de
  GPU**, o que eliminaria a etapa de decodificar PNG/JPG e converter para DDS.
- **[Documentação de desenvolvedor do Aurora](https://github.com/jrobiche/xbox360-aurora-developer-documentation)**
  cobre assets, banco e bibliotecas Lua, e documenta a **Nova**, uma **API HTTP com definição
  OpenAPI**, com endpoints de `Filebrowser`, `Title`, `Image`, `Profile`, `Achievement`,
  `Dashlaunch`, `Memory`, `System` e mais.

Essa última é a resposta completa para "existe leitura remota dos arquivos do console": no
**Aurora**, existe e é uma API HTTP documentada; no **FreeStyle**, o que há é o FTP embutido, e o
WebUI do plugin não navega arquivos.

Nada disso é recomendação de trocar de dashboard — é registro de que, do ponto de vista de *ler a
biblioteca*, o FSD 3 é engenharia reversa de um arquivo e o Aurora é schema documentado com
ferramenta viva.

## Sobre o Docker (resposta direta)

Sim, achei: **[ClementDreptin/xdk-docker](https://github.com/ClementDreptin/xdk-docker)**. Duas
imagens, `light` (Ubuntu, wine mínimo compilado do fonte, subconjunto do XDK, pensado para CI) e
`full` (Alpine, XDK inteiro, ambiente de desenvolvimento).

Três limitações que valem saber antes de contar com ele:

- **Não há imagem pré-construída.** O repositório só tem Dockerfiles, e o build exige
  `--build-arg XDK=<caminho>` apontando para um XDK já instalado. Ele elimina a dependência de
  **Windows**, não a do XDK.
- **Só `make`.** Não tem MSBuild, logo os projetos `.vcxproj` do imgui-xbox360 e do XexUtils não
  são usados como estão: precisamos escrever Makefiles. Trabalho pequeno, mas existe.
- Testado com XDK `2.0.21256.17`; qualquer `2.0.21256.XX` deve servir.

## Esforço, por fase

| # | Fase | Estimativa | O que prova |
|---|---|---|---|
| 0 | Imagem Docker do XDK e um `.xex` "hello" rodando no console | 1–2 dias | que o ciclo compilar → FTP → rodar fecha |
| 1 | ImGui na tela, navegação por gamepad | 2–4 dias | que dá para iterar UI |
| 2 | Ler o `content.db` e parsear o FSDA: lista real com as capas na tela | 2–4 dias | que os dados do console chegam na tela |
| 3 | Lançar o jogo de verdade | 2–4 dias | que é um launcher, não um álbum |
| 4 | Enriquecer com o vault (Metacritic, HLTB, modos) por title id | 1–2 dias | a tela de detalhe |
| 5 | Acabamento: ordenação, busca, som, transições | aberto | que é agradável |

As fases 0 e 1 são a prova de fogo: se fecharem num fim de semana, o resto é trabalho conhecido.
A fase 3 é o maior risco de descoberta — o comportamento de `XamLoaderLaunchTitle` com jogo em
STFS, GOD e disco não está bem documentado, e é aqui que o fonte do FSD vira consulta obrigatória,
já que ele faz exatamente isso.

Sobre testar: o **Xenia Canary** roda parte do homebrew, de forma imperfeita. Serve para encurtar
um ciclo de UI, não para validar — o veredito é no console.

## Riscos, em ordem de quanto doem

1. **Sem XDK, o caminho A não existe.** É o bloqueio de entrada; nada mais importa antes disso.
2. **ImGui tem teto estético.** Leva a 80% funcional muito rápido e é a escolha certa para as
   fases 1–4, mas "Big Picture" é tile grande, animação e foco — isso pede render próprio em D3D9
   ou o XUI nativo (feito para UI de TV, e dependente do XuiTool). Pode virar refactor na fase 5;
   melhor saber antes.
3. **OpenXeChain imaturo:** hoje não é opção; em um ano talvez seja a casa do projeto.

O risco que existia sobre a qualidade das capas **foi eliminado por medição**: são 900×600 em
DXT5, em 120 dos 120 jogos, com fundo em 1080p em metade deles.

## Recomendação

A medição de 23/09/2026 fechou as incertezas de dados. **Sobra um único bloqueio: o XDK.** Sem ele
não se compila `.xex`, e a fase 0 não começa.

Enquanto o XDK não aparece, o que anda — e anda bem — é **prototipar a interface no PC com os
dados reais**: os 120 jogos do `content.db` e as capas de 900×600 extraídas dos `.assets`. Isso
entrega três coisas de uma vez: resolve a única decisão de produto ainda aberta (como a coleção se
parece), produz um parser FSDA que se traduz direto para C++ depois, e valida o desenho com o
conteúdo verdadeiro em vez de capas de mentira.

Não é trabalho jogado fora: é a fase 1 feita onde ela é barata.
