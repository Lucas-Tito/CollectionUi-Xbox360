#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Monta as capas das ROMs de emulador, no formato que o console consome.

Por que isto roda no PC e nao no app: a capa de SNES e DEITADA (a caixa de papelao
americana) e a de PS1 e QUADRADA (encarte de jewel case). O slot da grade e retrato,
146x208. Recortar para preencher come o logo -- "DONKEY KONG COUNTRY" vira "EY KONG
NTRY" -- e encaixar inteiro deixa metade do card vazio. A saida e a capa inteira sobre
um borrao dela mesma, e fazer isso aqui significa que o app nao ganha uma linha de
logica de proporcao: ele le um .jpg que ja esta no tamanho certo.

O casamento ROM -> jogo tambem fica aqui, na tabela abaixo, escrito a mao. Casamento
aproximado acerta 13 de 16 destas ROMs, mas erra UMA EM SILENCIO: "Super Mario World
2.smc" tem 0,94 de similaridade com "Super Mario World" e e, na verdade, o Yoshi's
Island. Capa errada sem aviso e pior que capa faltando. Ver a decisao 115.

Uso:
    python3 ferramentas/capas-de-rom.py [--vault DIR] [--saida DIR]

Depois, suba a pasta de saida para <pasta do CollectionUI no console>\\capas\\.
Para corrigir uma capa, basta largar ali um .jpg com o nome exato do arquivo da ROM.
"""
import argparse
import os
import sys

try:
    from PIL import Image, ImageFilter
except ImportError:
    sys.exit("falta a Pillow: pip install Pillow")

# O slot da grade, em app/main.cpp (CAPA_L x CAPA_A).
LARGURA, ALTURA = 146, 208

# nome do arquivo da ROM  ->  id do jogo no xbox-vault
#
# Esta tabela tem de cobrir a pasta de ROMs INTEIRA. A primeira versao cobria 16 de 23
# porque foi montada a partir de uma listagem FTP truncada -- sete jogos apareceram no
# console sem capa. Confira contra a pasta antes de rodar.
TABELA = [
    ("Contra III - The Alien Wars.smc",     "emu-snes-contra-iii-the-alien-wars"),
    # Nome japones do Contra III: o mesmo jogo, e de proposito a mesma capa.
    ("Contra Spirits.smc",                  "emu-snes-contra-iii-the-alien-wars"),
    ("Metal Warriors.smc",                  "emu-snes-metal-warriors"),
    ("Mega Man X.smc",                      "emu-snes-mega-man-x"),
    ("Super Mario Kart.smc",                "emu-snes-super-mario-kart"),
    ("Super Mario World 1.smc",             "emu-snes-super-mario-world"),
    # NAO e "Super Mario World": e o Yoshi's Island. Era o erro silencioso.
    ("Super Mario World 2.smc",             "emu-snes-super-mario-world-2-yoshi-s-island"),
    ("Top Gear 2.smc",                      "emu-snes-top-gear-2"),
    ("Donkey Kong Country.smc",             "emu-snes-donkey-kong-country"),
    ("Dixie Kong's Double Trouble (U).smc", "emu-snes-donkey-kong-country-3-dixie-kong-s-double-trouble"),
    ("TMNT IV - Turtles in Time.smc",       "emu-snes-teenage-mutant-ninja-turtles-iv-turtles-in-time"),
    ("Super Bomberman 3 (E).fig",           "emu-snes-super-bomberman-3"),
    ("Saturday Night Slam Masters (U).smc", "emu-snes-saturday-night-slam-masters"),
    ("Donkey Kong Country 2.smc",            "emu-snes-donkey-kong-country-2-diddy-s-kong-quest"),
    ("Final Fight (U).smc",                  "emu-snes-final-fight"),
    ("Kirby Super Star (U) [!].smc",         "emu-snes-kirby-super-star"),
    ("Lost Vikings.sfc",                     "emu-snes-the-lost-vikings"),
    # "Goof Troop" tambem casa com tres hacks no catalogo (Space Treasure, Le Goof
    # Troop, VP of Goof Troop). O jogo e o da Disney.
    ("Goof Troop (U) [!].smc",               "emu-snes-disney-s-goof-troop"),
    ("Great Battle IV.smc",                  "emu-snes-the-great-battle-iv"),
    ("Great Battle V.smc",                   "emu-snes-the-great-battle-v"),
    ("Crash Team Racing.bin",               "emu-ps1-ctr-crash-team-racing"),
    ("Micro Machines V3.img",               "emu-ps1-micro-machines-v3"),
    ("twisted_metal_4.bin",                 "emu-ps1-twisted-metal-4"),
]


# Acima disto a imagem conta como deitada e e girada para caber em pe. 1,15 deixa de
# fora o quadrado do jewel case de PS1 (1,01), que girado nao ganharia nada, e o flyer
# de arcade, que ja nasce em pe.
DEITADA = 1.15


# ROMs de arcade. O zip usa o nome curto do MAME, que nao diz nada na grade e nao tem
# capa no xbox-vault (que so cobre PS1, SNES e GBA). Entao aqui vao tres coisas por
# linha: o zip, como mostrar, e o nome no acervo do libretro (FBNeo - Arcade Games).
#
# O nome do libretro NAO e o do MAME: ele usa a descricao da DAT do FBNeo, com os
# caracteres &*/:`<>?|" trocados por _. Casar por similaridade erra feio aqui --
# "Killer Instinct" vira "Power Instinct" e "KOF '98" vira "KOF '97 Plus (bootleg)".
ARCADE = [
    ("mslug.zip",   "Metal Slug",                "Metal Slug - Super Vehicle-001"),
    ("mslug2.zip",  "Metal Slug 2",              "Metal Slug 2 - Super Vehicle-001_II (NGM-2410 ~ NGH-2410)"),
    ("mslug3.zip",  "Metal Slug 3",              "Metal Slug 3 (NGH-2560)"),
    ("mslug4.zip",  "Metal Slug 4",              "Metal Slug 4 (NGH-2630)"),
    ("mslug5.zip",  "Metal Slug 5",              "Metal Slug 5 (bootleg)"),
    ("mslug6.zip",  "Metal Slug 6",              "Metal Slug 6 (Metal Slug 3 bootleg) [Bootleg]"),
    ("kof98.zip",   "The King of Fighters '98",
     "The King of Fighters '98 - The Slugfest _ King of Fighters '98 - Dream Match Never Ends (NGM-2420)"),
    ("kof2003.zip", "The King of Fighters 2003", "The King of Fighters 2003 (NGH-2710)"),
    ("mvsc.zip",    "Marvel vs. Capcom",         "Marvel Vs. Capcom_ Clash of Super Heroes (USA 971222)"),
    # Naomi, nao FBNeo: o acervo de arcade do libretro nao tem. Fica sem capa, com o
    # nome certo -- melhor que capa errada.
    ("mvsc2u.zip",  "Marvel vs. Capcom 2",       None),
    ("megaman.zip", "Mega Man: The Power Battle","Mega Man - the power battle (951006 USA)"),
    ("megaman2.zip","Mega Man 2: The Power Fighters",
     "Mega Man 2 - the power fighters (960708 USA)"),
    ("contra.zip",  "Contra",                    "Contra (US _ Asia, set 1)"),
    ("gaia.zip",    "Gaia Crusaders",            "Gaia Crusaders"),
    ("alexkidd.zip","Alex Kidd: The Lost Stars", "Alex Kidd_ The Lost Stars (set 2, unprotected)"),
    ("Killer Instinct.zip", "Killer Instinct",
     "Killer Instinct (ROM ver. 1.5d) [Works best in 64-bit build]"),
    ("kinst2.zip",  "Killer Instinct 2",
     "Killer Instinct II (ROM ver. 1.4) [Works best in 64-bit build]"),
]

LIBRETRO = ("https://thumbnails.libretro.com/"
            "FBNeo%20-%20Arcade%20Games/Named_Boxarts/")


def baixar_arcade(nome_libretro, destino):
    """Baixa a capa do libretro para um arquivo de cache. Devolve o caminho ou None."""
    import urllib.parse
    import urllib.request

    if os.path.exists(destino):
        return destino

    saneado = nome_libretro
    for c in '&*/:`<>?|"':
        saneado = saneado.replace(c, "_")

    # Duas tentativas: 404 e nome errado e nao adianta insistir, mas timeout e so o
    # servidor engasgando -- na primeira rodada o Killer Instinct caiu por isso.
    dados = None
    for tentativa in range(2):
        try:
            with urllib.request.urlopen(LIBRETRO + urllib.parse.quote(saneado) + ".png",
                                        timeout=60) as r:
                dados = r.read()
            break
        except urllib.error.HTTPError:
            return None
        except Exception:
            continue
    if dados is None:
        return None

    with open(destino, "wb") as f:
        f.write(dados)
    return destino


def compor(origem):
    """Capa inteira, centralizada, sobre um borrao escurecido dela mesma."""
    im = Image.open(origem).convert("RGB")

    # A caixa do SNES e deitada (1,41) e o slot e em pe (1,425): girada, a capa ocupa
    # 99% do card em vez de 49%, sem recortar nada. O criterio e a PROPORCAO da imagem,
    # nao o sistema -- assim PS1 e arcade ficam de fora sozinhos, e um sistema novo se
    # resolve sem tabela. Sentido anti-horario: o titulo le de baixo para cima.
    if im.width > im.height * DEITADA:
        im = im.rotate(90, expand=True)

    razao = max(LARGURA / im.width, ALTURA / im.height)
    cheia = im.resize((max(1, int(im.width * razao)), max(1, int(im.height * razao))),
                      Image.LANCZOS)
    x = (cheia.width - LARGURA) // 2
    y = (cheia.height - ALTURA) // 2
    fundo = cheia.crop((x, y, x + LARGURA, y + ALTURA))
    fundo = fundo.filter(ImageFilter.GaussianBlur(14))
    fundo = Image.eval(fundo, lambda v: int(v * 0.55))

    razao = min(LARGURA / im.width, ALTURA / im.height)
    frente = im.resize((max(1, int(im.width * razao)), max(1, int(im.height * razao))),
                       Image.LANCZOS)
    fundo.paste(frente, ((LARGURA - frente.width) // 2, (ALTURA - frente.height) // 2))
    return fundo


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--vault", default=os.path.expanduser("~/Documentos/xbx"),
                    help="pasta do xbox-vault (a que tem images/)")
    ap.add_argument("--saida", default="capas", help="onde gravar os .jpg")
    args = ap.parse_args()

    imagens = os.path.join(args.vault, "images")
    if not os.path.isdir(imagens):
        sys.exit("nao achei %s -- aponte --vault para a pasta do xbox-vault" % imagens)

    os.makedirs(args.saida, exist_ok=True)
    feitas, faltando, total = 0, [], 0

    for rom, slug in TABELA:
        origem = os.path.join(imagens, slug + ".webp")
        if not os.path.exists(origem):
            faltando.append((rom, slug))
            continue

        # O nome do arquivo E a chave: o app procura <nome da ROM sem extensao>.jpg,
        # sem normalizar nada. Trocar este nome e trocar o que o console acha.
        destino = os.path.join(args.saida, os.path.splitext(rom)[0] + ".jpg")
        compor(origem).save(destino, "JPEG", quality=88)
        total += os.path.getsize(destino)
        feitas += 1

    # Arcade: capa do libretro, com cache em disco para nao rebaixar a cada rodada.
    cache = os.path.join(args.saida, ".cache-arcade")
    os.makedirs(cache, exist_ok=True)
    nomes = []

    for zipn, mostrar, libretro in ARCADE:
        nomes.append((zipn, mostrar))
        if libretro is None:
            faltando.append((zipn, "sem capa no acervo de arcade"))
            continue

        bruto = baixar_arcade(libretro, os.path.join(cache, zipn + ".png"))
        if bruto is None:
            faltando.append((zipn, libretro))
            continue

        destino = os.path.join(args.saida, os.path.splitext(zipn)[0] + ".jpg")
        compor(bruto).save(destino, "JPEG", quality=88)
        total += os.path.getsize(destino)
        feitas += 1

    # O arquivo que o app le para mostrar "Metal Slug 3" em vez de "mslug3". O nome do
    # ARQUIVO nao muda: renomear o zip quebraria o romset do FBANext, e e do nome do
    # arquivo que saem o id sintetico e a busca da capa.
    with open(os.path.join(args.saida, "nomes.txt"), "w", encoding="utf-8") as f:
        f.write("# CollectionUI: nome de exibicao das ROMs.\n")
        f.write("#   arquivo da ROM, barra vertical, nome a mostrar\n")
        f.write("# Mexer aqui nao tira a ROM das colecoes: o id sai do nome do arquivo.\n")
        for zipn, mostrar in nomes:
            f.write("%s|%s\n" % (zipn, mostrar))

    print("%d capas em %s (%d KB)" % (feitas, args.saida, total // 1024))
    print("nomes.txt com %d linhas" % len(nomes))
    for rom, slug in faltando:
        print("  SEM IMAGEM NO VAULT: %-42s -> %s" % (rom, slug))
    return 1 if faltando else 0


if __name__ == "__main__":
    sys.exit(main())
