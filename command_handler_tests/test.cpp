#include "pch.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <functional>
#include <flev/command_handler.hpp>
using namespace flev;

#include "test_utils.hpp"

// =============================================
// Simple function tests (void, args, returns)
// =============================================

// Test: void(void) function
TEST(CommandHandlerTestSimpleFunctions, VoidFunctionNoArgs) 
{
    auto& handler = flev::Command_handler::instance();

    // Register and execute
    handler.register_command(GET_FUNC_NAME(void_void), &void_void);
    nlohmann::json request = { {"command", "void_void"}};
    auto response = handler.execute(request.dump());
    handler.unregister_command(GET_FUNC_NAME(void_void));

    // Check response
    ASSERT_EQ(response["status"], "ok");
    ASSERT_FALSE(response.contains("result")); // No return value
}

TEST(CommandHandlerTestSimpleFunctions, UnregisterTest)
{
    auto& handler = flev::Command_handler::instance();

    // Register and execute
    handler.register_command(GET_FUNC_NAME(void_void), &void_void);
    nlohmann::json request = { {"command", "void_void"} };
    handler.unregister_command(GET_FUNC_NAME(void_void));

    auto response = handler.execute(request.dump());

    // Check response
    ASSERT_EQ(response["status"], "error");
    ASSERT_EQ(response["message"], nlohmann::json(std::string("Error: Unknown command: void_void")));
}

// Test: void(int) function
TEST(CommandHandlerTestSimpleFunctions, VoidFunctionWithArg)
{
    auto& handler = flev::Command_handler::instance();

    handler.register_command(GET_FUNC_NAME(void_int), &void_int);
    nlohmann::json request = { {"command", "void_int"}, {"data", {5}} };
    auto response = handler.execute(request.dump());
    handler.unregister_command(GET_FUNC_NAME(void_int));

    ASSERT_EQ(response["status"], "ok"); // No error = success
}

// Test: int(void) function
TEST(CommandHandlerTestSimpleFunctions, FunctionWithReturnValue) 
{
    auto& handler = flev::Command_handler::instance();

    handler.register_command(GET_FUNC_NAME(int_void), &int_void);
    nlohmann::json request = { {"command", "int_void"} };
    auto response = handler.execute(request.dump());
    handler.unregister_command(GET_FUNC_NAME(int_void));

    ASSERT_EQ(response["result"], 42);
    ASSERT_EQ(response["status"], "ok");
}

// Test: int(int) function
TEST(CommandHandlerTestSimpleFunctions, FunctionWithArgAndReturn) 
{
    auto& handler = flev::Command_handler::instance();

    handler.register_command(GET_FUNC_NAME(int_int), &int_int);
    nlohmann::json request = { {"command", "int_int"}, {"data", {10}} };
    auto response = handler.execute(request.dump());
    handler.unregister_command(GET_FUNC_NAME(int_int));

    ASSERT_EQ(response["result"], 10);
    ASSERT_EQ(response["status"], "ok");
}

TEST(CommandHandlerTestSimpleFunctions, MultiArgFunction) 
{
    auto& handler = flev::Command_handler::instance();

    handler.register_command(GET_FUNC_NAME(sum_two), &sum_two);
    nlohmann::json request = { {"command", "sum_two"}, {"data", {3.5, 2}} };
    auto response = handler.execute(request.dump());
    handler.unregister_command(GET_FUNC_NAME(sum_two));

    ASSERT_DOUBLE_EQ(response["result"], 5.5);
    ASSERT_EQ(response["status"], "ok");
}

TEST(CommandHandlerTestSimpleFunctions, InvalidArgumentCount) 
{
    auto& handler = flev::Command_handler::instance();

    handler.register_command(GET_FUNC_NAME(int_int), &int_int);
    nlohmann::json request = { {"command", "int_int"}, {"data", {10, 20}} }; // Extra argument
    auto response = handler.execute(request.dump());
    handler.unregister_command(GET_FUNC_NAME(int_int));

    ASSERT_EQ(response["status"], "error");
    std::string message = response["message"].dump();
    ASSERT_TRUE(message.find("Invalid number of arguments") != message.npos);
}

// =============================================
// JSON & Custom structures tests
// =============================================

// Test: Function accepts JSON and returns it
TEST(CommandHandlerTestJsonAndStructs, JsonInputOutput) 
{
    auto& handler = flev::Command_handler::instance();

    handler.register_command(GET_FUNC_NAME(json_to_json), &json_to_json);
    nlohmann::json request = {
        {"command", "json_to_json"},
        {"data", { {{"key", "value"}, {"number", 42}} } }
    };
    auto response = handler.execute(request.dump());
    handler.unregister_command(GET_FUNC_NAME(json_to_json));

    // Verify that the returned JSON matches the input
    ASSERT_EQ(response["status"], "ok");
    ASSERT_EQ(response["result"]["key"], "value");
    ASSERT_EQ(response["result"]["number"], 42);
}

// Test: Function returns a predefined JSON
TEST(CommandHandlerTestJsonAndStructs, ReturnJson) 
{
    auto& handler = flev::Command_handler::instance();

    handler.register_command(GET_FUNC_NAME(get_default_json), &get_default_json);
    nlohmann::json request = { {"command", "get_default_json"}};
    auto response = handler.execute(request.dump());
    handler.unregister_command(GET_FUNC_NAME(get_default_json));

    ASSERT_EQ(response["result"]["status"], "ok");
    ASSERT_EQ(response["result"]["code"], 200);
}

// Test: Function accepts and returns UserData (serialization test)
TEST(CommandHandlerTestJsonAndStructs, UserDataRoundTrip) 
{
    auto& handler = flev::Command_handler::instance();

    handler.register_command(GET_FUNC_NAME(echo_user_data), &echo_user_data);
    nlohmann::json request = {
        {"command", "echo_user_data"},
        {"data", { {{"name", "Alice"}, {"age", 30}, {"is_active", true}} } }
    };
    auto response = handler.execute(request.dump());
    handler.unregister_command(GET_FUNC_NAME(echo_user_data));

    // Verify deserialization correctness
    ASSERT_EQ(response["status"], "ok");
    ASSERT_EQ(response["result"]["name"], "Alice");
    ASSERT_EQ(response["result"]["age"], 30);
    ASSERT_EQ(response["result"]["is_active"], true);
}

// Test: Function parses JSON into UserData
TEST(CommandHandlerTestJsonAndStructs, ParseUserDataFromJson) 
{
    auto& handler = flev::Command_handler::instance();

    handler.register_command(GET_FUNC_NAME(parse_user_data), &parse_user_data);
    nlohmann::json request = {
        {"command", "parse_user_data"},
        {"data", { {{"name", "Bob"}, {"age", 25}, {"is_active", false}} } }
    };
    auto response = handler.execute(request.dump());
    handler.unregister_command(GET_FUNC_NAME(parse_user_data));

    ASSERT_EQ(response["result"]["name"], "Bob");
    ASSERT_EQ(response["result"]["age"], 25);
    ASSERT_EQ(response["result"]["is_active"], false);
}

// Test: Invalid JSON input for UserData parsing
TEST(CommandHandlerTestJsonAndStructs, InvalidUserDataJson) 
{
    auto& handler = flev::Command_handler::instance();

    handler.register_command(GET_FUNC_NAME(parse_user_data), &parse_user_data);
    nlohmann::json request = {
        {"command", "parse_user_data"},
        {"data", { {{"name", "Bob"}, {"age", "invalid_number"}} } } // Error: age is not a number
    };
    auto response = handler.execute(request.dump());
    handler.unregister_command(GET_FUNC_NAME(parse_user_data));

    ASSERT_EQ(response["status"], "error");
    std::string message = response["message"].dump();
    ASSERT_TRUE(message.find("json.exception") != std::string::npos);
}


// =============================================
// Class method binding tests
// =============================================

TEST(CommandHandlerTestClassMethods, ObjectMethodBinding)
{
    auto& handler = Command_handler::instance();
    auto obj = std::make_shared<Test_class>();

    handler.register_command("obj_method", &Test_class::method, obj);

    nlohmann::json request = {
        {"command", "obj_method"},
        {"data", {5}}
    };
    auto response = handler.execute(request.dump());
    handler.unregister_command("obj_method");

    ASSERT_EQ(response["result"], 10);
    ASSERT_EQ(response["status"], "ok");
}

TEST(CommandHandlerTestClassMethods, ObjectNoLongerExists)
{
    auto& handler = Command_handler::instance();
    auto obj = std::make_shared<Test_class>();

    handler.register_command("obj_method", &Test_class::method, obj);
    obj.reset();
    nlohmann::json request = {
        {"command", "obj_method"},
        {"data", {5}}
    };
    auto response = handler.execute(request.dump());
    handler.unregister_command("obj_method");

    ASSERT_EQ(response["status"], "error");
    ASSERT_EQ(response["message"], nlohmann::json(std::string("Error: Object no longer exists")));
}

TEST(CommandHandlerTestClassMethods, StaticMethodBinding)
{
    auto& handler = Command_handler::instance();

    handler.register_command("static_method", &Test_class::static_method);

    nlohmann::json request = {
        {"command", "static_method"},
        {"data", { {} }} // Empty JSON
    };
    auto response = handler.execute(request.dump());
    handler.unregister_command("static_method");

    ASSERT_TRUE(response["result"]["processed"].get<bool>());
    ASSERT_EQ(response["status"], "ok");
}

// =============================================
// Lambda functions tests
// =============================================

TEST(CommandHandlerTestLambdas, StatelessLambdaExecution)
{
    auto& handler = Command_handler::instance();

    handler.register_command("lambda", [](int a, int b) { return a + b; });

    nlohmann::json request = {
        {"command", "lambda"},
        {"data", {2, 3}}
    };
    auto response = handler.execute(request.dump());
    handler.unregister_command("lambda");

    ASSERT_EQ(response["result"], 5);
}

// =============================================
// Duplicate policy tests
// =============================================

TEST(CommandHandlerTestPolicies, ReplacePolicy)
{
    auto& handler = Command_handler::instance();
    handler.set_duplicate_policy(Duplicate_policy::Replace);

    handler.register_command("dupe_test", []() { return "v1"; });
    handler.register_command("dupe_test", []() { return "v2"; });

    auto response = handler.execute(R"({"command": "dupe_test"})");
    handler.unregister_command("dupe_test");

    ASSERT_EQ(response["result"], "v2");
}

TEST(CommandHandlerTestPolicies, ThrowPolicy)
{
    auto& handler = Command_handler::instance();
    handler.set_duplicate_policy(Duplicate_policy::Throw);

    handler.register_command("dupe_test", []() {});
    EXPECT_THROW(
        handler.register_command("dupe_test", []() {}),
        std::runtime_error
    );

    handler.unregister_command("dupe_test");
}

// =============================================
// Complex JSON structures tests
// =============================================

TEST(CommandHandlerTestComplexJson, NestedObjectsAndArrays)
{
    auto& handler = Command_handler::instance();

    handler.register_command("complex", [](const nlohmann::json& j) {
        return j["matrix"][0][0].get<int>() +
            j["config"]["offset"].get<int>();
    });

    nlohmann::json request = {
        {"command", "complex"},
        {"data", {{
            {"matrix", {{1,2}, {3,4}}},
            {"config", {{"offset", 10}}}
        }}
        }
    };

    auto response = handler.execute(request.dump());
    handler.unregister_command("complex");

    ASSERT_EQ(response["result"], 11);
}

TEST(CommandHandlerTestComplexJson, BinaryDataHandling)
{
    auto& handler = Command_handler::instance();

    handler.register_command("binary", [](const std::vector<uint8_t>& data) {
        return data.size();
    });

    std::vector<uint8_t> binary_data = { 0xDE, 0xAD, 0xBE, 0xEF };
    nlohmann::json request = {
        {"command", "binary"},
        {"data", {
            binary_data
        }}
    };
    auto response = handler.execute(request.dump());
    handler.unregister_command("binary");
    ASSERT_EQ(response["result"], 4);
}

// =============================================
// Middleware execution order and priority tests
// =============================================

TEST(CommandHandlerTestMiddleware, PreMiddlewareExecutionOrder)
{
    auto& handler = Command_handler::instance();
    std::vector<int> execution_order;

    // Register pre-middlewares with different priorities
    handler.add_pre_middleware([&](Execution_context&) { execution_order.push_back(1); }, 10);

    // Higher priority
    handler.add_pre_middleware([&](Execution_context&) { execution_order.push_back(2); }, 5); 

    handler.add_pre_middleware([&](Execution_context&) { execution_order.push_back(3); }, 15);

    handler.register_command("noop", []() {});
    handler.execute(R"({"command": "noop"})");
    handler.unregister_command("noop");

    // Verify execution order: 2 (prio 5), 1 (10), 3 (15)
    ASSERT_EQ(execution_order, (std::vector<int>{2, 1, 3}));

    handler.clear_pre_middlewares();
}

// =============================================
// Context modification tests
// =============================================

TEST(CommandHandlerTestMiddleware, PreMiddlewareRequestModification)
{
    auto& handler = Command_handler::instance();

    // Modify request in pre-middleware
    handler.add_pre_middleware([](Execution_context& ctx) {
        ctx.request["data"].push_back(42); // Add extra argument
    });

    handler.register_command("modified_args", [](int a, int b) { return a + b; });

    nlohmann::json request = {
        {"command", "modified_args"},
        {"data", {2}} // Original single argument
    };
    auto response = handler.execute(request.dump());
    handler.unregister_command("modified_args");

    ASSERT_EQ(response["result"], 44); // 2 + 42

    handler.clear_pre_middlewares();
}

TEST(CommandHandlerTestMiddleware, PostMiddlewareResponseModification)
{
    auto& handler = Command_handler::instance();

    // Add timestamp in post-middleware
    handler.add_post_middleware([](Execution_context& ctx) {
        ctx.response["timestamp"] = 1234567890;
        });

    handler.register_command("ping", []() { return "pong"; });
    auto response = handler.execute(R"({"command": "ping"})");
    handler.unregister_command("ping");

    ASSERT_EQ(response["result"], "pong");
    ASSERT_EQ(response["timestamp"], 1234567890);

    handler.clear_post_middlewares();
}

// =============================================
// Middleware error handling tests
// =============================================

TEST(CommandHandlerTestMiddleware, MiddlewareExceptionHandling)
{
    auto& handler = Command_handler::instance();

    // Throw exception in pre-middleware
    handler.add_pre_middleware([](Execution_context&) {
        throw std::runtime_error("Auth failed");
    });

    handler.register_command("secret", []() { return "data"; });
    auto response = handler.execute(R"({"command": "secret"})");
    handler.unregister_command("secret");

    ASSERT_EQ(response["status"], "error");
    std::string message = response["message"].dump();
    ASSERT_TRUE(message.find("Auth failed") != message.npos);

    handler.clear_pre_middlewares();
}

// =============================================
// Inter-middleware communication tests
// =============================================

TEST(CommandHandlerTestMiddleware, MiddlewareDataSharing)
{
    auto& handler = Command_handler::instance();

    // Store data in pre-middleware
    handler.add_pre_middleware([](Execution_context& ctx) {
        ctx.storage["user_id"] = 1001;
        });

    // Retrieve data in post-middleware
    handler.add_post_middleware([](Execution_context& ctx) {
        if (ctx.storage.find("user_id") != ctx.storage.end()) {
            ctx.response["user_id"] = std::any_cast<int>(ctx.storage["user_id"]);
        }
        });

    handler.register_command("get_data", []() { return nlohmann::json(); });
    auto response = handler.execute(R"({"command": "get_data"})");
    handler.unregister_command("get_data");

    ASSERT_EQ(response["user_id"], 1001);

    handler.clear_pre_middlewares();
    handler.clear_post_middlewares();
}

// =============================================
// Middleware cleanup verification
// =============================================

TEST(CommandHandlerTestMiddleware, MiddlewareCleanup)
{
    auto& handler = Command_handler::instance();
    bool executed = false;

    // Add temporary middleware
    handler.add_pre_middleware([&](Execution_context&) { executed = true; });
    handler.clear_pre_middlewares();

    // Verify middleware cleanup
    handler.register_command("no_middleware", []() {});
    auto response = handler.execute(R"({"command": "no_middleware"})");
    handler.unregister_command("no_middleware");

    ASSERT_FALSE(executed); // Middleware should not execute
}