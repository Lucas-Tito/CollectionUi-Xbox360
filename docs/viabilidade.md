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
