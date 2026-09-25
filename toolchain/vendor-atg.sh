#!/usr/bin/env bash
# Copia a ATG (Advanced Technology Group) do XDK para app/vendor/atg/.
#
# A ATG é o conjunto de classes de amostra que a Microsoft distribui com o SDK:
# renderizador de fonte com atlas, desenho de retângulo em espaço de tela e os
# shaders simples. É o que poupa escrever um renderizador de fonte do zero.
#
# Não entra no git: é código proprietário de amostra, mesmo estatuto do resto do
# XDK. O repositório guarda esta receita, não o conteúdo.
set -euo pipefail

ORIGEM=${1:-sdk/XDK/Source/Samples/Common}
DESTINO=${2:-app/vendor/atg}

[ -d "$ORIGEM" ] || { echo "não achei $ORIGEM — extraia o XDK primeiro (veja toolchain/README.md)"; exit 1; }

mkdir -p "$DESTINO"

# O conjunto mínimo. AtgBound e AtgCollision entram porque AtgDebugDraw depende
# deles para o DrawBound 3D, que não usamos mas é referenciado no mesmo .cpp.
ARQUIVOS=(
    AtgFont AtgResource AtgSimpleShaders AtgDebugDraw
    AtgUtil AtgDevice AtgBound AtgCollision
)

# Todos os headers, porque eles se incluem entre si (AtgFont.cpp puxa AtgApp.h) e
# caçar um a um é perda de tempo — são poucos KB. Dos .cpp, só os que usamos.
cp -f "$ORIGEM"/*.h "$DESTINO/"

for a in "${ARQUIVOS[@]}"; do
    cp -f "$ORIGEM/$a.cpp" "$DESTINO/"
done

# Os arquivos do SDK são somente-leitura e o cp preserva isso, o que faz a próxima
# execução do script falhar. Devolvemos a permissão de escrita.
chmod -R u+w "$DESTINO"

echo "ATG copiada: $(ls "$DESTINO" | wc -l) arquivos em $DESTINO"

# A fonte. O Arial_16 já vem empacotado (.xpr) no SDK e traz os 16 glifos de botão
# do controle — é ele que desenha o rodapé. Também fica fora do git.
MEDIA=${3:-app/media}
mkdir -p "$MEDIA"
FONTE=$(find sdk/XDK -name 'Arial_16.xpr' 2>/dev/null | head -1)
if [ -n "$FONTE" ]; then
    cp "$FONTE" "$MEDIA/"
    echo "fonte copiada: $MEDIA/Arial_16.xpr"
else
    echo "aviso: não achei Arial_16.xpr no SDK"
fi

# O efeito do SimpleShaders. ATENÇÃO: passar NULL para Initialize() NÃO quer dizer
# "sem arquivo" — quer dizer "use game:\media\effects\simpleshaders.fxobj". Sem ele,
# FXLCreateEffect recebe ponteiro nulo e o console dá fatal crash.
mkdir -p "$MEDIA/effects"
EFEITO=$(find sdk/XDK -iname 'SimpleShaders.fxobj' 2>/dev/null | head -1)
if [ -n "$EFEITO" ]; then
    cp -f "$EFEITO" "$MEDIA/effects/"
    echo "efeito copiado: $MEDIA/effects/$(basename "$EFEITO")"
else
    echo "aviso: não achei SimpleShaders.fxobj no SDK"
fi
