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

8. **Fonte primária: o banco do próprio FreeStyle**, em `Game:\Data\Databases\fsd2data.db`, aberto
   **somente para leitura**. Nome, title id, caminho do executável, gênero, desenvolvedora,
   publicadora, capa, banner, fundo, favoritos e recém-jogados já estão lá.
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
  arquivo por FTP e verificar quantos itens existem, quais tipos de asset estão preenchidos e,
  principalmente, **em que resolução as capas estão** (o FSD deixa escolher isso ao baixar do
  XboxUnity, então é medição e não suposição). É a maior incerteza restante.
- **Comportamento de `XamLoaderLaunchTitle`** com jogo em STFS, GOD e disco — mal documentado; o
  fonte do FSD é a consulta, já que ele faz exatamente isso.
