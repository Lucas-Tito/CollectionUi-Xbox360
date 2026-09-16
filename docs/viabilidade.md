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

**O FreeStyle já tem tudo, num SQLite, no console.** O
[fonte do FSD é aberto](https://github.com/XboxUnity/freestyledash) e o
`Freestyle/Tools/SQLite/FSDSql.cpp` diz onde e como:

```
Game:\Data\Databases\fsd2data.db        <- conteúdo e assets
Game:\Data\Databases\fsd2settings.db    <- anexado como "Settings"
```

**Atenção ao `Game:`** — no Xbox 360 ele é relativo ao **título em execução**, não um lugar fixo.
Quando o FreeStyle roda, `Game:` é a pasta de instalação dele; quando o **nosso** `.xex` rodar,
`Game:` vai ser a pasta *do nosso app*, e esse caminho não acha nada. Então o app precisa do
**caminho absoluto** da instalação do FSD — algo como `Hdd1:\Freestyle\Data\Databases\fsd2data.db`,
com o nome do dispositivo e da pasta a confirmar no console. Como isso não é descobrível por
adivinhação, o app deve **procurar** o banco nos dispositivos montados (`Hdd*:`, `Usb*:`) em vez de
assumir um caminho, e guardar o que achou.

### Como pegar o arquivo para inspecionar

O FreeStyle já tem **servidor FTP embutido** (`Freestyle/Tools/FTP/FTPServer.cpp` no fonte, mais o
plugin `FtpDll` que acompanha o FSD 3). Basta ligar e puxar o `fsd2data.db` pela rede — não é
preciso instalar homebrew nenhum para isso.

Para registro, o **Freestyle WebUI** (painel web servido pelo *Freestyle Plugin*) **não** serve:
ele mostra o jogo em execução e mexe em configurações, e não há componente de filesystem no fonte
do dash. Não é um navegador de arquivos.

Tabelas que interessam:

| tabela | o que tem |
|---|---|
| `ContentItems` | um registro por item instalado: `ContentItemTitleId`, `...Path`, `...Directory`, `...FileName`, `...Name`, `...Description`, `...Developer`, `...Publisher`, `...Genre`, `...Rating`, `...Raters`, `...ReleaseDate`, `...MediaId`, `...DiscNum`, `...DiscsInSet` |
| `Assets` | `AssetFileData` **BLOB** com a imagem, ligada por `AssetContentId` ao item e por `AssetAssetTypeId` ao tipo |
| `AssetTypes` | os tipos que o FSD popula: `0 Icon`, `1 BoxCover`, `2 Background`, `3 Banner`, `4 ScreentShot` (o typo é do FSD), `5 Video` |
| `Favorites` | favoritos por perfil de gamer |
| `RecentlyPlayedTitles` | item, data/hora e ordem — a fileira "jogados recentemente" sai de graça |
| `TitleUpdates` | TUs conhecidos |

Ou seja: **nome, title id, caminho do executável, gênero, desenvolvedora, capa, banner, fundo,
favoritos e recentes já estão prontos no console.** Não há capa para rebaixar nem catálogo para
pré-compilar: é abrir um SQLite e ler.

E dá para ler de dentro do `.xex`: a amalgamação do **sqlite3 está no próprio fonte do FSD**
(`Freestyle/Tools/SQLite/sqlite3.c`, 4 MB), o que prova que compila para o 360 com o XDK. Nosso
app linka o mesmo sqlite3 e abre o mesmo arquivo.

Três cuidados:

1. **Somente leitura.** Nunca escrever no banco do FSD; o estado do dashboard dele não é nosso.
2. **A resolução das capas é a que o FSD baixou.** O FSD deixa escolher a resolução ao baixar do
   XboxUnity, então isso é coisa de **medir no seu console**, não de supor. Se estiverem pequenas,
   a saída é remandar o FSD baixar maiores — não reconstruir um pipeline no PC.
3. **Os blobs são PNG/JPG.** O `D3DX` cria textura a partir de memória, mas decodificar a cada
   frame é caro: na primeira execução o app converte para **DDS (DXT1/DXT5)** num cache próprio e
   depois só carrega isso. A conversão não desaparece — ela **sai do PC e vira cache em runtime**,
   o que é melhor, porque acompanha automaticamente o que você instala e desinstala.

### Quanto confiar nesse schema

**Menos do que parece, e a diferença importa.** Tudo acima foi lido do fonte do **Freestyle Dash
2.0 RC2.1**, e o repositório aberto é *um único despejo de código de 12/07/2011* — a branch `v2`
tem exatamente dois commits ("(Added) Freestyle Dash 2.0 RC2.1 Source Code" e "(Removed) .user
files") e a `master` é isso mais quatro commits que só tocam `LICENSE.md` e o nome do README. O
**FSD 3** (o 3.0.775 que roda no console) e o **Aurora** nunca foram abertos.

Ou seja: entre o que foi lido e o que está instalado há a linha 3 inteira, fechada. Até o nome
`fsd2data.db` é o nome que o código **do 2** usa — não dá para garantir que o arquivo do FSD 3 se
chame assim.

Isso **não derruba o plano**, por um motivo específico: se o FSD 3 continuou em SQLite, **o
arquivo carrega o próprio schema** (`sqlite_master` descreve todas as tabelas), e não é preciso o
fonte do FSD 3 — é preciso o *arquivo* dele. Por isso a medição deixou de ser "conferir a
resolução das capas" e passou a ser "descobrir se o arquivo existe, como se chama e qual é o
schema real".

### Está criptografado?

**Não, e a evidência é indireta mas convergente:**

- O fonte do FSD 2 abre o banco com `sqlite3_open` puro, e o que está na árvore é a amalgamação
  **padrão** do SQLite — não há SQLCipher nem camada de cifra em lugar nenhum.
- As capas são BLOB de PNG/JPG dentro dessa mesma base, sem envelope.
- Do lado do Aurora, o [AuroraDbManager](https://github.com/XboxUnity/AuroraDbManager) lê a base
  como SQLite comum a partir de um app C# no PC, e o
  [AuroraAssetEditor](https://github.com/XboxUnity/AuroraAssetEditor) lê e escreve os `.asset`.
  Nenhum dos dois trata chave ou senha.
- O sistema de arquivos do console (FATX) também não é cifrado, e o que sai por FTP são os bytes
  do arquivo.

O que **é** assinado/cifrado no Xbox 360 é outra camada: os pacotes de jogo (STFS/GOD têm tabelas
de hash e blocos cifrados) e os executáveis XEX. Mas nada disso está no nosso caminho — o que
queremos é o índice e o cache de capas da dashboard.

Verificação definitiva em um segundo, assim que o arquivo chegar: todo banco SQLite começa com a
string mágica `SQLite format 3\0`.

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
entradas (1.579 de Xbox 360 e 3.313 de XBLIG). Duas notas:

- O vault guarda o id em **hex, como texto** (`"584109A8"`); o FSD guarda em **INTEGER**. Converter.
- 1.579 dos 2.155 jogos de 360 do vault têm `titleId`, ou **73%**. O resto precisa de fallback por
  nome normalizado — e o `norm()` do `tools/taglib.py` já faz essa normalização.

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
| 2 | Ler o `fsd2data.db`: lista real, capas dos blobs, cache DDS | 3–5 dias | que os dados do console chegam na tela |
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
3. **A qualidade das capas é a que o FSD baixou** (item 2 de
   [De onde vêm os dados](#de-onde-vêm-os-dados)). Medir cedo, no console.
4. **OpenXeChain imaturo:** hoje não é opção; em um ano talvez seja a casa do projeto.

## Recomendação

Nada de pipeline de dados no PC: os dados estão no console, e o vault entra depois, como
enriquecimento opcional.

O primeiro passo real é a **fase 0**, que depende do XDK. Enquanto ele não estiver em mãos, o que
dá para adiantar sem nada instalado é **medir o terreno**: puxar o `fsd2data.db` do console por
FTP e olhar de verdade quantos itens tem, que tipos de asset estão preenchidos e em que resolução
as capas estão. Isso responde a maior incerteza restante do projeto com meia hora de trabalho e
um cliente de FTP.
