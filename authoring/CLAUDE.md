# authoring

How Claude Code (or a human) talks to a running Lua-scripted AbacDsp plugin instance to
draft, audition, and iterate on a script - directly over HTTP, or through the CLI in this
folder. This supersedes the old `llm-genscripts/` watched-folder workflow: the plugin no
longer watches any folder, and there is no more polling for a `state-*.json` sibling file
- every call gets its result back synchronously in the HTTP response.

Full Lua scripting API (MIDI handlers, `OnStart`/`OnStop`, dynamic UI parameters, the
`Music`/`Vel`/`Rr`/`Rhythm` helper library, `Timer`, `Transport`, sandboxed stdlib) is in
root `LUA.md`; each example's own extra hooks are documented in its own README.md under
"Scripting". This file only covers the HTTP protocol and CLI.

## Precondition

The plugin instance must be running with Authoring Mode explicitly enabled (Scripts >
Authoring Mode > Enable, in the plugin's own menu - it defaults to off every session and
is never auto-started). Enabling it opens a status dashboard in your default browser and
prints the port to the plugin's status bar.

## Finding a running instance

Every enabled instance writes a discovery file to a shared, per-machine directory:
`~/Library/Application Support/AbacDsp/AuthoringInstances/<pid>-<port>.json` on macOS
(`%APPDATA%\AbacDsp\AuthoringInstances\` on Windows, `$XDG_CONFIG_HOME/AbacDsp/AuthoringInstances/`,
default `~/.config/...`, on Linux), containing `module`, `pid`, `port`, `token`, and
`baseUrl`. A stale file whose pid is no longer running is ignored.

```
python3 authoring/cli.py list
dronesequencer    pid=4242    port=54321    http://127.0.0.1:54321
```

## Calling the API directly

Every request needs the instance's token as `Authorization: Bearer <token>` (GET requests
may instead pass `?token=...`). All bodies/responses are JSON.

```
curl -H "Authorization: Bearer <token>" http://127.0.0.1:<port>/context
curl -X POST -H "Authorization: Bearer <token>" -H "Content-Type: application/json" \
     -d '{"text": "function OnNoteOn(channel, note, velocity) end"}' \
     http://127.0.0.1:<port>/script
```

| Method & path | Purpose |
|---|---|
| `GET /` | Human dashboard (browser, via `Accept: text/html`) or the same payload as `GET /status` otherwise |
| `GET /status` | module/pid/port/wrapperType + current script/patch name + error state + CPU/pool diagnostics, one call |
| `GET /instance` | module/pid/port/wrapperType only |
| `GET /context` | current script text + name, current patch name, declared UI-param slots, installed library names, current script error |
| `GET /diagnostics` | script error state, CPU load, Lua pool bytes in use |
| `POST /script` | body `{"text": "..."}`; response `{"compiled": bool, "error": "..."}` - applies immediately; a failing script leaves whatever was running before untouched |
| `GET /patches` | `{"patches": [...]}` - saved patch names, including any `Factory/<name>` bundled with the plugin |
| `GET /patches/{name}` | raw JSON of one saved patch (every knob value plus the embedded script); 404 if the name doesn't exist |
| `POST /patches/{name}` | saves the current live parameters + script under this name; `{"saved": bool}` |
| `POST /patches/{name}/load` | loads and applies a saved patch directly - no "save unsaved changes?" prompt, discards them; `{"loaded": bool}` |
| `DELETE /patches/{name}` | `{"deleted": bool}` |
| `GET /libraries` | `{"libraries": [...]}` - installed library names (User + Base combined) |
| `GET /libraries/{name}` | `{"content": "..."}` - a library's Lua source; 404 if it doesn't exist |
| `POST /libraries/{name}` | body `{"content": "..."}`; saves to `Library/User/`, then re-applies the current script so any `import "{name}"` is actually re-resolved and validated - response `{"compiled": bool, "error": "..."}` describes *that re-apply*, not the library file in isolation (a library the current script doesn't import returns `compiled: true` even if the library itself is broken) |
| `POST /midi` | body `{"type": "noteOn"\|"noteOff"\|"cc"\|"programChange"\|"pitchBend"\|"aftertouch"\|"polyPressure", "channel": 0-15, ...}` - injects a synthetic MIDI event on the next audio block, merged with any real incoming MIDI; `channel` is 0-based, matching `LUA.md`'s `OnNoteOn`/etc. Type-specific fields: `note`+`velocity` (noteOn/noteOff), `controller`+`value` (cc), `program` (programChange), `value` (pitchBend/aftertouch), `note`+`value` (polyPressure) |

## Using the CLI instead

```
python3 authoring/cli.py list
python3 authoring/cli.py status <instance>
python3 authoring/cli.py context <instance>
python3 authoring/cli.py diagnostics <instance>
python3 authoring/cli.py apply-script <instance> <script.lua>
python3 authoring/cli.py list-patches <instance>
python3 authoring/cli.py get-patch <instance> <name>
python3 authoring/cli.py save-patch <instance> <name>
python3 authoring/cli.py load-patch <instance> <name>
python3 authoring/cli.py delete-patch <instance> <name>
python3 authoring/cli.py export-patch <instance> <name> [--into DIR]
python3 authoring/cli.py list-libraries <instance>
python3 authoring/cli.py get-library <instance> <name>
python3 authoring/cli.py apply-library <instance> <name> <library.lua>
python3 authoring/cli.py midi <instance> note-on --note 60 --velocity 100
```

`<instance>` is a pid, a port, or a module name (only if it names exactly one running
instance - `list` first if unsure). Stdlib-only, nothing to install.

## Workflow

1. `list` to find the instance (or ask the user to enable Authoring Mode if none show up).
2. `context` to see the current script, patch, declared UI params, and available libraries
   before writing anything - don't guess at what's already there.
3. Write the script, then `apply-script <instance> <file>`. Read `compiled`/`error` in the
   result directly - no waiting, no polling.
4. On a compile error, fix the script and re-run `apply-script` with the same file; there
   is no "pulled" file to avoid re-using like the old folder workflow, edit in place freely.
5. To actually hear it: `midi <instance> note-on --note 60 --velocity 100` injects a
   synthetic note without needing a real controller or host-forwarded MIDI.
6. Once happy, `save-patch <instance> <name>` to save the current parameters + script as a
   named patch, then `export-patch <instance> <name>` to write its JSON into that example's
   `factory-patches/` folder. Commit that file - it ships with the plugin and is synced
   back in as `Factory/<name>` in every user's patch list on every launch.

## Watched-folder adapter (optional)

For a drop-a-file-and-go workflow instead of calling `apply-script` directly:

```
python3 authoring/watch_adapter.py <instance> <folder>
```

Polls `<folder>` for the newest `*.lua` file not already prefixed `pulled-`, applies it via
`POST /script`, writes `<folder>/state-<name>.json` with the result, and renames the
source to `pulled-<name>-<epoch-ms>.lua` - the same on-disk protocol
`llm-genscripts/CLAUDE.md` used to document, except the plugin itself never touches this
folder: this script is a plain HTTP client polling it on your behalf.
