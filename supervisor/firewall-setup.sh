#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Fixed-purpose helper, installed root-owned for the capability-only service.
set -eu
PATH=/usr/sbin:/usr/bin:/sbin:/bin
export PATH
test "$#" -le 1 || exit 2
exec /usr/bin/python3 -I "$(dirname -- "$0")/firewall.py" "${1:-ensure}"
