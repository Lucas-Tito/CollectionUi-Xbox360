# Decisões

Registro do que ficou **batido** na rodada de 15/09/2026, a primeira do projeto, para não
relitigar depois. O levantamento técnico que sustenta cada item está em
[viabilidade.md](viabilidade.md); aqui ficam só as decisões, as descobertas em forma de fato e o
que continua em aberto.

## O que o produto é

1. **Um `.xex` nativo próprio**, escrito para o console, aberto como um app — do mesmo jeito que
   se abre um jogo.
2. **Não substitui o dashboard.** Nada de virar dash de boot via Dashlaunch. A pessoa abre quando
   quiser.
3. **Não é mod de dashboard.** Descartadas skin/script de Aurora (que não usamos) e de FreeStyle.
   Personalizar um dash é outro produto, não a v1 deste.
4. **O que a tela mostra é a biblioteca instalada**, no modelo do Steam Big Picture. Navegar o
   catálogo completo do console (os 6.900 títulos do xbox-vault) é uma **segunda fase opcional**,
   com orçamento próprio, e não entra na v1.
5. **Voltar para o app ao sair de um jogo não faz parte da experiência.** Ao sair, o console vai
   para onde o Dashlaunch mandar, e está tudo bem.

## Ambiente

6. **Console: RGH/JTAG rodando FreeStyle Dash.** Como o FreeStyle lança qualquer `.xex` pela lista
   de aplicativos, **não** é preciso publicar o app na seção Jogos do dashboard original — a
   técnica do GameShortcut/BLAST foi avaliada e descartada, serve só para quem usa o dash de
   fábrica.
7. **Desenvolvimento em Linux**, compilando via Docker
   ([xdk-docker](https://github.com/ClementDreptin/xdk-docker)). Sem VM Windows.

## Dados

8. **Fonte primária: o banco do próprio FreeStyle**, aberto **somente para leitura**. Nome, title
   id, caminho do executável, gênero, desenvolvedora, publicadora, capa, banner, fundo, favoritos
   e recém-jogados já estão lá. O arquivo é `Data\Databases\fsd2data.db` **dentro da pasta de
   instalação do FSD** — o fonte dele escreve em `Game:\Data\Databases\`, e `Game:` é relativo ao
   título em execução, então do nosso app aquele caminho aponta para a *nossa* pasta. O app
   **procura** o banco nos dispositivos montados em vez de assumir caminho.
9. **Nenhum pipeline de imagens no PC.** Não vamos rebaixar capas nem pré-compilar catálogo: a
   conversão para DDS acontece como **cache em runtime**, na primeira execução, o que acompanha
   sozinho o que se instala e desinstala.
10. **O [xbox-vault](https://github.com/Lucas-Tito/xbox-vault) entra como enriquecimento
    opcional**, não como fonte: Metacritic, HowLongToBeat, modos de jogo, co-op e nº de jogadores
    — o que o FSD não tem. Junção por **title id**. Opcional para a v1.

## Tecnologia

11. **Toolchain: XDK oficial + VS2010**, porque é o único que produz `.xex` rodando sob o kernel,
    com acesso ao `xam` — e é o `xam` que lança jogo.
12. **libxenon está descartado**, com motivo técnico e não por gosto: roda bare-metal via XeLL, sem
    kernel e sem `xam`, os dashboards não o reconhecem e **não há como lançar um jogo de varejo**.
    Serve para emulador e port, não para launcher.
13. **[OpenXeChain](https://github.com/OpenXeChain) fica no radar, não no caminho crítico.** É o
    substituto livre do XDK e é onde o projeto deveria querer estar em um ano, mas o SynthXEX se
    declara em estágio inicial ("MANY features are missing") e parou em outubro de 2025.
14. **UI com Dear ImGui** ([imgui-xbox360](https://github.com/ClementDreptin/imgui-xbox360)) nas
    fases iniciais, **com o teto estético assumido**: ImGui chega rápido ao funcional, mas tile
    grande, animação e foco de verdade pedem render próprio em D3D9 ou o XUI nativo. Decisão
    consciente de dívida, a revisitar no acabamento.

## Ferramentas de acesso remoto avaliadas

Levantadas no `data/homebrew.json` do próprio xbox-vault:

- **FTP embutido no FreeStyle** (`Tools/FTP/FTPServer.cpp`; plugin `FtpDll`, que acompanha o FSD 3
  e o Aurora) — **é o caminho**. Não exige instalar nada novo.
- **Freestyle WebUI** — painel web do *Freestyle Plugin*: mostra o jogo em execução e mexe em
  configurações. **Não navega arquivos**, e não há componente de filesystem no fonte do dash.
  Avaliado e descartado para este fim.
- **stfs-webjs** ([InvoxiPlayGames](https://github.com/InvoxiPlayGames/stfs-webjs)) — lê
  contêineres STFS no navegador. Não serve para acesso remoto, mas guarda-se para quando for
  preciso entender os pacotes dos títulos instalados.
- **fatx** ([mborgerson](https://github.com/mborgerson/fatx)) — biblioteca, driver FUSE e
  explorador de FATX. Relevante por ser Linux: permite montar um dump do HD aqui na máquina.
- **ConnectX** e **SmbDll** — montam compartilhamento SMB da rede no console. Caminho inverso
  (levar arquivo para o console), não o nosso.

## Descobertas desta rodada

O que passou a ser fato conhecido, com a fonte:

- **O FreeStyle Dash é open source** ([XboxUnity/freestyledash](https://github.com/XboxUnity/freestyledash)),
  e o `Freestyle/Tools/SQLite/FSDSql.cpp` entrega o caminho do banco, o schema e os tipos de
  asset: `ContentItems` (um registro por item instalado), `Assets` (`AssetFileData` **BLOB** com a
  imagem), `AssetTypes` populada com `0 Icon`, `1 BoxCover`, `2 Background`, `3 Banner`,
  `4 ScreentShot` — o typo é do FSD —, `5 Video`, mais `Favorites`, `RecentlyPlayedTitles` (com
  data/hora e ordem) e `TitleUpdates`.
- **sqlite3 compila para o Xbox 360**: a amalgamação está no próprio fonte do FSD
  (`Freestyle/Tools/SQLite/sqlite3.c`, 4 MB). Nosso app linka o mesmo sqlite3.
- **O `data/x360db.json` do xbox-vault já tem `titleId`** em 100% das suas 4.892 entradas (1.579
  de Xbox 360 e 3.313 de XBLIG). É a chave de junção com o FSD. Duas pegadinhas: o vault guarda em
  **hex-string** (`"584109A8"`) e o FSD em **INTEGER**; e 1.579 dos 2.155 jogos de 360 do vault têm
  title id, ou **73%** — o resto precisa de fallback por nome normalizado, e o `norm()` do
  `tools/taglib.py` já faz isso.
- **Existe homebrew nativo ativo em 2026** para aprender de: o
  [X-Store](https://github.com/951261/X-Store) (app nativo com HTTPS e `docs/` de arquitetura, mas
  UI de texto, não gráfica) e todo o ecossistema de
  [ClementDreptin](https://github.com/ClementDreptin) — `XexUtils` (HTTP, JSON, filesystem, input,
  Xam, Dashlaunch, STFS), `imgui-xbox360`, `XboxPlayground`, `ModdingResources`.
- **O lançamento de jogo é `XamLoaderLaunchTitle` / `XLaunchNewImage`**, exportadas pelo `xam`.
- **Limites do xdk-docker**: não há imagem pré-construída (só Dockerfiles), o build exige
  `--build-arg XDK=<caminho>` para um XDK já instalado, e só há `make` — sem MSBuild, logo os
  `.vcxproj` do imgui-xbox360 e do XexUtils precisarão de Makefiles. Testado com `2.0.21256.17`.
- **Limites do imgui-xbox360**: travado em Dear ImGui **v1.86** (a v1.87 derrubou compilador
  pré-C++11/VS2010) e `DisplaySize` fixo em 720p, porque as outras resoluções saem do scaler de
  hardware do console.

## Premissas que caíram no caminho

Ficam registradas porque custaram uma rodada e não devem voltar:

- *"O app é o dashboard de boot"* — **errado**. O produto é um app que se abre. Isso também
  esvaziou a importância da distinção RGH × BadUpdate para o design.
- *"Precisa aparecer na seção Jogos do dash"* — **desnecessário**: o FreeStyle já lança `.xex`.
- *"Começar por uma skin de Aurora"* — **fora**: não usamos Aurora, e mexer em dash é outro
  produto.
- *"Rebaixar milhares de capas no PC e converter em lote"* — **desnecessário**: as capas já estão
  no console, dentro do banco do FSD.

## Em aberto

- **Conseguir o XDK.** É o bloqueio de entrada: sem ele nada compila, e toda a fase 0 depende
  disso.
- **Medir o `fsd2data.db` de verdade** — próximo passo, não precisa de XDK nem de código: puxar o
  arquivo pelo **FTP embutido do FreeStyle** (`Tools/FTP/FTPServer.cpp`, mais o plugin `FtpDll`) e
  verificar quantos itens existem, quais tipos de asset estão preenchidos e, principalmente, **em
  que resolução as capas estão** (o FSD deixa escolher isso ao baixar do XboxUnity, então é
  medição e não suposição). É a maior incerteza restante.
- **Onde exatamente o FSD está instalado** — define o caminho absoluto do banco, e sai da primeira
  listagem do FTP.
- **Comportamento de `XamLoaderLaunchTitle`** com jogo em STFS, GOD e disco — mal documentado; o
  fonte do FSD é a consulta, já que ele faz exatamente isso.

---

# Rodada de 16/09/2026 — fork, dashboards abertas e confiança no schema

## Batido

15. **Não forkar o FreeStyle.** Usar como **implementação de referência**, não como base. Os
    motivos são estruturais e não mudariam com um fonte mais novo: a UI é XUR (trocaria o trabalho
    de ImGui por XuiTool, não o eliminaria), são 495 arquivos e 2,8 MB de código próprio, e
    arquitetonicamente é um dashboard — boa parte do esforço seria apagar cena que não queremos.
16. **Continuar no FreeStyle como fonte de dados**, apesar de o Aurora estar mais bem documentado.
    Preferência do dono do projeto, e a medição dirá o tamanho real da diferença.
17. **Atenção à GPLv3 do FSD:** ler para descobrir qual API chamar é uma coisa; copiar
    implementação torna o projeto derivado e obrigatoriamente GPLv3. Decisão a tomar
    conscientemente quando (e se) acontecer.

## Descobertas

- **O fonte aberto do FreeStyle é um despejo único de 2011.** A branch `v2` tem dois commits, os
  dois de 12/07/2011; a `master` é isso mais quatro commits que só tocam `LICENSE.md` e o README.
  FSD 3 e Aurora nunca foram abertos — o que a Team FSD/Phoenix abriu foi o *ferramental* do
  Aurora, não os dashboards.
- **Só existe uma dashboard aberta de Xbox 360 capaz de lançar jogo de varejo**, e é essa, de
  2011. As outras abertas (Xemini, Xenu, XMENU) são libxenon e não lançam jogo. Aurora, FSD 3,
  XeXMenu, XexDash, Viper360, IngeniouX, XeXLoader e 360Menu são fechadas.
- **O lançamento de jogo está resolvido** — `ContentItemNew::LaunchGame()`: `XLaunchNewImage()`
  para XEX/XBE solto, e `Xbox360Container` para STFS/GOD. Sobre o `XamLoaderLaunchTitle`, o
  comentário no fonte deles é *"not really needed, just use xlaunchnewimage"*. Era o maior risco
  da fase 3.
- **Confiança no schema rebaixada:** o que foi lido é do FSD **2.0 RC2.1**; o console roda FSD
  **3.0.775**. Nem o nome `fsd2data.db` é garantido. Mitigação: SQLite é auto-descritivo
  (`sqlite_master`), então basta o **arquivo**, não o fonte.
- **Não há criptografia no caminho.** `sqlite3_open` puro sobre a amalgamação padrão do SQLite,
  capas como BLOB de PNG/JPG, FATX sem cifra, e as ferramentas de PC do Aurora leem tudo sem
  chave. O que é assinado/cifrado no 360 são os pacotes de jogo (STFS/GOD) e os XEX — outra
  camada, fora do nosso caminho.
- **O Aurora tem API HTTP documentada (Nova)**, com definição OpenAPI e endpoints de
  `Filebrowser`, `Title`, `Image`, `Profile`, `Achievement`, `Dashlaunch`, `Memory` e `System`
  ([documentação](https://github.com/jrobiche/xbox360-aurora-developer-documentation)). É a
  resposta completa para "leitura remota dos arquivos do console" — que no FreeStyle só existe via
  FTP.
- **As capas do Aurora são D3DTexture** dentro dos `.asset`, ou seja, já em formato de textura de
  GPU — eliminaria a etapa de decodificar e converter para DDS que o FSD exige.

- **Dá para modificar o FreeStyle instalado sem ter o fonte dele**, em três camadas: trocar a
  **skin** (`.xzp`/XUR, dados puros, muda só a aparência); **injetar uma DLL** no dash em execução e
  desenhar por cima; ou **patchar o `.xex`** (a cena já faz — o build "FSD 3 Fixed (Unofficial)" do
  3.0.775 existe sem fonte).
- **A injeção de DLL é um caminho real e documentado**: o `examples/dll/main.cpp` do imgui-xbox360
  detoura `XuiRenderEnd`, pega o `D3DDevice` do título em execução via `XuiRenderGetDevice`
  (importado de `xam.xex`, ordinal 2095) e renderiza ImGui sobre a UI existente. O `XexUtils` tem
  `Detour`, e em RGH/JTAG o carregamento é pelo Dashlaunch. **Mas o exemplo mira o dashboard oficial
  (`0xFFFE07D1`), não o FSD** — que o mesmo gancho pegue no FSD é hipótese plausível (ele também
  renderiza via XUI), não fato verificado.

## Em aberto (novo)

- **Recentes:** o `LaunchGame()` do FSD grava em `RecentlyPlayed` antes de lançar. Com o banco
  aberto só para leitura, jogo lançado pelo nosso app não entra na lista — nem na do FSD nem na
  nossa, que sai da mesma tabela. Ou se relaxa a regra nessa tabela, ou se mantém registro próprio.
- **Plugin × app próprio.** O caminho do plugin elimina varredura, capas, arqueologia de schema e o
  risco de lançamento, mas contraria a decisão 1 (deixa de ser algo que se abre como um jogo) e
  acopla o projeto a um binário fechado. **Não está decidido.** O teste que resolve é pequeno:
  descobrir o title id do FSD 3 instalado e verificar se o gancho de render do XUI pega nele.

---

# Rodada de 23/09/2026 — a medição

Os arquivos do FreeStyle foram abertos no PC, a partir do pendrive. Isto encerra a fase de
suposição sobre dados.

## Corrigido (estava errado nos docs)

- O banco **não** é `fsd2data.db`, é **`Data/Databases/content.db`** (mais `settings.db`).
- As capas **não** são BLOB de PNG/JPG no banco. O FSD 3 removeu a tabela `Assets` e pôs a arte em
  `Data/GameData/<id>/`, num container `FSDA`.
- **Não existe etapa de conversão para DDS**, nem no PC nem em runtime: a arte já está em **DXT5**.

## Fatos, medidos

- **`Freestyle.780/` é a instalação ativa** — último jogo lançado em 07/09/2026 14:15, contra
  09/09/2020 da outra pasta. Timestamps do FAT não servem de prova aqui (o 360 grava `2005-11-22`
  com relógio zerado); a prova saiu de `RecentlyPlayedTitles`.
- **O banco é SQLite puro e não criptografado** (`SQLite format 3`).
- **O fonte do FSD 2 era um mapa válido:** mesmas tabelas, mesmos nomes de coluna. O FSD 3 só
  acrescentou `ContentItemHash` e `ContentItemKinectFlag`.
- **A biblioteca tem 120 jogos**, com nome, descrição, desenvolvedora, publicadora, gênero, nota,
  nº de avaliadores, data, title id e caminho. Mais 218 recém-jogados (timestamp unix) e 71 TUs.
- **`ContentItemFileType` distingue `1` = XEX solto de `3` = container**, que são exatamente os
  dois ramos do `LaunchGame()`.
- **A arte é farta:** 991 MB para 120 jogos. **Capa grande em 120/120**, tipicamente **900×600**;
  fundo em **1920×1080** em 51; além de ícone, banner e capa pequena. Tudo DXT5.
- **A junção com o xbox-vault rende 78%** (93 de 120) por title id. Os 27 restantes são
  emuladores (não são jogos), jogos de Xbox original (estão no `xbox.json`, não no `x360db.json`)
  e jogos de 360 sem title id no x360db (caem no fallback por nome).
- **O formato FSDA está documentado** em [viabilidade.md](viabilidade.md): header big-endian,
  tabela de entradas de 16 bytes e os tipos 1/2/4/8/64/128.

## Em aberto (atualizado)

- **O XDK é o único bloqueio real que resta.** Todo o resto está medido ou resolvido.
- Um detalhe do parser FSDA: há DDS contíguos ao fim do arquivo fora da tabela de entradas
  (screenshots), e a máscara do header acende um bit a mais do que o número de entradas. Não é
  bloqueante.
