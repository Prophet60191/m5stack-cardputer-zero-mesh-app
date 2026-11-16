#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Root-owned, fixed-purpose nftables helper. No caller-selected port/command."""
import json
import os
import shutil
import subprocess
import sys

PORT = 47400
TABLE = "cz_mesh"


def protected(document: dict) -> bool:
    entries = document.get("nftables", [])
    chain_ok = any(e.get("chain", {}).get("family") == "inet"
                   and e["chain"].get("table") == TABLE
                   and e["chain"].get("name") == "input"
                   and e["chain"].get("type") == "filter"
                   and e["chain"].get("hook") == "input"
                   and e["chain"].get("prio") == -1 for e in entries)
    rules = [e["rule"] for e in entries if "rule" in e]
    expected = [
        {"match": {"op": "!=", "left": {"meta": {"key": "iifname"}}, "right": "lo"}},
        {"match": {"op": "==", "left": {"payload": {"protocol": "tcp", "field": "dport"}},
                   "right": PORT}},
        {"drop": None},
    ]
    # No earlier accept/jump rules may bypass our drop.
    return chain_ok and len(rules) == 1 and rules[0].get("family") == "inet" \
        and rules[0].get("table") == TABLE and rules[0].get("chain") == "input" \
        and rules[0].get("expr") == expected


def main() -> int:
    os.environ["PATH"] = "/usr/sbin:/usr/bin:/sbin:/bin"
    if len(sys.argv) != 2 or sys.argv[1] not in ("ensure", "check", "remove"):
        raise SystemExit("usage: firewall-setup.sh {ensure|check|remove}")
    nft = shutil.which("nft")
    if not nft:
        raise SystemExit("nftables is required; radio must remain off")
    result = subprocess.run([nft, "-j", "list", "table", "inet", TABLE],
                            capture_output=True, text=True, timeout=5)
    if sys.argv[1] == "remove":
        if result.returncode == 0:
            subprocess.run([nft, "delete", "table", "inet", TABLE], check=True, timeout=5)
        return 0
    if result.returncode == 0 and protected(json.loads(result.stdout)):
        return 0
    if sys.argv[1] == "check":
        raise SystemExit("radio API firewall is absent or changed; keep radio off")
    # Replace only our table, atomically; never flush the machine's ruleset.
    prefix = f"delete table inet {TABLE}\n" if result.returncode == 0 else ""
    rules = prefix + f'''table inet {TABLE} {{
 chain input {{
  type filter hook input priority -1; policy accept;
  iifname != "lo" tcp dport {PORT} drop
 }}
}}
'''
    subprocess.run([nft, "-f", "-"], input=rules, text=True, check=True, timeout=5)
    checked = subprocess.run([nft, "-j", "list", "table", "inet", TABLE],
                             capture_output=True, text=True, check=True, timeout=5)
    if not protected(json.loads(checked.stdout)):
        raise SystemExit("firewall verification failed; keep radio off")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
