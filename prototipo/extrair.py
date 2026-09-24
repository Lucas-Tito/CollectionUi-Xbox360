#!/usr/bin/env python3
"""Lê os dados do FreeStyle e produz o que o protótipo consome.

Entrada:  ~/Documentos/CollectionUI-dados  (cópia do console; ver LEIA-ME de lá)
Saída:    prototipo/build/games.json  e  prototipo/build/img/c<id>.webp

O trabalho de verdade está em ler o container FSDA e recortar a capa: o asset do tipo 128
não é a capa, é o encarte inteiro (contracapa + lombada + frente), e a frente são os 46,8%
da direita. Calibrado à mão contra a biblioteca real.

Enriquecimento (Metacritic, HowLongToBeat) é opcional: acontece se os .json do xbox-vault
estiverem em --vault. A interface atual não usa esses campos, mas eles saem de graça.
"""
import argparse, base64, io, json, os, re, sqlite3, struct, sys, unicodedata

CROP_FRENTE = 0.468          # fração da direita do encarte que é a capa
TAM_CAPA = (260, 364)        # 5:7, o que a grade do protótipo mostra
TIPO_CAPA = 128              # tipos do FSDA: 1 ícone, 2 fundo, 4 banner, 8 capa peq., 128 capa

def norm(s):
    s = unicodedata.normalize('NFD', (s or '').lower())
    s = ''.join(c for c in s if unicodedata.category(c) != 'Mn').replace('&', 'and')
    return re.sub(r'[^a-z0-9]', '', s)

def assets(caminho):
    """Devolve {tipo: PIL.Image} de um .assets. Formato FSDA, big-endian."""
    from PIL import Image
    d = open(caminho, 'rb').read()
    if d[:4] != b'FSDA':
        return {}
    n = struct.unpack_from('>I', d, 16)[0]
    out = {}
    for i in range(n):
        tipo, off, tam, _ = struct.unpack_from('>4I', d, 24 + i * 16)
        if off + 8 > len(d) or d[off:off+4] != b'DDS ':
            continue
        try:
            im = Image.open(io.BytesIO(d[off:off+tam])); im.load()
            out[tipo] = im.convert('RGB')
        except Exception:
            pass
    return out

def enriquecimento(vault):
    """{slug_por_titleid_hex, metacritic, hltb, slug_por_nome} ou None."""
    if not vault or not os.path.isdir(vault):
        return None
    def carrega(nome):
        p = os.path.join(vault, nome)
        return json.load(open(p, encoding='utf-8')) if os.path.exists(p) else {}
    x360db = carrega('x360db.json')
    porhex = {v['titleId'].upper(): k for k, v in x360db.items() if v.get('titleId')}
    pornome = {}
    for arq in ('x360.json', 'xbox.json'):
        for it in (carrega(arq) or []):
            if isinstance(it, dict) and it.get('title'):
                pornome.setdefault(norm(it['title']), it['id'])
    return porhex, pornome, carrega('metacritic.json'), carrega('hltb.json')

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dados', default=os.path.expanduser('~/Documentos/CollectionUI-dados'))
    ap.add_argument('--saida', default=os.path.join(os.path.dirname(os.path.abspath(__file__)), 'build'))
    ap.add_argument('--vault', default=None, help='pasta com os data/*.json do xbox-vault')
    a = ap.parse_args()

    from PIL import Image
    db = os.path.join(a.dados, 'Databases', 'content.db')
    gd = os.path.join(a.dados, 'GameData')
    if not os.path.exists(db):
        sys.exit('não achei %s — veja o LEIA-ME em %s' % (db, a.dados))

    img = os.path.join(a.saida, 'img')
    os.makedirs(img, exist_ok=True)
    enr = enriquecimento(a.vault)

    c = sqlite3.connect('file:%s?mode=ro' % db, uri=True); c.row_factory = sqlite3.Row
    jogos, ncap = [], 0
    for r in c.execute('select * from ContentItems order by ContentItemName'):
        cid = r['ContentItemId']; hexid = '%08X' % cid
        p = os.path.join(gd, hexid, hexid + '.assets')
        if not os.path.exists(p):
            continue
        g = {'id': cid,
             'nome': r['ContentItemName'] or '(sem nome)',
             'tid': '%08X' % (r['ContentItemTitleId'] & 0xFFFFFFFF),
             'genero': r['ContentItemGenre'], 'dev': r['ContentItemDeveloper'],
             'pub': r['ContentItemPublisher'], 'nota': r['ContentItemRating'],
             'raters': r['ContentItemRaters'], 'lanc': r['ContentItemReleaseDate'],
             'desc': (r['ContentItemDescription'] or '').strip(),
             'tipo': r['ContentItemFileType'], 'caminho': r['ContentItemPath'],
             'kinect': r['ContentItemKinectFlag'], 'discos': r['ContentItemDiscsInSet']}
        if enr:
            porhex, pornome, mc, hl = enr
            slug = porhex.get(g['tid']) or pornome.get(norm(g['nome']))
            m = mc.get(slug) if slug else None
            if isinstance(m, dict) and m.get('score'):
                g['mc'] = m['score']
            h = hl.get(slug) if slug else None
            if isinstance(h, dict) and h.get('main'):
                g.update(hltb=h['main'], hltbN=h.get('n'), hltbPlus=h.get('plus'), hltb100=h.get('cem'))
        im = assets(p)
        if TIPO_CAPA in im:
            w, hh = im[TIPO_CAPA].size
            frente = im[TIPO_CAPA].crop((int(w * (1 - CROP_FRENTE)), 0, w, hh))
            frente.resize(TAM_CAPA, Image.LANCZOS).save(
                os.path.join(img, 'c%d.webp' % cid), 'WEBP', quality=76, method=5)
            g['cap'] = 'img/c%d.webp' % cid
            ncap += 1
        jogos.append({k: v for k, v in g.items() if v not in (None, '')})
    c.close()

    json.dump(jogos, open(os.path.join(a.saida, 'games.json'), 'w', encoding='utf-8'), ensure_ascii=False)
    print('%d jogos, %d capas -> %s' % (len(jogos), ncap, a.saida))

if __name__ == '__main__':
    main()
