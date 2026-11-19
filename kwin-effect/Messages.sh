#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
set -eu

: "${podir:?Set podir to the directory for the translation template}"
podir=$(CDPATH='' cd -- "$podir" && pwd)
cd -- "$(dirname -- "$0")"
${XGETTEXT:-xgettext} --language=C++ --from-code=UTF-8 \
    --keyword=i18n:1 --keyword=i18nc:1c,2 \
    config.cpp -o "$podir/kwin_effect_oledpanel.pot"
