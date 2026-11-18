"""A ficha de cada jogo, com o que foi escolhido em 18/11 e mais nada:

  - o co-op, SÓ DO CO-OPTIMUS E SÓ LOCAL (o online não interessa): quantos jogadores no
    mesmo console, o texto dele e o que é de sofá (tela dividida, entrar e sair, campanha
    em co-op). Jogo que o Co-Optimus não lista, ou lista só com co-op online, fica sem
    nada: não se afirma que não tem, e o número de jogadores sem co-op não diz nada
    (pedido de 18/11; as tags da Wikipédia não entram);
  - a nota do Metacritic e, quando não houver, a dos jogadores (0 a 5);
  - a descrição;
  - o tempo de jogo (HowLongToBeat: história, com extras, 100%);
  - ano, gênero, desenvolvedora e publicadora.

Fonte: o xbox-vault (~/Documentos/xbx/data), pelo TitleId; o que ele não tiver cai no que
a FreeStyle já guarda (o games.json da prévia traz descrição, gênero, dev, pub, nota e data).

O texto passa por um filtro de pontuação: o atlas da fonte não tem ’ “ ” – — … nem ® ™, que
sairiam como quadradinho;
e tira o U+FFFD, o ® que já chega estragado da FreeStyle. O gerador do fichas.txt do console vai precisar do mesmo filtro.
"""
import json, os

VAULT = os.path.expanduser('~/Documentos/xbx/data')

TROCAS = {'’': "'", '‘': "'", '“': '"', '”': '"', '–': '-', '—': '-',
          '…': '...', '®': '', '™': '', ' ': ' ', '•': '-',
          '\ufffd': ''}   # o ® que já chega estragado da FreeStyle


def limpa(t):
    if not t or t.strip().lower() in ('not available', 'n/a', 'unknown'):   # o vazio da FreeStyle
        return None
    for a, b in TROCAS.items():
        t = t.replace(a, b)
    return ' '.join(t.split())


def _carrega(nome):
    try:
        with open(os.path.join(VAULT, nome), encoding='utf-8') as f:
            return json.load(f)
    except OSError:
        return {}


def fichas(jogos):
    """Devolve {id do item: ficha} para os jogos que têm algo a mostrar."""
    x360db = _carrega('x360db.json')
    x360 = {e['id']: e for e in (_carrega('x360.json') or [])}
    coop = _carrega('coop.json')
    meta, hltb, desc = _carrega('metacritic.json'), _carrega('hltb.json'), _carrega('descricoes.json')
    por_tid = {v['titleId'].upper(): k for k, v in x360db.items() if v.get('titleId')}

    saida = {}
    for j in jogos:
        slug = por_tid.get((j.get('tid') or '').upper())
        e, db = x360.get(slug, {}), x360db.get(slug, {})
        f = {}

        # co-op no sofá, só do Co-Optimus
        c = coop.get(slug) or {}
        if c.get('local'):
            f['coop'] = c['local']
            if limpa(c.get('exp')): f['coopTexto'] = limpa(c['exp'])
            extras = {'Splitscreen': 'tela dividida', 'Drop In/Drop Out': 'entrar e sair a qualquer hora',
                      'Co-Op Campaign': 'campanha em co-op', 'Co-Op Specific Content': 'conteúdo só do co-op'}
            ex = [extras[x] for x in c.get('extras', []) if x in extras]
            if ex: f['coopExtras'] = ex

        # nota: Metacritic, ou a dos jogadores
        m = (meta.get(slug) or {}).get('score')
        if m: f['metacritic'] = m
        else:
            ur = db.get('ur')
            if not ur and j.get('nota'):
                try: ur = float(j['nota'])
                except ValueError: ur = None
            if ur: f['jogadores_nota'] = round(float(ur), 1)

        # descrição e tempo
        d = desc.get(slug) or j.get('desc')
        if limpa(d): f['descricao'] = limpa(d)
        h = hltb.get(slug) or {}
        tempo = {k: h[k] for k in ('main', 'plus', 'cem') if h.get(k)}
        if tempo: f['tempo'] = tempo

        # ficha técnica
        ano = e.get('year')
        if not ano and j.get('lanc') and str(j['lanc'])[:4].isdigit(): ano = int(str(j['lanc'])[:4])
        if ano: f['ano'] = ano
        gen = e.get('genre') or j.get('genero')
        if limpa(gen): f['genero'] = limpa(gen)
        dev = ', '.join(e.get('developers') or []) or db.get('dev') or j.get('dev')
        pub = ', '.join(e.get('publishers') or []) or db.get('pub') or j.get('pub')
        if limpa(dev): f['dev'] = limpa(dev)
        if limpa(pub): f['pub'] = limpa(pub)

        if f:
            saida[j['id']] = f
    return saida
