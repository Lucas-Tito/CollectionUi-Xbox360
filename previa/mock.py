"""Itens FALSOS para a prévia ter as mesmas abas e contagens que o console.

A biblioteca da prévia é a extração antiga (120 itens) e o console tem bem mais: as
coleções do console citam TitleIds que a prévia não conhece, e sem esses itens a barra
de cima perde Homebrew e Emulador (a aba só existe quando alguma coleção tem item dela)
e as contagens saem menores. Igualar os dados de verdade não é objetivo; isto só põe
no lugar de cada TitleId que falta um item plausível:

  - no catálogo do xbox-vault (x360db.json): nome e capa do vault, XBLA se o vault diz
    que é, senão Xbox 360;
  - TitleId com as duas letras de editora (41 43 = "AC", 45 41 = "EA"...) fora do
    vault: Xbox clássico, com nome genérico e sem capa;
  - o resto tem cara de id sintético (o hash do caminho que o app dá a homebrew e ROM):
    metade vira homebrew e metade ROM, pela paridade do id. É invenção, e o nome diz.

A categoria sai do caminho, como a prévia já faz para os itens de verdade.
"""
import base64, io, json, os

VAULT = os.path.expanduser('~/Documentos/xbx')


def _capa(slug):
    caminho = os.path.join(VAULT, 'images', slug + '.webp')
    if not os.path.exists(caminho):
        return None
    from PIL import Image
    im = Image.open(caminho).convert('RGB').resize((260, 364))   # 5:7, o recorte da prévia
    b = io.BytesIO()
    im.save(b, 'WEBP', quality=80)
    return 'data:image/webp;base64,' + base64.b64encode(b.getvalue()).decode()


def completar(caminho_colecoes, jogos, presentes=None, primeiro_id=20000):
    """presentes: os TitleIds que estão no console (do vault.txt), ou None. O console
    conta só o que está instalado, e o colecoes.txt guarda também TitleIds de jogos que
    saíram dele (numa coleção de 127, o console mostrava 96). Sem o vault.txt não há
    como saber quais, e as contagens saem maiores."""
    conhecidos = {j['tid'].upper() for j in jogos if j.get('tid')}
    faltam = []
    for linha in open(caminho_colecoes, encoding='utf-8', errors='replace'):
        p = linha.rstrip('\r\n').split('|', 3)
        if len(p) == 4 and p[1] == 'jogos':
            for t in p[3].split(','):
                t = t.strip().upper()
                if t and t not in conhecidos and t not in faltam and (presentes is None or t in presentes):
                    faltam.append(t)

    dados = os.path.join(VAULT, 'data')
    try:
        x360db = json.load(open(os.path.join(dados, 'x360db.json'), encoding='utf-8'))
        x360 = {e['id']: e for e in json.load(open(os.path.join(dados, 'x360.json'), encoding='utf-8'))}
    except OSError:
        x360db, x360 = {}, {}
    por_tid = {v['titleId'].upper(): k for k, v in x360db.items() if v.get('titleId')}

    novos, n = [], primeiro_id
    for t in faltam:
        j = {'id': n, 'tid': t}
        n += 1
        slug = por_tid.get(t)
        editora = all(0x41 <= int(t[i:i + 2], 16) <= 0x5A for i in (0, 2))
        if slug:
            e = x360.get(slug, {})
            j['nome'] = e.get('title') or slug.split('-', 1)[1].replace('-', ' ').title()
            raiz = 'XBLA' if e.get('flags', {}).get('xbla') else 'JOGOS'
            j['caminho'] = '\\%s\\%s\\default.xex' % (raiz, j['nome'])
            cap = _capa(slug)
            if cap:
                j['cap'] = cap
        elif editora:
            j['nome'] = 'Xbox clássico %s' % t
            j['caminho'] = '\\ORIGINAL XBOX\\%s\\default.xbe' % t
        elif int(t, 16) % 2:
            j['nome'] = 'Homebrew %s' % t
            j['caminho'] = '\\HOMEBREW\\%s\\default.xex' % t
        else:
            j['nome'] = 'ROM %s' % t
            j['caminho'] = '\\EMULADORES\\%s.smc' % t
            j['rom'] = 1
        novos.append(j)
    return novos
