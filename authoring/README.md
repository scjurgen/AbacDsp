# authoring

Python client and CLI for AbacDsp's Authoring HTTP API - the localhost-only interface a
running Lua-scripted plugin instance exposes once Authoring Mode is enabled, for
discovering an instance, reading its current script/patch context, and submitting a new
script to audition. See `CLAUDE.md` in this folder for the full protocol and workflow, and
root `LUA-MANUAL.md` for the Lua scripting API itself.

Stdlib-only - no `pip install` needed, matching `JuceStandaloneGenerator`'s own Python
tooling in this repo. Requires Python 3.10+.

```
python3 authoring/cli.py list
python3 authoring/cli.py status <instance>
python3 authoring/cli.py context <instance>
python3 authoring/cli.py apply-script <instance> path/to/script.lua
python3 authoring/cli.py save-patch <instance> <name>
python3 authoring/cli.py export-patch <instance> <name>
```

`client.py` is the reusable piece (instance discovery + the HTTP calls themselves) if
you're scripting against this from something other than the CLI. `watch_adapter.py` is an
optional drop-folder adapter for anyone who prefers dropping a file over calling
`apply-script` directly - it is a plain HTTP client, not something the plugin watches.
