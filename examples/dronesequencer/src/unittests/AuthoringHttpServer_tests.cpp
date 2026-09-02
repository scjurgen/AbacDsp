#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <httplib.h>
#include <juce_events/juce_events.h>
#include <optional>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "inc/AuthoringHttpServer.h"

namespace
{
// Runs a blocking HTTP-client action on its own thread while pumping this (test) thread's
// JUCE dispatch loop, so a server handler's callSync() has something to deliver its posted
// message to - a plain gtest binary otherwise never pumps messages, and callSync blocks forever.
template <typename Action>
auto runWhilePumpingMessages(Action&& action) -> decltype(action())
{
    using Result = decltype(action());
    std::promise<Result> promise;
    auto future = promise.get_future();
    std::thread worker([&promise, &action] { promise.set_value(action()); });

    while (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    }
    worker.join();
    return future.get();
}
}

class AuthoringHttpServerTest : public ::testing::Test
{
  protected:
    // The first thread to touch MessageManager becomes "the message thread" in JUCE's own
    // bookkeeping - this must be the test thread, not the server's background thread.
    void SetUp() override
    {
        juce::MessageManager::getInstance();
    }

    AuthoringHttpServer server;
};

TEST_F(AuthoringHttpServerTest, RequestWithoutTokenIsRejected)
{
    ASSERT_TRUE(server.start("testmodule"));
    const auto status = runWhilePumpingMessages(
        [&]
        {
            httplib::Client client("127.0.0.1", server.boundPort());
            const auto res = client.Get("/status");
            return res ? res->status : -1;
        });
    EXPECT_EQ(status, 401);
}

TEST_F(AuthoringHttpServerTest, RequestWithWrongTokenIsRejected)
{
    ASSERT_TRUE(server.start("testmodule"));
    const auto status = runWhilePumpingMessages(
        [&]
        {
            httplib::Client client("127.0.0.1", server.boundPort());
            client.set_bearer_token_auth("not-the-real-token");
            const auto res = client.Get("/status");
            return res ? res->status : -1;
        });
    EXPECT_EQ(status, 401);
}

TEST_F(AuthoringHttpServerTest, StatusReflectsWiredCallbacks)
{
    server.currentScriptName = [] { return juce::String("demo-script"); };
    server.currentPatchName = [] { return juce::String("demo-patch"); };
    server.hasScriptError = [] { return false; };
    ASSERT_TRUE(server.start("testmodule"));

    const auto body = runWhilePumpingMessages(
        [&]
        {
            httplib::Client client("127.0.0.1", server.boundPort());
            client.set_bearer_token_auth(server.token());
            const auto res = client.Get("/status");
            return res ? res->body : std::string{};
        });

    const auto j = nlohmann::json::parse(body);
    EXPECT_EQ(j.at("module").get<std::string>(), "testmodule");
    EXPECT_EQ(j.at("currentScriptName").get<std::string>(), "demo-script");
    EXPECT_EQ(j.at("currentPatchName").get<std::string>(), "demo-patch");
    EXPECT_FALSE(j.at("hasScriptError").get<bool>());
}

TEST_F(AuthoringHttpServerTest, PostScriptRoundTripsApplyResult)
{
    server.applyScriptText = [](const juce::String& text) { return text == "good script"; };
    server.scriptErrorMessage = [] { return juce::String("compile failed"); };
    ASSERT_TRUE(server.start("testmodule"));

    const auto goodBody = runWhilePumpingMessages(
        [&]
        {
            httplib::Client client("127.0.0.1", server.boundPort());
            client.set_bearer_token_auth(server.token());
            const auto res = client.Post("/script", R"({"text": "good script"})", "application/json");
            return res ? res->body : std::string{};
        });
    const auto goodResult = nlohmann::json::parse(goodBody);
    EXPECT_TRUE(goodResult.at("compiled").get<bool>());
    EXPECT_EQ(goodResult.at("error").get<std::string>(), "");

    const auto badBody = runWhilePumpingMessages(
        [&]
        {
            httplib::Client client("127.0.0.1", server.boundPort());
            client.set_bearer_token_auth(server.token());
            const auto res = client.Post("/script", R"({"text": "bad script"})", "application/json");
            return res ? res->body : std::string{};
        });
    const auto badResult = nlohmann::json::parse(badBody);
    EXPECT_FALSE(badResult.at("compiled").get<bool>());
    EXPECT_EQ(badResult.at("error").get<std::string>(), "compile failed");
}

TEST_F(AuthoringHttpServerTest, RootContentNegotiatesHtmlForBrowsers)
{
    ASSERT_TRUE(server.start("testmodule"));
    const auto contentType = runWhilePumpingMessages(
        [&]
        {
            httplib::Client client("127.0.0.1", server.boundPort());
            client.set_bearer_token_auth(server.token());
            httplib::Headers headers{{"Accept", "text/html"}};
            const auto res = client.Get("/", headers);
            return res ? res->get_header_value("Content-Type") : std::string{};
        });
    EXPECT_NE(contentType.find("text/html"), std::string::npos) << contentType;
}

TEST_F(AuthoringHttpServerTest, ListPatchesReturnsWiredNames)
{
    server.patchNames = [] { return std::vector<juce::String>{"alpha", "Factory/beta"}; };
    ASSERT_TRUE(server.start("testmodule"));

    const auto body = runWhilePumpingMessages(
        [&]
        {
            httplib::Client client("127.0.0.1", server.boundPort());
            client.set_bearer_token_auth(server.token());
            const auto res = client.Get("/patches");
            return res ? res->body : std::string{};
        });
    const auto names = nlohmann::json::parse(body).at("patches").get<std::vector<std::string>>();
    EXPECT_EQ(names, (std::vector<std::string>{"alpha", "Factory/beta"}));
}

TEST_F(AuthoringHttpServerTest, GetPatchReturnsJsonWhenFoundAnd404Otherwise)
{
    server.patchJson = [](const juce::String& name) -> std::optional<juce::String>
    {
        if (name == "alpha")
        {
            return juce::String(R"({"level": 0.5})");
        }
        return std::nullopt;
    };
    ASSERT_TRUE(server.start("testmodule"));

    const auto found = runWhilePumpingMessages(
        [&]
        {
            httplib::Client client("127.0.0.1", server.boundPort());
            client.set_bearer_token_auth(server.token());
            const auto res = client.Get("/patches/alpha");
            return std::make_pair(res ? res->status : -1, res ? res->body : std::string{});
        });
    EXPECT_EQ(found.first, 200);
    EXPECT_EQ(found.second, R"({"level": 0.5})");

    const auto missing = runWhilePumpingMessages(
        [&]
        {
            httplib::Client client("127.0.0.1", server.boundPort());
            client.set_bearer_token_auth(server.token());
            const auto res = client.Get("/patches/missing");
            return res ? res->status : -1;
        });
    EXPECT_EQ(missing, 404);
}

TEST_F(AuthoringHttpServerTest, SaveLoadAndDeletePatchDispatchToWiredCallbacks)
{
    std::vector<std::string> calls;
    server.savePatchNamed = [&](const juce::String& name)
    {
        calls.push_back("save:" + name.toStdString());
        return true;
    };
    server.loadPatchNamed = [&](const juce::String& name)
    {
        calls.push_back("load:" + name.toStdString());
        return true;
    };
    server.deletePatchNamed = [&](const juce::String& name)
    {
        calls.push_back("delete:" + name.toStdString());
        return false;
    };
    ASSERT_TRUE(server.start("testmodule"));

    const auto statuses = runWhilePumpingMessages(
        [&]
        {
            httplib::Client client("127.0.0.1", server.boundPort());
            client.set_bearer_token_auth(server.token());
            const auto save = client.Post("/patches/alpha", "", "application/json");
            const auto load = client.Post("/patches/alpha/load", "", "application/json");
            const auto del = client.Delete("/patches/alpha");
            return std::make_tuple(save ? save->status : -1, load ? load->status : -1, del ? del->status : -1);
        });
    EXPECT_EQ(std::get<0>(statuses), 200);
    EXPECT_EQ(std::get<1>(statuses), 200);
    EXPECT_EQ(std::get<2>(statuses), 404) << "deletePatchNamed was wired to return false";
    EXPECT_EQ(calls, (std::vector<std::string>{"save:alpha", "load:alpha", "delete:alpha"}));
}

TEST_F(AuthoringHttpServerTest, ListAndGetLibraryDispatchToWiredCallbacks)
{
    server.libraryScriptNames = [] { return std::vector<juce::String>{"helpers", "scales"}; };
    server.libraryScriptSource = [](const juce::String& name) -> std::optional<juce::String>
    {
        if (name == "helpers")
        {
            return juce::String("function Helper() return 42 end");
        }
        return std::nullopt;
    };
    ASSERT_TRUE(server.start("testmodule"));

    const auto listBody = runWhilePumpingMessages(
        [&]
        {
            httplib::Client client("127.0.0.1", server.boundPort());
            client.set_bearer_token_auth(server.token());
            const auto res = client.Get("/libraries");
            return res ? res->body : std::string{};
        });
    const auto names = nlohmann::json::parse(listBody).at("libraries").get<std::vector<std::string>>();
    EXPECT_EQ(names, (std::vector<std::string>{"helpers", "scales"}));

    const auto found = runWhilePumpingMessages(
        [&]
        {
            httplib::Client client("127.0.0.1", server.boundPort());
            client.set_bearer_token_auth(server.token());
            const auto res = client.Get("/libraries/helpers");
            return std::make_pair(res ? res->status : -1, res ? res->body : std::string{});
        });
    EXPECT_EQ(found.first, 200);
    EXPECT_NE(found.second.find("Helper"), std::string::npos);

    const auto missing = runWhilePumpingMessages(
        [&]
        {
            httplib::Client client("127.0.0.1", server.boundPort());
            client.set_bearer_token_auth(server.token());
            const auto res = client.Get("/libraries/missing");
            return res ? res->status : -1;
        });
    EXPECT_EQ(missing, 404);
}

TEST_F(AuthoringHttpServerTest, PostLibraryRoundTripsApplyResult)
{
    server.applyLibraryScript = [](const juce::String& name,
                                   const juce::String& content) -> std::pair<bool, juce::String>
    {
        if (name == "helpers" && content == "good content")
        {
            return {true, {}};
        }
        return {false, "current script does not compile against this library"};
    };
    ASSERT_TRUE(server.start("testmodule"));

    const auto goodBody = runWhilePumpingMessages(
        [&]
        {
            httplib::Client client("127.0.0.1", server.boundPort());
            client.set_bearer_token_auth(server.token());
            const auto res = client.Post("/libraries/helpers", R"({"content": "good content"})", "application/json");
            return res ? res->body : std::string{};
        });
    const auto goodResult = nlohmann::json::parse(goodBody);
    EXPECT_TRUE(goodResult.at("compiled").get<bool>());

    const auto badBody = runWhilePumpingMessages(
        [&]
        {
            httplib::Client client("127.0.0.1", server.boundPort());
            client.set_bearer_token_auth(server.token());
            const auto res = client.Post("/libraries/helpers", R"({"content": "bad content"})", "application/json");
            return res ? res->body : std::string{};
        });
    const auto badResult = nlohmann::json::parse(badBody);
    EXPECT_FALSE(badResult.at("compiled").get<bool>());
    EXPECT_EQ(badResult.at("error").get<std::string>(), "current script does not compile against this library");
}

TEST_F(AuthoringHttpServerTest, PostMidiParsesEachMessageTypeAndRejectsGarbage)
{
    std::vector<juce::MidiMessage> received;
    server.injectMidi = [&](const juce::MidiMessage& m) { received.push_back(m); };
    ASSERT_TRUE(server.start("testmodule"));

    const auto post = [&](const std::string& body)
    {
        return runWhilePumpingMessages(
            [&]
            {
                httplib::Client client("127.0.0.1", server.boundPort());
                client.set_bearer_token_auth(server.token());
                const auto res = client.Post("/midi", body, "application/json");
                return res ? res->status : -1;
            });
    };

    EXPECT_EQ(post(R"({"type": "noteOn", "channel": 0, "note": 60, "velocity": 100})"), 200);
    EXPECT_EQ(post(R"({"type": "cc", "channel": 0, "controller": 74, "value": 127})"), 200);
    EXPECT_EQ(post(R"({"type": "not-a-real-type", "channel": 0})"), 400);
    EXPECT_EQ(post("not json at all"), 400);

    ASSERT_EQ(received.size(), 2u);
    EXPECT_TRUE(received[0].isNoteOn());
    EXPECT_EQ(received[0].getNoteNumber(), 60);
    EXPECT_EQ(received[0].getChannel(), 1) << "channel 0 in the API is MIDI channel 1 (1-based)";
    EXPECT_TRUE(received[1].isController());
    EXPECT_EQ(received[1].getControllerNumber(), 74);
}
