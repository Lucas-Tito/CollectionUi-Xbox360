#!/usr/bin/env bash
# Compila dentro do container do XDK e devolve a posse dos arquivos ao usuário.
#
# O wine recusa um prefixo que não seja do dono do processo, então o container roda
# como root (é dele o prefixo criado na imagem) e a posse é corrigida no fim. Rodar
# com --user faria o wine abortar com "'/wine-prefix' is not owned by you".
set -euo pipefail

IMAGEM=${IMAGEM:-collectionui-xdk:light}
PROJETO=${1:-.}
shift || true

cd "$PROJETO"
docker run --rm -v "$PWD":/app -w /app "$IMAGEM" make "$@"
docker run --rm -v "$PWD":/app "$IMAGEM" chown -R "$(id -u):$(id -g)" /app
