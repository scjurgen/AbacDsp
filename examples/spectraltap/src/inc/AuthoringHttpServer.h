#pragma once

#include <cstddef>
#include <functional>
#include <httplib.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if JUCE_WINDOWS
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "AuthoringHttpServerCore.h"
#include "LuaScriptEngineBase.h"

[[nodiscard]] inline int currentProcessId()
{
#if JUCE_WINDOWS
    return static_cast<int>(::GetCurrentProcessId());
#else
    return static_cast<int>(::getpid());
#endif
}

/**
 * Localhost-only authoring API for a running Lua-scripted plugin instance - the
 * replacement for the old folder-watchdog LLM-Assist workflow. Binds 127.0.0.1 at an
 * OS-assigned ephemeral port only once start() is called (Authoring Mode explicitly
 * enabled), writes a discovery file so a CLI/Claude Code session can find this instance,
 * and serves both a JSON API and (content-negotiated on Accept) a human status dashboard
 * at the same root URL. Every request requires the per-instance bearer token, regenerated
 * on every start(). Callback surface mirrors the old LlmAssistWatcher's, plus context/
 * diagnostics reads; every callback is invoked via juce::MessageManager::callSync from
 * the server's own background thread, since script/patch state is otherwise only ever
 * touched from the message thread.
 */
class AuthoringHttpServer
{
  public:
    std::function<bool(const juce::String&)> applyScriptText;
    std::function<juce::String()> scriptErrorMessage;
    std::function<bool()> hasScriptError;
    std::function<juce::String()> currentScriptText;
    std::function<juce::String()> currentScriptName;
    std::function<juce::String()> currentPatchName;
    std::function<std::vector<juce::String>()> libraryScriptNames;
    std::function<std::vector<LuaUiParamSlot>()> uiParamSlots;
    std::function<float()> cpuLoadPercent;
    std::function<size_t()> poolBytesInUse;
    std::function<juce::String()> wrapperTypeDescription;
    std::function<std::vector<juce::String>()> patchNames;
    std::function<std::optional<juce::String>(const juce::String&)> patchJson;
    std::function<bool(const juce::String&)> savePatchNamed;
    std::function<bool(const juce::String&)> loadPatchNamed;
    std::function<bool(const juce::String&)> deletePatchNamed;
    std::function<std::optional<juce::String>(const juce::String&)> libraryScriptSource;
    std::function<std::pair<bool, juce::String>(const juce::String&, const juce::String&)> applyLibraryScript;
    // Called straight from this server's own worker thread, not marshaled through
    // callSync - juce::MidiMessageCollector::addMessageToQueue() is already thread-safe.
    std::function<void(const juce::MidiMessage&)> injectMidi;

    ~AuthoringHttpServer()
    {
        stop();
    }

    // Binds 127.0.0.1 at an OS-assigned ephemeral port and writes the discovery file;
    // false (nothing started) if the bind itself fails. Not real-time safe - call from
    // the message thread only, same as the old LlmAssistWatcher's folder toggle.
    bool start(const juce::String& moduleName)
    {
        if (isRunning())
        {
            return true;
        }
        m_moduleName = moduleName;
        m_token = generateAuthoringToken();
        registerRoutes();

        const int port = m_server.bind_to_any_port("127.0.0.1", 0);
        if (port <= 0)
        {
            return false;
        }
        m_boundPort = port;
        writeDiscoveryFile();
        m_serverThread = std::jthread([this](std::stop_token) { m_server.listen_after_bind(); });
        return true;
    }

    void stop()
    {
        if (!isRunning())
        {
            return;
        }
        m_server.stop();
        m_serverThread = std::jthread{};
        removeDiscoveryFile();
        m_boundPort = 0;
    }

    [[nodiscard]] bool isRunning() const noexcept
    {
        return m_boundPort > 0;
    }

    [[nodiscard]] int boundPort() const noexcept
    {
        return m_boundPort;
    }

    // Exposed for tests/CLI-style clients that need to build an Authorization header
    // directly rather than going through dashboardUrl()'s query parameter.
    [[nodiscard]] const std::string& token() const noexcept
    {
        return m_token;
    }

    // Root URL a browser or the CLI uses to reach this instance, token included - see
    // AuthoringHttpServerCore.h for why the token also travels as a query parameter.
    [[nodiscard]] juce::URL dashboardUrl() const
    {
        return juce::URL("http://127.0.0.1:" + juce::String(m_boundPort) + "/").withParameter("token", m_token);
    }

  private:
    struct ApplyResult
    {
        bool compiled{false};
        juce::String error;
    };

    [[nodiscard]] bool isAuthorized(const httplib::Request& req) const
    {
        static constexpr std::string_view kBearerPrefix = "Bearer ";
        const std::string authHeader = req.get_header_value("Authorization");
        if (authHeader.size() > kBearerPrefix.size() && authHeader.compare(0, kBearerPrefix.size(), kBearerPrefix) == 0)
        {
            if (authoringTokensMatch(m_token, authHeader.substr(kBearerPrefix.size())))
            {
                return true;
            }
        }
        if (req.method == "GET")
        {
            return authoringTokensMatch(m_token, req.get_param_value("token"));
        }
        return false;
    }

    // Runs fn on the message thread and returns its result, or a default-constructed
    // Result if the message couldn't be posted - the same callSync() pattern every other
    // handler below already uses, factored out for the patch endpoints' sake.
    template <typename Result, typename Fn>
    [[nodiscard]] Result onMessageThread(Fn&& fn) const
    {
        return juce::MessageManager::callSync(std::forward<Fn>(fn)).value_or(Result{});
    }

    [[nodiscard]] AuthoringStatusSnapshot currentStatusSnapshot() const
    {
        const auto result = juce::MessageManager::callSync(
            [this]() -> AuthoringStatusSnapshot
            {
                AuthoringStatusSnapshot s;
                s.module = m_moduleName.toStdString();
                s.pid = currentProcessId();
                s.port = m_boundPort;
                s.wrapperType = wrapperTypeDescription ? wrapperTypeDescription().toStdString() : std::string{};
                s.currentScriptName = currentScriptName ? currentScriptName().toStdString() : std::string{};
                s.currentPatchName = currentPatchName ? currentPatchName().toStdString() : std::string{};
                s.hasScriptError = hasScriptError && hasScriptError();
                s.scriptErrorMessage = scriptErrorMessage ? scriptErrorMessage().toStdString() : std::string{};
                s.cpuLoadPercent = cpuLoadPercent ? cpuLoadPercent() : 0.f;
                s.poolBytesInUse = poolBytesInUse ? poolBytesInUse() : 0;
                return s;
            });
        return result.value_or(AuthoringStatusSnapshot{});
    }

    void registerRoutes()
    {
        m_server.set_pre_routing_handler(
            [this](const httplib::Request& req, httplib::Response& res) -> httplib::Server::HandlerResponse
            {
                if (isAuthorized(req))
                {
                    return httplib::Server::HandlerResponse::Unhandled;
                }
                res.status = 401;
                res.set_content(R"({"error":"missing or invalid token"})", "application/json");
                return httplib::Server::HandlerResponse::Handled;
            });

        m_server.Get("/",
                     [this](const httplib::Request& req, httplib::Response& res)
                     {
                         const std::string accept = req.get_header_value("Accept");
                         if (accept.find("text/html") != std::string::npos)
                         {
                             res.set_content(std::string(kDashboardHtml), "text/html");
                         }
                         else
                         {
                             res.set_content(makeStatusJson(currentStatusSnapshot()), "application/json");
                         }
                     });

        m_server.Get("/status", [this](const httplib::Request&, httplib::Response& res)
                     { res.set_content(makeStatusJson(currentStatusSnapshot()), "application/json"); });

        m_server.Get("/instance",
                     [this](const httplib::Request&, httplib::Response& res)
                     {
                         const auto s = currentStatusSnapshot();
                         const nlohmann::json j{
                             {"module", s.module}, {"pid", s.pid}, {"port", s.port}, {"wrapperType", s.wrapperType}};
                         res.set_content(j.dump(), "application/json");
                     });

        m_server.Get("/diagnostics",
                     [this](const httplib::Request&, httplib::Response& res)
                     {
                         const auto s = currentStatusSnapshot();
                         const nlohmann::json j{
                             {"hasScriptError", s.hasScriptError},
                             {"scriptErrorMessage", s.scriptErrorMessage},
                             {"cpuLoadPercent", s.cpuLoadPercent},
                             {"poolBytesInUse", s.poolBytesInUse},
                         };
                         res.set_content(j.dump(), "application/json");
                     });

        m_server.Get("/context", [this](const httplib::Request&, httplib::Response& res)
                     { res.set_content(contextJson(), "application/json"); });

        m_server.Post("/script",
                      [this](const httplib::Request& req, httplib::Response& res)
                      {
                          const auto result = applyScriptFromRequestBody(req.body);
                          res.set_content(makeApplyScriptResultJson(result.compiled, result.error.toStdString()),
                                          "application/json");
                      });

        m_server.Get("/patches",
                     [this](const httplib::Request&, httplib::Response& res)
                     {
                         const auto body = onMessageThread<std::string>(
                             [this]
                             {
                                 nlohmann::json names = nlohmann::json::array();
                                 if (patchNames)
                                 {
                                     for (const auto& name : patchNames())
                                     {
                                         names.push_back(name.toStdString());
                                     }
                                 }
                                 return nlohmann::json{{"patches", names}}.dump();
                             });
                         res.set_content(body, "application/json");
                     });

        // A patch name is a single path segment here (no "/" subfolders, unlike the
        // in-app patch browser) - simplest thing that works for a first cut.
        m_server.Get(R"(/patches/([^/]+))",
                     [this](const httplib::Request& req, httplib::Response& res)
                     {
                         const juce::String name(req.matches[1].str());
                         const auto json = onMessageThread<std::optional<std::string>>(
                             [this, name]() -> std::optional<std::string>
                             {
                                 const auto result = patchJson ? patchJson(name) : std::nullopt;
                                 return result ? std::optional<std::string>(result->toStdString()) : std::nullopt;
                             });
                         if (!json)
                         {
                             res.status = 404;
                             res.set_content(R"({"error":"patch not found"})", "application/json");
                             return;
                         }
                         res.set_content(*json, "application/json");
                     });

        m_server.Post(R"(/patches/([^/]+)/load)",
                      [this](const httplib::Request& req, httplib::Response& res)
                      {
                          const juce::String name(req.matches[1].str());
                          const bool ok =
                              onMessageThread<bool>([this, name] { return loadPatchNamed && loadPatchNamed(name); });
                          res.status = ok ? 200 : 404;
                          res.set_content(nlohmann::json{{"loaded", ok}}.dump(), "application/json");
                      });

        m_server.Post(R"(/patches/([^/]+))",
                      [this](const httplib::Request& req, httplib::Response& res)
                      {
                          const juce::String name(req.matches[1].str());
                          const bool ok =
                              onMessageThread<bool>([this, name] { return savePatchNamed && savePatchNamed(name); });
                          res.status = ok ? 200 : 400;
                          res.set_content(nlohmann::json{{"saved", ok}}.dump(), "application/json");
                      });

        m_server.Delete(R"(/patches/([^/]+))",
                        [this](const httplib::Request& req, httplib::Response& res)
                        {
                            const juce::String name(req.matches[1].str());
                            const bool ok = onMessageThread<bool>(
                                [this, name] { return deletePatchNamed && deletePatchNamed(name); });
                            res.status = ok ? 200 : 404;
                            res.set_content(nlohmann::json{{"deleted", ok}}.dump(), "application/json");
                        });

        m_server.Get("/libraries",
                     [this](const httplib::Request&, httplib::Response& res)
                     {
                         const auto body = onMessageThread<std::string>(
                             [this]
                             {
                                 nlohmann::json names = nlohmann::json::array();
                                 if (libraryScriptNames)
                                 {
                                     for (const auto& name : libraryScriptNames())
                                     {
                                         names.push_back(name.toStdString());
                                     }
                                 }
                                 return nlohmann::json{{"libraries", names}}.dump();
                             });
                         res.set_content(body, "application/json");
                     });

        m_server.Get(R"(/libraries/([^/]+))",
                     [this](const httplib::Request& req, httplib::Response& res)
                     {
                         const juce::String name(req.matches[1].str());
                         const auto source = onMessageThread<std::optional<std::string>>(
                             [this, name]() -> std::optional<std::string>
                             {
                                 const auto result = libraryScriptSource ? libraryScriptSource(name) : std::nullopt;
                                 return result ? std::optional<std::string>(result->toStdString()) : std::nullopt;
                             });
                         if (!source)
                         {
                             res.status = 404;
                             res.set_content(R"({"error":"library not found"})", "application/json");
                             return;
                         }
                         res.set_content(nlohmann::json{{"content", *source}}.dump(), "application/json");
                     });

        m_server.Post(R"(/libraries/([^/]+))",
                      [this](const httplib::Request& req, httplib::Response& res)
                      {
                          const juce::String name(req.matches[1].str());
                          const auto result = applyLibraryFromRequestBody(name, req.body);
                          res.set_content(makeApplyScriptResultJson(result.compiled, result.error.toStdString()),
                                          "application/json");
                      });

        m_server.Post("/midi",
                      [this](const httplib::Request& req, httplib::Response& res)
                      {
                          const auto message = parseMidiMessage(req.body);
                          if (!message)
                          {
                              res.status = 400;
                              res.set_content(R"({"error":"invalid MIDI message body"})", "application/json");
                              return;
                          }
                          if (injectMidi)
                          {
                              injectMidi(*message);
                          }
                          res.set_content(R"({"injected":true})", "application/json");
                      });
    }

    [[nodiscard]] ApplyResult applyScriptFromRequestBody(const std::string& body) const
    {
        std::string text;
        try
        {
            text = nlohmann::json::parse(body).value("text", std::string{});
        }
        catch (const nlohmann::json::exception&)
        {
            return {false, "invalid JSON body - expected {\"text\": \"...\"}"};
        }

        const auto result = juce::MessageManager::callSync(
            [this, text]() -> ApplyResult
            {
                ApplyResult r;
                r.compiled = applyScriptText && applyScriptText(juce::String(text));
                r.error = r.compiled ? juce::String{} : (scriptErrorMessage ? scriptErrorMessage() : juce::String{});
                return r;
            });
        return result.value_or(ApplyResult{false, "server not wired to a processor"});
    }

    [[nodiscard]] ApplyResult applyLibraryFromRequestBody(const juce::String& name, const std::string& body) const
    {
        std::string content;
        try
        {
            content = nlohmann::json::parse(body).value("content", std::string{});
        }
        catch (const nlohmann::json::exception&)
        {
            return {false, "invalid JSON body - expected {\"content\": \"...\"}"};
        }

        const auto result = juce::MessageManager::callSync(
            [this, name, content]() -> ApplyResult
            {
                if (!applyLibraryScript)
                {
                    return {false, "server not wired to a processor"};
                }
                const auto [compiled, error] = applyLibraryScript(name, juce::String(content));
                return {compiled, error};
            });
        return result.value_or(ApplyResult{false, "server not wired to a processor"});
    }

    // Channel is 0-based here (matching LUA.md's OnNoteOn/etc.) but juce::MidiMessage's
    // factories take 1-based - convert once, here. Returns nullopt on any malformed body.
    [[nodiscard]] static std::optional<juce::MidiMessage> parseMidiMessage(const std::string& body)
    {
        nlohmann::json j;
        try
        {
            j = nlohmann::json::parse(body);
        }
        catch (const nlohmann::json::exception&)
        {
            return std::nullopt;
        }
        const int channel = j.value("channel", 0) + 1;
        if (channel < 1 || channel > 16)
        {
            return std::nullopt;
        }
        const std::string type = j.value("type", std::string{});
        if (type == "noteOn")
        {
            return juce::MidiMessage::noteOn(channel, j.value("note", 60),
                                             static_cast<juce::uint8>(j.value("velocity", 100)));
        }
        if (type == "noteOff")
        {
            return juce::MidiMessage::noteOff(channel, j.value("note", 60),
                                              static_cast<juce::uint8>(j.value("velocity", 0)));
        }
        if (type == "cc")
        {
            return juce::MidiMessage::controllerEvent(channel, j.value("controller", 0), j.value("value", 0));
        }
        if (type == "programChange")
        {
            return juce::MidiMessage::programChange(channel, j.value("program", 0));
        }
        if (type == "pitchBend")
        {
            return juce::MidiMessage::pitchWheel(channel, j.value("value", 8192));
        }
        if (type == "aftertouch")
        {
            return juce::MidiMessage::channelPressureChange(channel, j.value("value", 0));
        }
        if (type == "polyPressure")
        {
            return juce::MidiMessage::aftertouchChange(channel, j.value("note", 60), j.value("value", 0));
        }
        return std::nullopt;
    }

    [[nodiscard]] std::string contextJson() const
    {
        const auto result = juce::MessageManager::callSync(
            [this]() -> std::string
            {
                nlohmann::json libs = nlohmann::json::array();
                if (libraryScriptNames)
                {
                    for (const auto& name : libraryScriptNames())
                    {
                        libs.push_back(name.toStdString());
                    }
                }
                nlohmann::json params = nlohmann::json::array();
                if (uiParamSlots)
                {
                    for (const auto& slot : uiParamSlots())
                    {
                        if (!slot.claimed)
                        {
                            continue;
                        }
                        params.push_back({{"id", slot.id},
                                          {"name", slot.name},
                                          {"type", luaUiParamTypeName(slot.type)},
                                          {"unit", slot.unit},
                                          {"description", slot.description}});
                    }
                }
                const nlohmann::json j{
                    {"scriptText", currentScriptText ? currentScriptText().toStdString() : std::string{}},
                    {"scriptName", currentScriptName ? currentScriptName().toStdString() : std::string{}},
                    {"patchName", currentPatchName ? currentPatchName().toStdString() : std::string{}},
                    {"hasScriptError", hasScriptError && hasScriptError()},
                    {"scriptErrorMessage", scriptErrorMessage ? scriptErrorMessage().toStdString() : std::string{}},
                    {"libraries", libs},
                    {"uiParams", params},
                };
                return j.dump();
            });
        return result.value_or(std::string{"{}"});
    }

    [[nodiscard]] static std::string_view luaUiParamTypeName(const LuaUiParamType type) noexcept
    {
        switch (type)
        {
            case LuaUiParamType::Knob:
                return "knob";
            case LuaUiParamType::Drop:
                return "drop";
            case LuaUiParamType::Switch:
                return "switch";
        }
        return "knob";
    }

    [[nodiscard]] static juce::File discoveryDirectory()
    {
        auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
#if JUCE_MAC
        base = base.getChildFile("Application Support");
#endif
        const auto dir = base.getChildFile("AbacDsp").getChildFile("AuthoringInstances");
        dir.createDirectory();
        return dir;
    }

    void writeDiscoveryFile() const
    {
        AuthoringInstanceInfo info;
        info.module = m_moduleName.toStdString();
        info.pid = currentProcessId();
        info.port = m_boundPort;
        info.token = m_token;
        info.startedAtEpochMs = juce::Time::currentTimeMillis();
        const auto file = discoveryDirectory().getChildFile(makeDiscoveryFilename(info.pid, info.port));
        file.replaceWithText(juce::String(makeDiscoveryFileJson(info)));
    }

    void removeDiscoveryFile() const
    {
        discoveryDirectory().getChildFile(makeDiscoveryFilename(currentProcessId(), m_boundPort)).deleteFile();
    }

    // clang-format off
    static constexpr std::string_view kDashboardHtml =
"<!doctype html>\n"
"<html><head><meta charset=\"utf-8\">\n"
"<title>Authoring Mode</title>\n"
"<style>\n"
"  body { font-family: -apple-system, BlinkMacSystemFont, sans-serif; margin: 2rem; max-width: 720px; }\n"
"  h1 { font-size: 1.3rem; }\n"
"  .row { display: flex; justify-content: space-between; padding: 0.25rem 0; border-bottom: 1px solid #ddd; }\n"
"  .label { color: #666; }\n"
"  .error { background: #fee; color: #900; padding: 0.5rem; border-radius: 4px; margin: 0.5rem 0; display: none; }\n"
"  .offline { background: #fee; color: #900; padding: 0.5rem; border-radius: 4px; margin: 0.5rem 0; display: none; }\n"
"  pre { background: #f4f4f4; padding: 0.75rem; border-radius: 4px; overflow-x: auto; white-space: pre-wrap; }\n"
"</style></head>\n"
"<body>\n"
"<h1 id=\"title\">Authoring Mode</h1>\n"
"<div class=\"offline\" id=\"offline\">Connection lost - this instance is no longer running "
"(closed, or Authoring Mode was disabled). Showing the last known status below.</div>\n"
"<div class=\"error\" id=\"error\"></div>\n"
"<div class=\"row\"><span class=\"label\">Module</span><span id=\"module\">-</span></div>\n"
"<div class=\"row\"><span class=\"label\">PID</span><span id=\"pid\">-</span></div>\n"
"<div class=\"row\"><span class=\"label\">Port</span><span id=\"port\">-</span></div>\n"
"<div class=\"row\"><span class=\"label\">Wrapper</span><span id=\"wrapperType\">-</span></div>\n"
"<div class=\"row\"><span class=\"label\">Current script</span><span id=\"scriptName\">-</span></div>\n"
"<div class=\"row\"><span class=\"label\">Current patch</span><span id=\"patchName\">-</span></div>\n"
"<div class=\"row\"><span class=\"label\">CPU load</span><span id=\"cpu\">-</span></div>\n"
"<div class=\"row\"><span class=\"label\">Lua pool</span><span id=\"pool\">-</span></div>\n"
"<h2>Quick start</h2>\n"
"<pre id=\"quickstart\"></pre>\n"
"<script>\n"
"const token = new URLSearchParams(location.search).get('token');\n"
"function poll() {\n"
"  fetch('/status', { headers: { 'Authorization': 'Bearer ' + token } })\n"
"    .then(function(r) { if (!r.ok) { throw new Error('http ' + r.status); } return r.json(); })\n"
"    .then(function(s) {\n"
"      document.getElementById('offline').style.display = 'none';\n"
"      document.title = s.module + ' - Authoring Mode';\n"
"      document.getElementById('title').textContent = s.module + ' - Authoring Mode';\n"
"      document.getElementById('module').textContent = s.module;\n"
"      document.getElementById('pid').textContent = s.pid;\n"
"      document.getElementById('port').textContent = s.port;\n"
"      document.getElementById('wrapperType').textContent = s.wrapperType || '-';\n"
"      document.getElementById('scriptName').textContent = s.currentScriptName || '(none)';\n"
"      document.getElementById('patchName').textContent = s.currentPatchName || '(none)';\n"
"      document.getElementById('cpu').textContent = s.cpuLoadPercent.toFixed(1) + '%';\n"
"      document.getElementById('pool').textContent = s.poolBytesInUse + ' bytes';\n"
"      const err = document.getElementById('error');\n"
"      if (s.hasScriptError) { err.style.display = 'block'; err.textContent = s.scriptErrorMessage; }\n"
"      else { err.style.display = 'none'; }\n"
"      const base = location.origin;\n"
"      document.getElementById('quickstart').textContent =\n"
"        'curl -H \"Authorization: Bearer ' + token + '\" ' + base + '/context\\n' +\n"
"        'curl -X POST -H \"Authorization: Bearer ' + token + '\" -d \\'{\"text\":\"...\"}\\' ' + base + '/script';\n"
"    })\n"
"    .catch(function() { document.getElementById('offline').style.display = 'block'; });\n"
"}\n"
"poll();\n"
"setInterval(poll, 1500);\n"
"</script>\n"
"</body></html>\n";
    // clang-format on

    httplib::Server m_server;
    std::jthread m_serverThread;
    juce::String m_moduleName;
    std::string m_token;
    int m_boundPort{0};
};
