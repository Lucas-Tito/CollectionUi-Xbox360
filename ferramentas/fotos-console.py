#!/usr/bin/env python3
"""Fotos de todas as telas do app no console, por um roteiro de controle, sem ninguém no sofá.

Uso: fotos-console.py ROTEIRO [--saida DIR] [--sem-subir] [--limite MIN] [--ip IP]

  1. pega a trava de uso do console (a mesma do bt3xbx: ~/.config/console360/uso.lock)
  2. confere pelo xbdm que o console está parado na dashboard (senão recusa: alguém jogando)
  3. pelo FTP da FreeStyle: guarda o colecoes.txt de antes, sobe o .xex (Release; --sem-subir pula) e o roteiro
     como roteiro.txt, com nome temporário, DELE, RNFR/RNTO e md5 de volta
  4. abre o app pelo xbdm (magicboot) e vigia até ele voltar à dashboard (o roteiro termina com `sai`), com limite
  5. baixa o diário, cada foto que o roteiro pede (game:\\fotos\\NOME.bmp) e confere que o colecoes.txt não mudou
  6. converte as fotos para PNG (com o Pillow, se houver) em DIR

O formato do roteiro está em app/src/roteiro.h. As regras do console são as do bt3xbx (guia de port, "O xbdm no
console"): só abrir na dashboard, toda abertura com vigia que termina sozinho, uma conexão curta por comando, o FTP
só com o console na FreeStyle, uma conexão por vez, sem QUIT, sem REST, RETR depois de CWD.

O que é da instalação fica fora do repositório:
  ~/.config/bt3xbx/maquinas.env       BT3_CONSOLE_IP (dica; se não responder, procura na rede) e BT3_CONSOLE_DASH
  ~/.config/collectionui/console.env  CUI_CONSOLE_PASTA, a pasta do app no FTP (ex.: /Hdd1/Apps/CollectionUI)
"""
import argparse, concurrent.futures, fcntl, ftplib, hashlib, io, os, re, socket, subprocess, sys, time

AQUI = os.path.dirname(os.path.abspath(__file__))
XEX_LOCAL = os.path.join(AQUI, "..", "app", "build", "Release", "bin", "collectionui.xex")
XBDM = 730


def conf(chave, arquivos=("~/.config/collectionui/console.env", "~/.config/bt3xbx/maquinas.env")):
    if os.environ.get(chave):
        return os.environ[chave]
    for a in arquivos:
        try:
            with open(os.path.expanduser(a), encoding="utf-8") as f:
                for l in f:
                    l = l.strip()
                    if l.startswith(chave + "="):
                        return l.split("=", 1)[1].split("#", 1)[0].strip().strip("\"'")
        except OSError:
            pass
    return None


def avisa(t):
    print(t, flush=True)


# ---- trava ---------------------------------------------------------------------------------------------------------
def pega_trava():
    if os.environ.get("BT3_CONSOLE_TRAVA_PRESA") == "1":
        return None
    caminho = os.environ.get("BT3_CONSOLE_TRAVA") or os.path.expanduser("~/.config/console360/uso.lock")
    os.makedirs(os.path.dirname(caminho), exist_ok=True)
    f = open(caminho, "a+")
    try:
        fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        f.seek(0)
        raise SystemExit("RECUSADO: o console está em uso (%s); trava em %s" % (f.read().strip() or "?", caminho))
    f.seek(0); f.truncate()
    f.write("collectionui pid %d: fotos-console\n" % os.getpid()); f.flush()
    return f


# ---- xbdm ----------------------------------------------------------------------------------------------------------
def xbdm(ip, texto, tempo=10.0):
    s = socket.create_connection((ip, XBDM), timeout=tempo)
    try:
        f = s.makefile("rb")
        if not f.readline().decode(errors="replace").startswith("201"):
            raise RuntimeError("xbdm: saudação inesperada")
        s.sendall((texto + "\r\n").encode())
        primeira = f.readline().decode(errors="replace").strip()
        codigo = int(primeira[:3]) if primeira[:3].isdigit() else -1
        linhas = [primeira[5:]]
        if codigo == 202:
            linhas = []
            while True:
                x = f.readline().decode(errors="replace").rstrip("\r\n")
                if x in (".", ""):
                    break
                linhas.append(x)
        try:
            s.sendall(b"bye\r\n")
        except OSError:
            pass
        return codigo, linhas
    finally:
        s.close()


def rodando(ip):
    _, linhas = xbdm(ip, "xbeinfo running")
    m = re.search(r'name="([^"]*)"', " ".join(linhas))
    return m.group(1) if m else " ".join(linhas)


def na_dash(ip):
    dash = conf("BT3_CONSOLE_DASH")
    if not dash:
        raise SystemExit("ERRO: sem BT3_CONSOLE_DASH (trecho do caminho da dashboard) no maquinas.env")
    return dash.lower() in rodando(ip).lower()


def porta(ip, p, espera=1.5):
    try:
        with socket.create_connection((ip, p), timeout=espera):
            return True
    except OSError:
        return False


def achar(ip):
    for c in [ip, conf("BT3_CONSOLE_IP")]:
        if c and porta(c, XBDM):
            return c
    saida = subprocess.run(["ip", "-4", "-o", "addr"], capture_output=True, text=True).stdout
    prefixos = sorted({l.split()[3].split("/")[0].rsplit(".", 1)[0] for l in saida.splitlines()
                       if l.split()[3].startswith(("192.168.", "10."))})
    for pre in prefixos:
        cand = ["%s.%d" % (pre, i) for i in range(1, 255)]
        with concurrent.futures.ThreadPoolExecutor(64) as ex:
            for c, ok in zip(cand, ex.map(lambda x: porta(x, XBDM), cand)):
                if ok:
                    return c
    raise SystemExit("ERRO: nenhum console com o xbdm (porta 730) na rede. Ele está ligado, no cabo, na dashboard?")


def vigia(ip, limite_min):
    t0 = time.time()
    while time.time() - t0 < 30:  # o magicboot leva uns segundos para trocar de título
        try:
            if not na_dash(ip):
                break
        except (OSError, RuntimeError):
            pass
        time.sleep(2)
    while time.time() - t0 < limite_min * 60:
        time.sleep(5)
        try:
            if na_dash(ip):
                avisa("vigia: voltou à dashboard em %.0f s" % (time.time() - t0))
                time.sleep(5)  # a FreeStyle sobe o FTP depois de aparecer
                return True
        except (OSError, RuntimeError):
            pass
    avisa("vigia: LIMITE de %.0f min; o app não voltou (o vigia não mata nada: decida à mão)" % limite_min)
    return False


# ---- ftp -----------------------------------------------------------------------------------------------------------
def ftp(ip, pasta):
    for tentativa in range(6):
        try:
            f = ftplib.FTP(ip, timeout=20)
            f.login("xbox", "xbox")
            f.sock.settimeout(120)
            f.cwd(pasta)
            return f
        except (OSError, ftplib.Error) as e:
            avisa("ftp: %s; de novo em 5 s" % e)
            time.sleep(5)
    raise SystemExit("ERRO: o FTP da FreeStyle não respondeu (pare e pergunte: alguém pode estar usando o console)")


def baixa(f, nome):
    b = io.BytesIO()
    f.retrbinary("RETR " + nome, b.write, blocksize=1 << 20)
    return b.getvalue()


def sobe(f, dados, nome):
    tmp = "%s.%d.tmp" % (nome[:20], os.getpid() % 100000)
    f.storbinary("STOR " + tmp, io.BytesIO(dados), blocksize=1 << 20)
    try:
        f.sendcmd("DELE " + nome)
    except ftplib.error_perm:
        pass
    f.sendcmd("RNFR " + tmp)
    f.sendcmd("RNTO " + nome)
    if hashlib.md5(baixa(f, nome)).hexdigest() != hashlib.md5(dados).hexdigest():
        raise SystemExit("ERRO: %s chegou diferente no console" % nome)
    avisa("ftp: ok %s (%d bytes, md5 %s)" % (nome, len(dados), hashlib.md5(dados).hexdigest()))


def dispositivo(pasta):
    m = re.match(r"^/?Hdd1/(.*)$", pasta.strip("/") and "/" + pasta.strip("/"), re.I)
    if not m:
        raise SystemExit("ERRO: só sei traduzir pasta em /Hdd1 para caminho de dispositivo (%s)" % pasta)
    return "\\Device\\Harddisk0\\Partition1\\" + m.group(1).replace("/", "\\")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("roteiro")
    p.add_argument("--saida", default=None)
    p.add_argument("--sem-subir", action="store_true", help="não sobe o .xex (usa o que já está no console)")
    p.add_argument("--limite", type=float, default=8.0, help="minutos até o vigia desistir")
    p.add_argument("--ip")
    a = p.parse_args()

    texto = open(a.roteiro, "rb").read()
    fotos = re.findall(r"^\s*foto\s+([A-Za-z0-9_-]+)", texto.decode("utf-8"), re.M)
    if not re.search(r"^\s*sai\s*(#.*)?$", texto.decode("utf-8"), re.M):
        raise SystemExit("RECUSADO: o roteiro não termina com 'sai'; o vigia ficaria esperando o limite")
    pasta = conf("CUI_CONSOLE_PASTA")
    if not pasta:
        raise SystemExit("ERRO: sem CUI_CONSOLE_PASTA em ~/.config/collectionui/console.env")
    saida = a.saida or os.path.join(AQUI, "..", "local", "fotos", time.strftime("%Y%m%d-%H%M%S"))
    os.makedirs(saida, exist_ok=True)

    trava = pega_trava()  # noqa: F841 (segura até o fim do processo)
    ip = achar(a.ip)
    atual = rodando(ip)
    if not na_dash(ip):
        raise SystemExit("RECUSADO: o console não está na dashboard (rodando %s)" % atual)
    avisa("console %s na dashboard" % ip)

    f = ftp(ip, pasta)
    try:
        antes = baixa(f, "colecoes.txt")
    except ftplib.error_perm:
        antes = None
    if antes is not None:
        open(os.path.join(saida, "colecoes-antes.txt"), "wb").write(antes)
    if not a.sem_subir:
        sobe(f, open(XEX_LOCAL, "rb").read(), "collectionui.xex")
    sobe(f, texto, "roteiro.txt")
    f.close()  # nunca QUIT

    xex = dispositivo(pasta) + "\\collectionui.xex"
    codigo, linhas = xbdm(ip, 'magicboot title="%s" directory="%s"' % (xex, xex.rsplit("\\", 1)[0]))
    if codigo != 200:
        raise SystemExit("ERRO: magicboot respondeu %d %s" % (codigo, linhas))
    avisa("xbdm: app aberto; roteiro com %d fotos" % len(fotos))
    voltou = vigia(ip, a.limite)

    if not voltou:
        return 2
    f = ftp(ip, pasta)
    log = baixa(f, "collectionui.log")
    open(os.path.join(saida, "collectionui.log"), "wb").write(log)
    if antes is not None:
        depois = baixa(f, "colecoes.txt")
        avisa("colecoes.txt %s" % ("igual ao de antes" if depois == antes else "MUDOU (cópia de antes em colecoes-antes.txt)"))
    f.cwd("fotos")
    baixadas = []
    for nome in fotos:
        try:
            dados = baixa(f, nome + ".bmp")
        except ftplib.error_perm:
            avisa("falta a foto %s" % nome)
            continue
        caminho = os.path.join(saida, nome + ".bmp")
        open(caminho, "wb").write(dados)
        baixadas.append(caminho)
    f.close()

    try:
        from PIL import Image
        for b in baixadas:
            Image.open(b).save(b[:-4] + ".png")
            os.remove(b)
    except ImportError:
        avisa("sem o Pillow: as fotos ficam em .bmp")

    falhas = [l for l in log.decode("utf-8", "replace").splitlines() if "FALHA" in l or "ERRO" in l]
    for l in falhas:
        avisa("log: " + l)
    avisa("fotos: %d de %d em %s" % (len(baixadas), len(fotos), os.path.abspath(saida)))
    return 0 if len(baixadas) == len(fotos) and not falhas else 1


if __name__ == "__main__":
    sys.exit(main())
