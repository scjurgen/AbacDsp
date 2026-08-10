# llm-genscripts

Exchange point between a Claude Code session and a running DroneSequencer instance's
"LLM-Assist" watched-folder mode: an alternative to the in-app script editor where an LLM
(or any external tool) drives the sequencer's Lua script through the filesystem instead.

- `CLAUDE.md` - protocol Claude follows when generating/updating a script here.
- `generated/` - the actual watched folder. Point DroneSequencer's Scripts > LLM-Assist >
  Choose Folder at this subfolder, not at `llm-genscripts/` itself. Holds the `.lua`
  scripts written for the app to pick up, the `pulled-*.lua` files it renames them to once
  applied, and the `state-*.json` compile-result reports it writes back. Contents are
  transient/generated; safe to clear out periodically.

The Lua scripting API itself (hooks, note-table fields, sandboxed stdlib, worked examples)
is documented in root `LUA.md` and `examples/dronesequencer/README.md` under "Scripting",
not duplicated here.
