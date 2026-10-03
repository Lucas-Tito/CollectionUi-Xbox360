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

---

# Rodada de 23/09/2026 (parte 2) — a interface

Um protótipo navegável em HTML, alimentado com os 120 jogos e as capas reais extraídas do console,
serviu para fechar o desenho antes de escrever C++. O detalhe está em
[interface.md](interface.md); aqui fica o que foi batido.

18. **O desenho é seco.** A primeira versão, um Big Picture cinematográfico com arte de fundo,
    desfoque, tipografia gigante e painel de metadados, foi recusada — *"exagerada demais pro
    360"*. Vale tela chapada, borda fina e verde só no foco.
19. **Duas telas:** coleções (quadrados) e jogos (grade). Não há terceira.
20. **A tela de coleções começa vazia**, com uma linha só, e **sem quadrado de "+"** — com muitas
    coleções ele se perderia no fim da lista. Criar é tecla.
21. **O rodapé é o único lugar que documenta controle**, com símbolo e ação, e só mostra a dica
    quando ela faz alguma coisa.
22. **Coleções em ordem alfabética**, sem reordenar à mão e sem reordenar sozinho.
23. **Grade de 5 por linha**, alfabética ignorando o artigo inicial, nome cortado em 24
    caracteres.
24. **Salto por letra pelos dois caminhos** (segurar `↓` e `LB`/`RB`), com **índice alfabético
    fixo na borda direita** que acende a letra atual e apaga as ausentes — para o salto ser
    visível antes de ser apertado.
25. **`A` confirma, `B` volta**, em todas as telas. `X` cria coleção, `☰` renomeia ou apaga, e
    **`X` não faz nada dentro de uma coleção**.
26. **O protótipo HTML é jogável no PC**, por teclado e mouse, com as teclas impressas no próprio
    rodapé ao lado do botão do controle.

## Consequência a encarar

O enxugamento tirou **Metacritic, tempo de jogo e ficha técnica** do desenho. É perda real — 96
dos 120 jogos têm nota e 91 têm duração — e está registrada em
[interface.md](interface.md#o-que-ficou-de-fora) junto com as outras duas pendências: o corte do
nome e a tela de escolher jogo a jogo ao montar uma coleção.

27. **Criar coleção pede só o nome, e ela nasce vazia.** Não vale escolher jogos na hora de criar.
    Consequência assumida: passou a existir a **tela de escolher jogos**, aberta pelo `☰` de
    dentro da coleção, com a biblioteca inteira na mesma grade e `A` marcando. Detalhe em
    [interface.md](interface.md#tela-de-escolher-jogos).

---

# Rodada de 25/09/2026 — o toolchain, provado no hardware

28. **O XDK entrou e a fase 0 fechou.** `cl.exe`, `link.exe` e `imagexex.exe` rodam sob wine em
    container no Linux, sem VM Windows e sem Visual Studio. As três armadilhas do caminho estão em
    [toolchain/README.md](../toolchain/README.md), cada uma com o motivo — a poda de DLLs do
    upstream que ficou velha, os headers de C/C++ que não moram em `include/xbox`, e o
    `WINEPREFIX` fixo que parece otimização e quebra o build.
29. **O primeiro `.xex` rodou no console**, a partir do pendrive: `Direct3DCreate9` ok,
    `CreateDevice` ok em 1280x720, laço de apresentação até o quadro 1200, cores avançando.
30. **O canal de log é arquivo, não XBDM.** O app escreve em `game:\<nome>.log`, que é a pasta de
    onde o executável rodou, e se lê de volta pelo pendrive ou por FTP. Confirmado funcionando do
    console para o USB. O XBDM exigiria plugin no console e um leitor do canal de notificação que
    não existe pronto — fica para depuração ao vivo, se um dia precisar.
31. **Compilar em Release para rodar no console.** O `d3d9d.lib` do Debug espera ambiente de
    devkit com XBDM e pode falhar de um jeito que confunde o diagnóstico. O Debug continua útil
    para pegar erro de API, mas não é o que se leva para o hardware.

## Confirmado de novo

O relógio do console está zerado: o log saiu datado de **2005-11-22**, a mesma data falsa que
aparecia nas pastas do FreeStyle e que serviu para descobrir qual instalação estava ativa. Se um
dia o app for gravar data de "jogado pela última vez", não dá para confiar no relógio da máquina.

32. **`X` acrescenta, `☰` abre opções, `A` confirma, `B` volta ou cancela.** A regra vale nas três
    telas: `X` cria coleção na primeira e adiciona jogos na segunda, porque as duas coisas são
    "acrescentar aqui". O `☰` deixou de ser um rótulo diferente em cada tela e passou a significar
    sempre "opções do que está em foco" — inclusive **remover um jogo da coleção**, que antes não
    existia.
33. **Ao adicionar jogos, `☰` conclui e `B` cancela** — e cancelar desfaz de verdade: as marcações
    vão para uma cópia e só o Concluir grava. Marcar no original faria de "cancelar" palavra sem
    efeito. O nome da tela também mudou: era "Escolher jogos", virou **"Adicionar jogos"**.

34. **`A` em cima de um jogo lança, e o CollectionUI morre ali.** Voltar para o app depois
    do jogo não faz parte da experiência — decisão do usuário, repetida. Isso simplifica:
    não há estado a preservar, e o único caminho de volta é o de erro.

35. **Duas APIs do XDK, nenhuma linha do FreeStyle.** `XLaunchNewImage` (`xbox.h:417`) para
    XEX e XBE soltos; `XContentLaunchImageFromFile` (`xbox.h:1293`) para container. A segunda
    monta o pacote e lança de dentro dele numa chamada só, e **devolve `DWORD`** — ao
    contrário da primeira, que é `DECLSPEC_NORETURN VOID`. Ela é de maio/2011; o fonte aberto
    do FSD 2 é de julho/2011 e usa o idioma antigo (`XamContentOpenFile` + montar + lançar).
    Consequência de licença: o lançamento inteiro não deriva do código GPLv3 do FreeStyle.

36. **`XLaunchNewImage` é `NORETURN` de verdade.** O `cl.exe` recusou com `C4702 unreachable
    code` um `return 0;` escrito depois dela. Em Release o otimizador apaga esse código, então
    **todo diagnóstico tem de vir antes da chamada** — inclusive o teste de existência do
    arquivo, porque a função não tem como avisar que o caminho não existe.

37. **O dispositivo do jogo se descobre sondando, não pelo banco.** O `content.db` guarda o
    caminho sem o volume (`\JOGOS\X\default.xex`): o prefixo é reconstruído a cada boot, e o
    apelido do FreeStyle (`Hdd1:`) não é o nosso (`Hdd:`). Testamos os apelidos montados com
    `GetFileAttributes` até achar. Custa no máximo 4 chamadas e é de graça, porque o teste de
    existência teria de ser feito de qualquer jeito (ver 36). A alternativa — join com
    `ScanPaths` no `settings.db` — fica para quando existir biblioteca em dois dispositivos.
    **A heurística "o jogo está no dispositivo do `content.db`" é falsa aqui:** o banco veio
    do pendrive e os jogos estão no HD.

38. **Existe `tipoArquivo == 2`.** São 8 jogos de Xbox original (`.xbe`) neste acervo, e
    lançam pela mesma chamada do tipo 1. O comentário do `biblioteca.h` dizia "1 = XEX solto,
    3 = container" e os omitia. Contagem medida: 1=57, 2=8, 3=55.

39. **Sem lista branca de `contentType`.** O `LaunchGame()` do FSD 2 recusa tudo que não seja
    ARCADE, XBOXTITLE, XBOX360TITLE, INSTALLED ou GAMEDEMO — e isso rejeitaria os 6 itens
    `contentType == 0x2` (indies/XBLIG) desta biblioteca, que o FreeStyle 3 desta casa lança
    normalmente. O `contentType` serve só para escolher `default.xex` ou `default.xbe` dentro
    de um container.

40. **Multi-disco e consolidação de TU ficam de fora.** `PrepareForMultiDiscLaunch` não chama
    API nenhuma do XDK: registra estado no plugin do FreeStyle, que não temos. Custa a troca de
    disco em 1 jogo dos 120 (L.A. Noire); os outros multi-disco já estão no banco como entradas
    separadas e abrem direto pela grade. `ConsolidateTitleUpdates` copia e **apaga** arquivos de
    title update, o que contraria a regra de só-leitura bem mais do que gravar em "recentes".

41. **Lançar exige desmontar o app antes.** A doc do XDK proíbe lançar com I/O de disco
    pendente e proíbe chamar da thread dona do device D3D. Então: `carregador::Parar()`, soltar
    as texturas do cache com `BlockUntilIdle`, fechar o log, e disparar de uma thread nova. O
    log reabre em **append** (`diario::Reabrir`) no caminho de erro — `Abrir` trunca e apagaria
    justamente o registro que explica a falha.

42. **A coleção guarda TitleId, não ContentItemId**, em hexadecimal no `colecoes.txt`.
    Decisão do usuário depois de pesar as duas perdas. O `ContentItemId` é a chave primária
    do banco do FreeStyle e quem a mantém é o CAMINHO (`UNIQUE (ContentItemPath)`): sobrevive
    a rescan e a reboot, mas morre ao reinstalar o jogo, ao **mover ou renomear a pasta**, ou
    se o banco for refeito. O `TitleId` vem do cabeçalho do XEX e não depende de nada disso.

    **O que se perde, aceito conscientemente:** itens que compartilham TitleId se fundem.
    Medido neste acervo: 116 TitleIds distintos em 120 itens, nenhum zero, 4 pares repetidos
    — Forza 4 e Splinter Cell Blacklist (discos 1 e 2), CoD World at War e Kill Team
    (instalados em duplicata). Marcar um disco traz o outro, e remover tira os dois. Em
    acervo grande (a expectativa são 600), dois jogos DIFERENTES com o mesmo TitleId se
    fundiriam de forma errada — não há caso assim nestes 120, mas a amostra é de 120.

    Um híbrido (`contentItemId:titleId` com fallback) foi proposto e **recusado**: não volte
    a sugerir.

43. **`strtoul` base 16, e validação `!= 0`.** O TitleId usa os 32 bits: o do Snes360 é
    `0xFFED0707`. Com `vector<int>` e `atoi`, ele voltaria negativo e a validação antiga
    (`id > 0`) o descartaria **em silêncio** a cada releitura do arquivo. Daí `unsigned int`
    em `Colecao::ids` e em toda a cadeia (`Tem`, `Alternar`, `Remover`, `g_selecao`).

44. **A capa continua vindo do `ContentItemId`.** A pasta de arte é `GameData\<id em hex>`,
    então o cache e o carregador não mudaram. TitleId é só o que vai para o disco. A contagem
    na tela de coleções conta os itens que a coleção realmente mostra, não quantos TitleIds
    ela guarda — senão um multi-disco diria "1 jogo" sobre uma grade com duas capas.

45. **O `colecoes.txt` se lê com linha de tamanho livre.** Com `char linha[4096]` uma coleção
    grande se destruía sozinha: 600 TitleIds em hexa dão ~5.4 KB, o `fgets` cortava no meio do
    455º token, o id partido virava número de lixo e os 145 restantes voltavam como linha sem
    `|`, descartada em silêncio. E como todo `Gravar` reescreve o arquivo a partir do que foi
    carregado, a perda de leitura virava perda definitiva. Os 600 são o acervo que a decisão 42
    projeta — o teto ficava **abaixo do alvo declarado**. Verificado no host: 600 ids voltam
    íntegros e o arquivo é estável na segunda gravação.

46. **O que separa coleção de comentário é a BARRA, não o `#`.** Tratar `#` inicial como
    comentário fazia uma coleção chamada "#1 favoritos" **sumir inteira** na releitura, e o
    teclado do sistema (`VKBD_LATIN_FULL`) deixa digitar `#`. Por isso o cabeçalho que `Gravar`
    escreve não contém barra nenhuma.

47. **TitleId zero é barrado na ESCRITA, não só na leitura.** Um item cujo cabeçalho o FreeStyle
    não leu teria TitleId 0: o anel acenderia, o arquivo gravaria `00000000` e a releitura
    descartaria — o jogo sumiria da coleção no boot seguinte, sem aviso. É a mesma perda
    silenciosa da decisão 43, deslocada do `atoi` para o `!= 0`. Não há caso assim nestes 120.

48. **Contagem e rótulos contam ITENS, não TitleIds.** Vale nas três telas: na de coleções
    (decisão 44), no cabeçalho e no rodapé da tela de adicionar, e no item do menu, que passa a
    dizer "Remover da coleção (2 itens)" quando o TitleId é compartilhado. Marcar um disco do
    Forza acende dois anéis; dizer "1 marcado" corroía confiança à toa.

49. **Capa não marcada é a mesma capa com um véu por cima.** `DrawScreenSpaceTexturedRectColored`
    fixa UV 0..1 dentro da própria função (`AtgDebugDraw.cpp:662`), então o ramo do jogo não
    marcado desenhava o **encarte inteiro** — contracapa e lombada espremidas no 5:7 — justo no
    estado inicial de todos eles. Agora desenha o mesmo recorte e escurece por cima.

50. **`carregador::Parar()` espera `INFINITE`.** O único chamador é o lançamento de jogo, e a doc
    do XDK proíbe lançar com I/O de disco pendente. Desistir em 1 s fechava o `HANDLE` com a
    thread viva; e se o lançamento falhasse, `Iniciar()` punha `g_parar` em falso e a thread
    velha voltava a consumir a fila ao lado da nova.

51. **O teclado do sistema é ASSÍNCRONO, e bloquear nele TRAVA O CONSOLE.** Sintoma
    observado: o app abre normal, a Guide responde, mas no primeiro `X` o console congela
    inteiro e não volta. Causa: a Guide desenha **por cima do quadro do título**, então um
    título parado num `WaitForSingleObject(INFINITE)` deixa o sistema sem nada para compor —
    e nem o botão Guide responde mais. O idioma certo está na amostra oficial do XDK
    (`Source/Samples/Online/StringVerify/StringVerify.cpp:161`): dispara, **volta para o
    laço**, e a cada quadro consulta `XHasOverlappedIoCompleted` continuando a desenhar.

52. **Os buffers do `XShowKeyboardUI` não podem ficar na pilha.** A doc é literal: *"the
    buffers ... must be guaranteed to remain valid until the operation is finished. For this
    reason, the buffer should not be declared on the stack."* Título, descrição, texto inicial
    e resultado agora vivem no módulo `teclado.cpp`.

53. **`XUSER_INDEX_ANY` (0xFF), não 0.** Este console não tem tela de login e pode estar sem
    perfil conectado. A amostra do XDK usa o índice de um usuário assinado porque ela tem
    tela de sign-in; nós não temos.

54. **O `g_bloqueou` foi embora.** Ele existia só para ressincronizar o controle depois de o
    teclado ter segurado o laço. Sem bloqueio não há dessincronia: o laço continua lendo o
    controle a cada quadro e apenas **ignora** os botões enquanto `teclado::Aberto()`, de
    modo que o `A` que confirmou dentro da Guide nunca reaparece como botão novo.

55. **Nunca use `DrawScreenSpaceTexturedRectColored` com textura `NULL`.** Ela faz
    `SetSampler(..., NULL)` e desenha com o shader **texturizado**, amostrando um sampler sem
    nada ligado — é o glitch que aparecia na letra acesa do índice alfabético. Para retângulo
    cheio de cor sólida, `DrawScreenSpaceRect(r, 0.0f, cor)`: largura zero desenha cheio e usa
    o shader de cor constante, sem sampler.

56. **A ATG não liga `D3DRS_ALPHABLENDENABLE` no `DebugDraw`** — só o `AtgFont` mexe nesse
    estado, e só em volta do `Begin/End` dele. Sem ligar à mão, o alfa da cor é **ignorado** e
    o retângulo sai opaco. Foi por isso que o véu do jogo não marcado, `ARGB(140, 6, 9, 8)`,
    saiu preto sólido e escondeu a capa inteira na tela de adicionar. O `Preencher()` do
    `main.cpp` liga a mistura, desenha e devolve o estado ao desligado.

57. **O lançamento funciona no console.** Adventure Time: Explore the Dungeon abriu —
    `tipoArquivo == 1` (XEX solto), pelo `XLaunchNewImage`. O ramo do container
    (`XContentLaunchImageFromFile`, 55 jogos deste acervo) **continua sem prova**: é o único
    risco de descoberta que resta, e falha devolvendo código de erro, não travando.

58. **Som de interface: os `.xma` do skin padrão da FreeStyle, tocados direto pelo XAudio2.**
    São RIFF/WAVE com codec XMA2 (`0x166`), que o XAudio2 do 360 consome **nativamente** — quem
    decodifica é o hardware do Xenon. Não convertemos para PCM: converter jogaria fora o
    `XMA2WAVEFORMATEX` do próprio arquivo, que é exatamente o que o `CreateSourceVoice` quer
    receber, e ainda acrescentaria uma dependência de ffmpeg ao build sem simplificar nada.
    Cinco efeitos: `btn_Focus` (mover foco), `btn_Select` (A), `btn_Back` (B), `flyout` (menu),
    `NotifyPopup` (erro).

59. **Não seguimos a arquitetura da FreeStyle, e não poderíamos.** Ela não toca som pelo C++:
    é uma aplicação XUI e só chama `XuiSoundXAudioRegister()` em `FreestyleUIApp.cpp:230`,
    deixando as cenas `.xur` do skin dispararem os sons. Ir por ali exigiria trazer o XUI
    inteiro — cenas, `.xur`, `XuiTool` —, outro framework de interface. O **motor**, porém, é o
    mesmo: o nome da função diz que o backend de som do XUI é o XAudio2.

60. **XMA exige `XPhysicalAlloc` alinhado em 2 KB.** *"XMA packets must be 2K aligned"*, da
    amostra `XAudio2BasicSound` do XDK. Um `new BYTE[]` daria um ponteiro qualquer, e quem lê
    esses bytes é o decodificador de hardware, não a CPU. Os 38 arquivos do skin são, sem
    exceção, `N × 2048 + 92` bytes — 2048 é o `XMA_BYTES_PER_PACKET` do `xma2defs.h`.

61. **O cabeçalho RIFF vem little-endian e o Xenon é big-endian.** O XDK resolve com
    `LocalizeXma2Format()` (`xma2defs.h:679`), que detecta pela `wFormatTag` e troca no lugar.
    Os tamanhos de chunk são lidos byte a byte: além da ordem, um chunk pode começar em
    endereço não alinhado, e no PowerPC isso não é só lento — pode falhar.

62. **A imagem do Docker copia as libs do XDK uma a uma.** Som exigiu acrescentar `xaudio2.lib`
    (repare que o sufixo de depuração fica no MEIO: `xaudiod2.lib`) e `xmcore.lib`, porque o
    XAudio2 usa a fila sem trava do xmcore (`XLFQueueCreate`/`XLFQueueAdd`) e sem ela o link
    para com quatro símbolos não resolvidos. Biblioteca nova do XDK = editar o Dockerfile e
    reconstruir a imagem.

63. **Som não pode derrubar o app.** Falha no `XAudio2Create`, no arquivo ou num efeito
    específico é registrada no log e o app segue — mudo, ou mudo só naquele efeito. É enfeite
    num launcher; a régua é diferente da do lançamento de jogo.

64. **`som::Parar()` entra na desmontagem antes de lançar** (decisão 41), junto do carregador e
    das texturas: uma voz tocando é o motor de áudio vivo, e a doc do XDK proíbe lançar com I/O
    pendente.

65. **`Tocar()` não reinicia som que ainda está tocando — desiste.** A sequência intuitiva
    (`Stop` → `FlushSourceBuffers` → `Submit` → `Start`) não recomeça o som: *"Stop is always
    asynchronous"*, e o `Flush` não tira da fila o buffer em reprodução enquanto a voz não
    parou de verdade. O `Submit` entraria **atrás** dele, e o som sairia cada vez mais atrasado
    em relação ao dedo — justo o que eu achava estar evitando. A amostra `XAudio2VoiceReuse` do
    XDK resolve esperando o flush drenar com `Sleep(1)`, o que numa thread de desenho é pior
    que o sintoma. Consultamos `GetState` e saímos se `BuffersQueued > 0`.

    Isso tornou o `INTERVALO_MIN` de 45 ms desnecessário — e o comentário dele estava **errado**:
    dizia proteger contra o analógico segurado, mas `ESPERA_REPETE` é 110 ms, maior que 45, e
    ele nunca disparava nesse caso. O que ele de fato cobria era a diagonal, que chama `Tocar`
    duas vezes no mesmo quadro — e o `GetState` cobre isso também.

66. **Som e carregador voltam juntos depois de um lançamento que falha.** O `SoltarTudo()`
    destrói as vozes antes de lançar (decisão 64). No caminho de erro, o `Jogar()` remontava só
    o carregador: o aviso saía mudo e o app ficava **silencioso para sempre**, até reiniciar.
    `som::Iniciar()` também ganhou guarda de idempotência — sem ela, uma segunda chamada
    zeraria os ponteiros das vozes e da memória física sem soltar nada.

67. **Som só toca quando algo aconteceu.** `Confirmar()` tocava na primeira linha, então A com
    a lista vazia dava clique de confirmação sem confirmar nada. O som foi para dentro de cada
    ramo, depois do teste de lista vazia. Mesmo motivo no `SaltoLetra`: com o foco já na ponta,
    LB/RB tocavam sem o foco sair do lugar.

68. **Tamanho de chunk se testa por SUBTRAÇÃO.** `p + 8 + tam > total` dá a volta em `DWORD`
    com um tamanho corrompido como `0xFFFFFFF8`: o teste passa, e pior, `p` não avança — laço
    infinito dentro do `Iniciar()`, antes do primeiro quadro, com tela preta e nada no log.

69. **XMA nunca se troca de ordem de bytes.** Confirmado na doc do XDK (*Audio Data and
    Endianness*): *"XMA, XMA2, and xWMA audio data should never be byte-swapped"* — o que precisa
    de troca é o cabeçalho e o campo de tamanho do chunk, que é exatamente o que
    `LocalizeXma2Format` e `LerDwordLE` fazem. Confirmado também pelo `AtgAudio.cpp` do XDK, cujo
    byte-swap do `ReadSample` trata só PCM e EXTENSIBLE.

70. **A cópia com `memcpy` para a memória física não precisa de flush de cache.** A amostra lê o
    arquivo direto para o buffer do `XPhysicalAlloc`; nós lemos para um `new BYTE[]` e copiamos,
    o que levantaria a dúvida de dado sujo na L2 sendo lido por hardware. O white paper *Xbox 360
    CPU Caches* responde: o South Bridge — onde vive o decodificador XMA — **faz snoop da L2**.
    Quem não faz é a GPU.

71. **A thread do carregador saiu da thread de hardware 4 — o XAudio2 mora lá.**
    `XAUDIO2_DEFAULT_PROCESSOR` é `(XboxThread4|XboxThread5)` (`xaudio2.h:181`), e o carregador
    estava fixado justamente na 4. O sintoma enganava: o app rodava centenas de quadros na tela
    de coleções e só quebrava **ao entrar numa coleção** — que é exatamente quando a thread de
    leitura de `.assets` acorda e passa a disputar o núcleo com o motor de áudio em tempo real.
    Agora o carregador vai para a thread 2 (o desenho roda na 0; 0-1, 2-3 e 4-5 são os três
    núcleos), e o XAudio2 fica com o padrão que o XDK testa.

72. **Volume dos efeitos a 0,65.** Os `.xma` do skin da FreeStyle não têm folga: medido no PCM
    decodificado, `btn_Back` bate em **32768** — o teto absoluto — e `btn_Focus` em 32715.
    Tocados a 1,0 num motor que ainda reamostra de 44,1 kHz para os 48 kHz da mastering voice,
    o pico **entre amostras** passa do teto e corta: o som sai estourado. Na FreeStyle quem
    atenua é o XUI, que não temos. É valor de ouvido, ajustável na constante `VOLUME`.

73. **Subir `.xex` por FTP usa nome temporário.** O servidor da FreeStyle não tem retomada
    (`REST`), então um envio interrompido deixa o arquivo pela metade — e se o nome for o
    definitivo, o app fica inutilizável. Sobe-se com outro nome, confere-se o tamanho, e só
    então `DELE` + `RNFR`/`RNTO`. Confirmação final por md5, baixando de volta. Um envio
    abortado também **tranca** o arquivo temporário até a sessão cair: nem `STOR` nem `DELE`
    funcionam nele, e a saída é outro nome ou reiniciar o console.

74. **Literal estreito do fonte NÃO é UTF-8 — e isso cortava texto na tela.** Com o BOM no
    arquivo (decisão da rodada do mojibake), o `cl.exe` converte `"..."` para a codificação de
    execução: `"coleção"` vira os bytes `e7 e3 6f`, conferido no `.obj`. O `Larga()` chamava
    `MultiByteToWideChar(CP_UTF8, ...)`, e `0xE7` é começo de sequência inválida — a conversão
    **parava ali**. Sintoma: o menu mostrava "Remover da cole", e o título "Nova coleção" chegava
    cortado ao teclado do sistema.

    O BOM continua certo: ele é o que faz os literais LARGOS (`L"..."`) saírem corretos, e são a
    maioria da interface. O que faltava era o `Larga()` aceitar as duas codificações que de fato
    chegam nele — UTF-8 do SQLite e do `colecoes.txt`, ANSI dos literais do fonte.

75. **A validação de UTF-8 é nossa, e o recuo é Latin-1 puro.** `MB_ERR_INVALID_CHARS` não existe
    nos headers do Xbox, e não dá para depender de uma flag não documentada. `EhUtf8()` percorre
    os bytes conferindo o padrão de byte líder e continuação; não sendo UTF-8 válido, cada byte
    vira o ponto de código de mesmo valor. Latin-1 cobre exatamente os acentos dos nossos
    literais (`e7` = ç, `e3` = ã) sem exigir que página de código nenhuma exista no console. E
    texto UTF-8 válido nunca chega ao recuo — é o que mantém certo o nome de jogo japonês.

76. **Quadrado de coleção: 244 px, quatro por linha.** Era 180 com cinco por linha, e sobrava
    tela de qualquer jeito — a grade terminava em x=1120 e as duas linhas em y=454, numa tela de
    1280x720. Agora a grade é centralizada por cálculo (`COL_MARGEM`), não pela margem geral.

77. **A tela de coleções ROLA, não pagina.** Era `paginaInicio = (g_iCol / COL_POR_PAGINA) *
    COL_POR_PAGINA`: passar do 8º item trocava a página inteira de uma vez, e com tudo saindo
    junto perde-se a referência de onde se estava. Agora tem `g_primeiraLinhaCol` e um
    `SeguirFocoColecao()` espelhando o `SeguirFoco()` dos jogos — sobe uma linha por vez. A
    inconsistência não era decisão: foi como saiu ao implementar as duas telas em momentos
    diferentes.

78. **A capa se cria como `D3DFMT_LIN_DXT5`, não `D3DFMT_DXT5`.** No Xbox 360 a textura normal é
    **ladrilhada**, e o DDS dentro do `.assets` é **linear**, como todo DDS de PC. Pedindo o
    formato ladrilhado, o D3DX converte o layout de cada capa; pedindo o linear, usa os bytes
    como estão. Era a **única** diferença entre a nossa chamada e a do FreeStyle
    (`TextureCache.cpp:91`), e causava um fatal crash determinístico na tela de adicionar jogos,
    sempre na 45ª textura. Passamos também largura e altura explícitas, lidas do cabeçalho DDS,
    em vez de `D3DX_DEFAULT_NONPOW2`.

    A lição é a mesma de rodadas atrás, e foi preciso repetir: **conferir como o FreeStyle faz
    antes de escrever**, não depois de quebrar.

79. **Os quatro controles, somados.** Lia só `XInputGetState(0, ...)` — quem estivesse com o
    segundo controle não navegava. Não havia motivo: é um launcher de sofá. Botões entram por OU
    e, no analógico, vale o que estiver mais longe do centro, para um controle parado não anular
    o que está em uso.

80. **`XPhysicalAlloc` com alinhamento ZERO, não 2048.** A doc: *"must be a power of two that is
    greater than or equal to the page size"* — e a página do 360 é 4096. As amostras do XDK
    passam 2048 assim mesmo; zero significa "o tamanho da página", que satisfaz a doc e os 2 KB
    que o XMA exige, porque 4096 é múltiplo de 2048.

81. **O `FatalError` da ATG agora escreve no nosso log antes de morrer.** Ele faz `DebugSpew`
    (que só sai pelo XBDM, e não há depurador acoplado), `DebugBreak()` e `exit(0)` — ou seja, um
    fatal crash **mudo**. Há quatro chamadas dele só no `AtgDebugDraw`. Essa linha foi o que
    permitiu **descartar** o ring buffer como causa e procurar no lugar certo.

    Junto, cada textura deixa duas linhas no log: uma antes da criação e outra depois. Foi o par
    que mostrou que todas as 45 criações completavam — e que o problema estava depois delas.

82. **O log grava com `WriteFile` + `FlushFileBuffers`, não com `FILE*`/`fflush`.** O `fflush`
    entrega ao sistema; o que fica no cache do sistema de arquivos **se perde** num crash de
    verdade — e a linha que mais importa é sempre a última. Três rodadas de diagnóstico foram
    construídas sobre um fim de log que não era o fim da execução: o log parava sempre na mesma
    textura, e a conclusão de que o problema estava ali **estava errada**. Quem desfez isso foi
    uma observação do usuário ("apareceu normal, crashou um tempo depois"), não o código.

83. **O log tem trava.** A thread do carregador passou a escrever nele junto com a de desenho, e
    duas threads no mesmo `FILE*` sem sincronização é corrupção esperando acontecer — bug
    introduzido por mim no próprio instrumento de diagnóstico.

84. **O app tem saída: `B` na tela de coleções volta ao dashboard.** Antes não havia nenhuma — o
    único jeito de sair era lançar um jogo. Pedindo o dashboard pela Guide, o sistema encerrava o
    título com a thread do carregador viva, o áudio tocando e 120 texturas alocadas, e o
    resultado era **tela preta**. Agora passa pela mesma desmontagem do lançamento de jogo
    (decisão 41) e então `XLaunchNewImage(NULL, 0)` — `NULL` é `XLAUNCH_KEYWORD_DASH`
    (`xbox.h:423`) — de uma thread separada.

85. **O `NORETURN` pegou de novo.** O `cl.exe` recusou com `C4702` o `return` depois do
    `XLaunchNewImage` da saída, exatamente como já recusara no lançamento de jogo (decisão 36).
    Segunda vez: **nada vai depois de um `XLaunchNewImage`.**

## O crash do `DrawText`, e o que ele ensinou

Esta seção existe porque o defeito custou **oito rodadas de teste no console** e cinco hipóteses
erradas minhas. O que resolveu não foi nenhuma delas.

86. **A causa: `ATG::Font::DrawText` escreve 64 bytes além do que reserva, sempre que trunca.**
    Ele reserva `4 * (wcslen + 3)` vértices no `BeginVertices` quando há `ATGFONT_TRUNCATED`. A
    terceira reticência escreve o quad dela e sai pelo `break` **antes** do `dwNumChars--`; o laço
    de preenchimento logo abaixo escreve o saldo inteiro. Sobra 1 quad = 16 floats = **64 bytes**
    fora da região reservada. Corrigido movendo o `dwNumChars--` para antes do `break`
    (`app/vendor/atg/AtgFont.cpp` — alteração NOSSA, não do XDK).

    Explica cada peça: só nas telas com nome truncado (os dois grids usam `TRUNCATED`);
    intermitente, porque quase sempre o ring buffer tem folga e só vira violação de acesso quando
    a reserva cai perto do fim; e sensível a qualquer coisa que mude o volume de desenho.

87. **Hipóteses minhas que estavam erradas, e por que eu acreditei nelas.** Ficam registradas
    porque cada uma consumiu rodadas do usuário:
    - *"É o Forza Motorsport 4"* — o log parava sempre na textura 117. **Era artefato**: a 117 é o
      último item da janela de prefetch na posição onde ele parava de rolar. Nada quebrava ali.
    - *"É ladrilhado contra linear"* — `D3DFMT_LIN_DXT5` É a chamada correta (decisão 78) e deve
      ficar, mas **não era a causa**; só mudou o limiar.
    - *"É o anel do jogo marcado"* — a bisseção apontou para ele, mas era **timing**: menos
      desenho por quadro, outra probabilidade de a reserva cair no lugar ruim.
    - *"É o array de vértices na pilha do `DrawScreenSpaceRect`"* — **derrubada pela doc**:
      *"The vertex data passed to DrawPrimitiveUP does not need to persist after the call."*
    - *"É o ring buffer estourando"* — a doc diz que `BeginVertices` só devolve `E_OUTOFMEMORY`
      dentro de `BeginTiling`/`BeginZPass`/`BeginCommandBuffer`, e não usamos nenhum.

88. **O que de fato resolveu: parar de adivinhar e instrumentar o ponto de morte.**
    `SetUnhandledExceptionFilter` registrando `ExceptionCode`, `Iar` e `Lr`, mais `-MAP` no link
    para resolver o endereço. Entregou `0xC0000005` dentro do `DrawText` numa única sessão —
    depois de oito sessões de bisseção que só devolviam um bit cada ("travou / não travou").

    **Regra para a próxima vez: num crash sem explicação, o primeiro passo é fazer o programa
    dizer ONDE morreu, não tentar adivinhar o quê.**

89. **Log em que não se pode confiar envenena todo o diagnóstico.** Três rodadas foram construídas
    sobre "o log termina sempre no mesmo ponto" quando o log é que estava perdendo o fim
    (decisão 82). Quem desfez o engano foi uma observação do usuário, não o código. Antes de tirar
    conclusão de um log, confirme que ele sobrevive ao evento que se quer diagnosticar.

90. **Bisseção por chave no `.ini` é barata, mas cada rodada devolve UM BIT.** As chaves `som=` e
    `anel=` (`config::Ligado`) foram úteis e ficam. Mas um contraste de uma sessão vale pouco: a
    sessão que "provou" o anel tinha p≈5% de acontecer por acaso. Instrumentar vale mais que
    bisseccionar quando o espaço de hipóteses é grande.

91. **O áudio transformava crash em travamento do console.** Com som ligado, a falha pendurava a
    máquina (desligamento forçado); sem som, era fatal crash limpo e a FreeStyle voltava. O filtro
    de exceção chama `som::Calar()` → `IXAudio2::StopEngine` ("stops the audio processing thread",
    sem desmontar nada) e isso resolveu — **confirmado no console**. Não se usa `som::Parar()` ali:
    ele faz `DestroyVoice`/`Release`, que alocam e esperam, coisas que não se faz num contexto já
    faltoso. O filtro devolve `EXCEPTION_CONTINUE_SEARCH` porque a doc avisa que
    `EXCEPTION_EXECUTE_HANDLER` *"usually results in the game console freezing"*.

92. **O que mudamos na ATG vira patch versionado.** O conteúdo da ATG não entra no git — é
    código proprietário de amostra, e o `vendor-atg.sh` guarda a receita, não o conteúdo
    (`app/.gitignore` ignora `vendor/atg/`). Mas isso significava que **a correção do `AtgFont`
    não estava no repositório**: o commit `7c00ecb` descrevia uma mudança num arquivo que o git
    não tinha, e um clone novo traria o bug de volta — o mesmo que custou oito rodadas de teste.

    Agora as nossas duas alterações (a contabilidade de vértices do `AtgFont` e o `FatalError`
    que registra no nosso log) vivem em `toolchain/patches/atg.patch`, aplicado pelo
    `vendor-atg.sh` logo depois da cópia. Falhando o patch, o script **aborta** em vez de deixar
    passar um build com o bug. Os fontes do XDK vêm com CRLF e o patch é LF, então o script
    normaliza antes de aplicar.

    Verificado: cópia limpa do XDK + patch reproduz byte a byte o que estamos compilando.

93. **O `media/` se divide por procedência.** O `Arial_16.xpr` e o `SimpleShaders.fxobj` são do
    XDK e ficam fora do git pelo mesmo motivo da ATG — e o `vendor-atg.sh` **já os copiava**,
    inclusive com o aviso sobre o `fxobj`, cuja falta dá fatal crash no arranque. (Eu afirmei
    numa rodada que não havia receita nenhuma para o `media/`; estava errado, havia para dois
    dos três.)

    Os cinco `.xma` eram o caso órfão: vêm do repositório **público** da FreeStyle
    (`Skins/Default/Audio`), são 43 KB, e não há impedimento de licença do lado do XDK. Passam a
    ser **versionados**, para o build não depender de um repositório de terceiro continuar
    existindo. A ressalva de procedência dos sons segue registrada na issue #2.

    Regra que fica: **o que é do XDK, o script resolve; o que é público e pequeno, o git guarda.**
    Pendência registrada, não resolvida.

94. **A correção do `AtgFont` está confirmada no console.** Sessão completa na configuração mais
    hostil (`som=1`, `anel=1`, marcando durante o carregamento): 120 texturas, 9 marcações, zero
    linhas de `CRASH`, e volta limpa para a tela de coleções. Antes, nessa mesma configuração, a
    taxa era de uma queda a cada 5 a 7 marcações.

    Sendo honesto com o peso: aguentar 9 marcações tem ~19% de probabilidade sob a taxa antiga,
    então é evidência boa, não prova. O que sustenta a conclusão é a soma — aritmética conferida
    em todos os caminhos, desassembly mostrando o endereço do crash dentro do laço de
    preenchimento, e agora o console.

95. **Sem fonte o app não roda.** Era um `AVISO` no log seguido de nada — e o comentário dizia
    "roda sem texto", o que **nunca foi verdade**: o primeiro `DrawText` morria desreferenciando
    `m_TranslatorTable` nulo (`AtgFont.cpp:710`, sem checagem). Agora sai dizendo o que falta.
    Também não faria sentido rodar: a interface inteira é texto sobre capa.

96. **O filtro de exceção registra o endereço acessado.** `ExceptionInformation[0]` diz
    leitura ou escrita e `[1]` dá o endereço. Com esse par, o crash do `DrawText` teria sido
    fechado comparando o número com o ponteiro devolvido pelo `BeginVertices` — sem desassemblar
    o binário. Uma linha que se paga no próximo crash.

97. **O log de detalhe é ligado no console de desenvolvimento e DESLIGADO nas releases.**
    O canal vive no `diario` (`DefinirDetalhe`/`Detalhe`), não numa variável de `main.cpp` — a
    primeira tentativa foi uma guarda local, e ela deixava de fora justamente as duas linhas por
    capa da thread do **carregador**, que é quem está lendo disco. Guarda em módulo não cobre
    outro módulo.

    Custo de cada linha: um `WriteFile` mais um `FlushFileBuffers`, ou seja, uma ida síncrona ao
    disco. Com o detalhe ligado, carregar a biblioteca inteira dá cerca de 240 delas no meio do
    laço de desenho.

    **As releases ficam seguras por construção, não por disciplina:** o zip leva o `.xex` e a
    pasta `media/`, e o `collectionui.ini` **não entra no pacote** — ele nasce no console, escrito
    pelo app quando se escolhe a biblioteca. Sem a linha `logDetalhe=1` escrita à mão, o canal não
    liga. Quem empacotar uma release a partir de uma pasta que já tenha um `.ini` quebra isso.

98. **O `.ini` preserva as chaves que não são dele.** `GravarBanco` reescrevia o arquivo do zero,
    então reescolher a biblioteca apagava em silêncio `som=`, `anel=` e `logDetalhe=` postos à
    mão — enquanto o cabeçalho do `config.h` promete que dá para editá-los por FTP. Agora as
    outras linhas são lidas antes do truncamento e devolvidas depois.

99. **Valor vazio no `.ini` não liga nada.** O teste era "diferente de `0`", e `logDetalhe=`
    sozinho passava, porque o caractere seguinte era `\n`.

100. **Apagar coleção pede confirmação, com "Cancelar" em foco.** Era a única ação do app que
     destruía algo sem volta, e estava a um `A` de distância no menu.

101. **Cada efeito de som tem ganho próprio.** Os arquivos do skin da FreeStyle não vêm nivelados
     entre si: medido no PCM decodificado, o RMS vai de −12,3 dBFS (`btn_Focus`) a −21,5
     (`btn_Select`), e o `flyout` do menu estava 6 dB acima do som de confirmar. Nivelados por
     volta de −19,5 dBFS, com a mão leve no som de foco, que tem 0,09 s — som curto soa mais baixo
     do que o RMS sugere, e corrigir o valor inteiro o faria sumir.

102. **Como a versão sobe.** Três dígitos, `MAIOR.MENOR.CORREÇÃO`, decididos pelo que muda **para
     quem usa**, não pelo tamanho do diff:

     - **MAIOR** (`2.0.0`): quebra de compatibilidade de dados ou mudança de propósito. O caso
       típico seria um `colecoes.txt` que a versão anterior não consegue ler. Até hoje não houve.
     - **MENOR** (`1.1.0`): o usuário passa a conseguir fazer algo que não conseguia. Vale tanto
       para funcionalidade nova quanto para algo que existia e **não funcionava** passar a
       funcionar — foi o caso dos containers STFS/GOD, que destravaram 55 dos 120 jogos.
     - **CORREÇÃO** (`1.1.1`): defeito resolvido, ajuste de interface, desempenho. Nada que o
       usuário não pudesse fazer antes.

     A régua prática: **se a lista do que o app faz muda, é MENOR; se só muda a qualidade com que
     ele faz, é CORREÇÃO.**

     Erro já cometido, registrado para não repetir: a correção dos containers saiu como `1.0.2`
     porque eu a tratei como "mais uma correção". Ela destravou quase metade da biblioteca, e
     pela régua acima era `1.1.0`. A release errada foi removida e republicada.

103. **Nota de release não afirma resultado de teste que não foi verificado.** As notas da `1.0.2`
     diziam "containers seguem sem abrir" porque eu interpretei um "tudo ok" do usuário como
     sendo só da parte visual, sem perguntar e sem conferir. O log não serve de prova aqui: ele é
     truncado a cada execução, então o registro do teste já tinha sido sobrescrito.

104. **Coleção dinâmica: decisões fechadas.** União de outras coleções, referenciadas por **id** e
     não por nome, para renomear não quebrar. Sem aninhamento: dinâmica não contém dinâmica, o que
     elimina ciclo por construção. Não guarda jogos próprios — ou é junção, ou é lista. Não se
     remove jogo de dentro dela; tira-se da origem. Apagar **uma** origem não apaga a dinâmica, o
     id só some da união; apagar **todas** apaga, porque união sem origem perdeu o sentido.

     No card, a dinâmica se distingue por um **símbolo de raio** no canto, gerado no arranque como
     a máscara de canto arredondado — sem arquivo novo.

105. **A união não guarda lista resolvida: ela responde "este jogo entra?".** A primeira versão
     tinha um `Resolver()` que devolvia o vetor de TitleIds da união. Ele nunca chegou a ser
     chamado — os três lugares que precisavam dele continuaram usando `Tem()`, que só olhava
     `ids`, sempre vazio numa união. **A união aparecia e abria vazia, e a funcionalidade inteira
     era código morto.** Pego por agente de revisão, não por teste.

     A correção não foi passar a chamar `Resolver`: foi levar a união para dentro do `Tem()` e
     apagar o `Resolver`. Os três chamadores varrem a biblioteca perguntando item por item se
     entra na coleção; devolver o vetor resolvido a cada um deles seria **alocação dentro do laço
     de desenho** — oito cards vezes a biblioteca inteira, por quadro.

     O guarda de ciclo mora nesse `Tem()`: ele só desce em origem que **não** é união. Um
     `colecoes.txt` editado à mão com uma união apontando para si mesma para ali, em vez de
     recorrer sem fim.

106. **Apagar conserta as uniões na hora, não no próximo arranque.** `LimparUnioes()` só rodava no
     `Carregar()`. Faltando no `Apagar()`, três coisas quebravam, e a terceira é a séria:
     `ProximoId()` devolve `maior + 1` sobre as coleções **vivas**, então apagar a de maior id
     libera aquele id. Com a referência pendurada no disco, a próxima coleção criada herdava o id
     e **entrava numa união sem ninguém ter pedido** — com o arquivo internamente coerente, isto
     é, reiniciar não consertava. Corrupção silenciosa e permanente.

107. **Abrir com número não diz que a linha é do formato novo.** O teste de migração era "dígitos
     até a primeira barra". Uma coleção chamada `1942` ou `007` — nada improvável num app de
     jogos — casava, as outras barras não eram achadas e a linha era **descartada em silêncio**.
     Como `Gravar()` reescreve o arquivo inteiro a partir do que foi lido, a primeira gravação
     seguinte apagava a coleção do disco para sempre.

     Agora quem decide é o segundo campo: no formato antigo ali vão TitleIds em hexa, nunca as
     palavras `jogos` ou `uniao`. E a regra que vale para todo leitor de arquivo daqui em diante:
     **linha que não entendi eu preservo, não descarto** — numa leitura que alimenta uma
     reescrita total, descartar é apagar.

108. **ROM de emulador não está em banco nenhum.** O `content.db` da FreeStyle tem oito tabelas
     (`ContentItems`, `ContentTypes`, `Favorites`, `HttpQueue`, `MountedDevices`,
     `RecentlyPlayedTitles`, `TitleUpdates`, `UserRatings`) e **nenhuma guarda ROM**. A FreeStyle
     indexa só o que o 360 lança: XEX, XBE e container. Os emuladores são itens comuns da
     biblioteca — o Snes360 é o `FFED0707` que já aparecia no `colecoes.txt`, com
     `ContentItemPath = \EMULADORES\Snes360\Snes360.xex`.

     As ROMs são arquivo solto dentro da pasta do emulador: `Snes360/Roms/*.smc`, `Ps1/roms/*.bin`,
     `FBANext*/roms/*.zip`. Trazê-las não é ler mais uma tabela — é o CollectionUI virar uma
     segunda fonte, que varre pasta.

     Achado lateral: `EMUS/SNES360/Preview` **não** é instalação velha. É o `PreviewPath` do
     `settings.xml` do Snes360, onde as imagens dele moram. Está vazia.

109. **Id sintético para item de emulador.** Todas as 20 ROMs de SNES compartilham o TitleId do
     Snes360: marcar uma marcaria as vinte. A identidade de item de emulador passa a ser derivada
     de emulador + nome do arquivo, em 32 bits, ocupando o mesmo campo do TitleId.

     Escolhido assim porque **não mexe em nada**: `Tem`, `Alternar`, `Remover` e o formato do
     `colecoes.txt` continuam iguais. Colisão com TitleId real é possível e tolerável — o efeito
     seria dois itens sempre marcados juntos, que é exatamente o caso de multi-disco, já previsto
     e já suportado pela tela.

110. **Lançar a ROM direto não é bifurcação de projeto: é tentativa grátis.** `XSetLaunchData`,
     `XGetLaunchData` e `XGetLaunchDataSize` estão exportadas em `xav.lib` e `xapilib.lib` e
     **não aparecem em header nenhum** do XDK — mesma situação do `XNotifyQueueUI`, que já se
     declara à mão. Gravar o caminho da ROM antes de lançar custa dez linhas.

     Dos emuladores instalados, nenhum anuncia ler isso: o Readme do Snes360 descreve a operação
     inteira sem mencionar, o `default.ini` do pcsxr-360 é `[pcsx] nothing = 0`, e o
     `FBANext.ini` só tem lista de recentes (`szPrevGames[0..9]`), não autoload.

     Então a regra: **constrói-se a versão que cai no menu do emulador, e grava-se a launch data
     na saída de qualquer jeito.** Quem souber ler abre direto; quem não souber ignora e abre o
     menu, que é o comportamento aceito. Não há o que decidir hoje, e um emulador que leia entra
     funcionando sem mudança.

111. **Capa de ROM não precisa do formato da FreeStyle.** A textura nasce de
     `D3DXCreateTextureFromFileInMemoryEx`, que decodifica JPG, PNG, BMP, DDS e TGA de bytes em
     memória. O container `FSDA` é como a FreeStyle guarda a arte dela, não um requisito nosso:
     para ROM, arquivo comum na pasta serve.

     De onde vem essa arte continua em aberto — nem o `Preview` do Snes360 nem a `covers/` do Ps1
     têm imagem dos jogos que ele tem.

112. **Maiúsculas só nos cards.** O caixa alta entra dentro do `QuebrarNome`, que é usada em um
     único lugar — o nome do card de coleção. A grade de jogos mostra o nome como ele é.

     Dois cuidados: a conversão acontece **antes** de medir o texto, porque maiúscula é mais larga
     e medir o original erraria a quebra de linha por alguns pixels; e ela não usa `towupper` nem
     `_wcsupr`, que só levantam ASCII enquanto a localidade for a "C" — a faixa 0x00E0–0x00FE do
     Latin-1 sobe subtraindo 0x20, pulando o 0x00F7, que é o sinal de divisão e não tem par.

113. **Lançar a ROM direto: não dá com estes emuladores, e o "de graça" que eu disse estava
     errado.** Lido o fonte dos três: Snes360 e FBANext não chamam `XGetLaunchData` em canto
     nenhum, não leem linha de comando (o `GetCommandLine()` do FBANext é código morto) e não têm
     chave de autoload. O `szPrevGames` do FBANext é lista de recentes consumida só pelo menu da
     versão Windows, que não é compilada no 360 — escrever o ini não faz nada.

     Existe formato de facto na cena: a struct do Aurora, `'AUOA'`/`'ROMS'`, 656 bytes
     (device NT + caminho relativo + nome do arquivo). Lê quem: **pcsxr-360 2.1.1a** — na 2.1.0
     desta casa o bloco está comentado como WIP — e o RetroArch até a v1.7.0. O RetroArch de hoje
     usa outra coisa: string ASCII crua terminada em NUL, e o core se escolhe por **qual .xex se
     lança**, porque é um xex por core, estático.

     Correção da decisão 110: os dois formatos **são mutuamente exclusivos**. A struct do Aurora
     mandada ao RetroArch de hoje vira um caminho chamado `AUOAROMS…`. Continua barato, mas é por
     alvo, não universal — e para os três emuladores de hoje não há o que mandar.

114. **A capa da ROM vem do xbox-vault, composta no PC.** O `Documentos/xbx` tem `data/emu.json`
     com 9.830 jogos de emulador (`system`, `title`, id em slug) e 15.791 capas já baixadas. Ganha
     do `libretro-thumbnails` por três motivos: já está no disco, não precisa de conta nem rede, e
     é a curadoria do próprio usuário.

     **A proporção é o problema de verdade, e é físico.** A caixa do SNES americano é de papelão
     deitada e o PS1 é jewel case de CD, cujo encarte é quadrado — nenhum acervo do mundo tem isso
     em retrato, porque a capa não é retrato. Num slot 1:1,425, recortar para preencher come o
     logo ("DONKEY KONG COUNTRY" vira "EY KONG NTRY") e encaixar inteiro deixa metade do card
     vazio.

     A saída: **caber inteira sobre um fundo borrado tirado da própria capa**, e fazer isso **no
     PC**, gravando o arquivo já em 146×208. O app não ganha nenhuma lógica de letterbox, nenhum
     custo de memória, e o cache de 160 texturas não sente. As 16 ROMs desta casa deram 164 KB.

115. **O app acha a capa da ROM por nome de arquivo, e só.** Procura
     `<pasta de arte>\<nome da ROM sem extensão>.jpg`, depois `.png`, senão cai no espaço vazio.
     Uma função, sem normalização, sem casamento aproximado, sem banco.

     O casamento título↔acervo fica **fora** do app, numa tabela escrita à mão. Medido contra as
     ROMs reais: 13 de 16 casam sozinhas, mas uma delas casa **errado e em silêncio** —
     `Super Mario World 2.smc` tem 0,94 de similaridade com "Super Mario World" e é, na verdade,
     o Yoshi's Island. Capa errada sem aviso é pior que capa faltando, e as três que falham
     (`Contra Spirits`, `TMNT IV`, `Dixie Kong's Double Trouble`) são justamente as que o usuário
     renomeou. Automatizar custaria mais que escrever dezesseis linhas.

     O ganho de fora: para corrigir qualquer capa, basta largar um arquivo com o nome da ROM na
     pasta. Sem config, sem reiniciar nada.

116. **O caminho da capa mora no `Jogo`, não no `main.cpp`.** Ele era montado em dois pontos da
     tela (`PastaArte` + `%08X.assets`), e agora é preenchido na leitura: jogo da FreeStyle ganha
     o `.assets`, ROM ganha o `.jpg`. A tela só faz `carregador::Pedir(id, j->capa)` e pula se
     estiver vazio.

     O `carregador` passa a decidir **pelo conteúdo**: o que abre com `FSDA` vai pelo container,
     o resto é imagem solta lida inteira. Um `.assets` renomeado continua funcionando e um `.jpg`
     com nome errado não é interpretado como container. A leitura crua tem teto de 8 MB — folga
     enorme para uma capa, e existe porque um caminho errado pode cair num `.bin` de ROM de
     600 MB, e aí o console morre na alocação em vez de numa linha de log.

117. **A ROM reaproveita o lançador inteiro.** O item de ROM sai com o `caminho` e o
     `tipoArquivo` do **próprio emulador**, então abrir uma ROM é abrir o emulador pelo mesmo
     código que abre qualquer jogo — nenhuma exceção no lançador.

     O `id` da ROM é **negativo** (`-(n+1)`). Ele é a chave do cache de texturas, e `ContentItemId`
     é sempre positivo: assim uma ROM nunca disputa a entrada de cache de um jogo da FreeStyle.
     Quem vai para o disco é o `titleId`, que é o id sintético da decisão 109.

     A varredura não tem tabela por emulador. Procura as pastas `Roms`/`roms`/`games` ao lado do
     xex e aceita uma lista generosa de extensões — amarrar extensão a titleId quebraria na
     próxima versão do emulador, amarrar à pasta vale para qualquer um que apareça depois. O
     dispositivo sai do `lancador::Resolver`, não de um `Hdd:` suposto: o emulador pode estar num
     pendrive.

118. **`biblioteca::Ordenar` existe solta.** As ROMs entram depois do `Ler`, e a tela conta com a
     lista ordenada — é dela que saem o índice alfabético e o salto por letra. Sem reordenar, as
     ROMs ficariam todas no fim e o salto por letra mentiria.

119. **Uma grafia por pasta de ROM — o `FindFirstFile` também não distingue caixa.** A primeira
     versão listava `Roms`, `roms` e `ROMS` "porque o sistema de arquivos não distingue, mas o
     `FindFirstFile` é alimentado com o nome exato". A segunda metade da frase é falsa: a XAPI
     desce para `NtCreateFile` com `OBJ_CASE_INSENSITIVE`, igual ao Windows de PC. As três abriam
     **o mesmo diretório**, e cada ROM entrava **três vezes** — três cards iguais, três cópias da
     mesma capa no cache de 160, a contagem do card triplicada, e o custo da ordenação
     multiplicado por nove, porque é quadrático no número de ROMs.

     Regra que fica: no console, uma grafia por nome de arquivo. Repetir caixa não é redundância
     inofensiva, é duplicata.

120. **`.cue` e `.bin` do mesmo jogo são um item só.** Um rip de PS1 é um descritor ao lado dos
     dados, e os dois estão na lista de extensões. Sem descarte, o mesmo jogo entrava duas vezes
     — e com ids sintéticos **diferentes**, porque o hash é sobre o nome com extensão. O usuário
     marcaria um, veria o outro desmarcado, e o sintoma seria indistinguível de bug de
     persistência.

     A varredura passou a ser em duas passadas: recolhe os nomes da pasta, anota quais bases têm
     descritor (`.cue`/`.pbp`/`.iso`) e, na segunda, recusa o `.bin`/`.img` de base já coberta.

121. **O id sintético normaliza caixa também em Latin-1.** A primeira versão só baixava A–Z, e o
     comentário dela já declarava o invariante que ela não cumpria. `Pokémon.smc` renomeado para
     `POKÉMON.smc` dava **outro hash** — e como esse número está no `colecoes.txt`, a ROM sumiria
     de todas as coleções em que estava, em silêncio, sem o arquivo ter mudado de conteúdo.

     A armadilha que explica o descuido: **`char` é signed no cl.exe do XDK**, então `c >= 0xC0`
     sem converter para `unsigned char` é sempre falso. A faixa não foi esquecida — ela foi
     escrita de um jeito que nunca executa.

122. **Só se lê cabeçalho DDS depois de confirmar que é DDS.** A largura e a altura saíam dos
     bytes 12–19 de tudo que chegasse ao criador de textura. Isso valia enquanto tudo vinha de um
     container FSDA; com a capa de ROM, chega JPG e PNG cru, e aqueles bytes são lixo. O lixo
     hoje cai fora da faixa 1..4096 e é descartado — mas isso é sorte da codificação, e a
     decisão 115 convida o usuário a largar qualquer imagem na pasta.

123. **Lista nova entra por intercalação, não por "acrescenta e reordena".** A ordenação é
     inserção simples, e acrescentar as ROMs no fim deixava uma cauda desordenada — quadrático no
     número de ROMs, com cada troca copiando um `Jogo` de nove `std::string`. Com um romset de
     arcade de alguns milhares de zips, isso é tela preta por dezenas de segundos, com cara de
     console travado. `Juntar` ordena só a cauda e intercala numa passada.

124. **Um nível de subpasta na varredura de ROM.** O `roms/` do FBANext não tem ROM nenhuma: tem
     subpasta por sistema (`Arcade`, `megadrive`, `neocdz`, `pce`). Sem descer, o emulador de
     arcade aparecia sem um único jogo.

     Um nível só, de propósito: varrer fundo custa tempo de arranque e entra em pasta de save,
     de arte e de configuração. O nome da subpasta entra no id sintético — senão
     `Arcade\sonic.zip` e `megadrive\sonic.zip` receberiam o mesmo id. Na raiz o rótulo é vazio,
     então **as ROMs que já estão em coleção não mudam de id**.

125. **Capa deitada é girada; o critério é a proporção, não o sistema.** A caixa americana do SNES
     é deitada (1,41) e o slot é em pé (1,425) — girada, a capa ocupa **99% do card em vez de
     49%**, sem recortar nada. Gira no sentido anti-horário, com o título lendo de baixo para cima.

     Decidir pela proporção da imagem, e não por uma tabela de sistema, não foi só economia: das
     23 capas, 18 giraram e 5 ficaram em pé — as três de PS1 (jewel case quadrado, 1,01) e
     **Great Battle IV e V, que são japonesas, e a caixa japonesa de SNES é em pé** (240×432). Uma
     tabela "SNES gira" teria deitado essas duas.

     O limite é 1,15: deixa de fora o quadrado do PS1 e o flyer de arcade, que já nasce em pé.

126. **A migração do `colecoes.txt` foi conferida no console, não presumida.** Primeiro arranque
     com o formato novo: as sete coleções saíram intactas, com id 1 a 7, nomes preservados
     inclusive o acentuado ("não zerados") e os mesmos TitleIds. E a "versus 4 players" já traz um
     id sintético de ROM gravado — prova de que a cadeia inteira (varredura, id, persistência)
     fecha ponta a ponta.
