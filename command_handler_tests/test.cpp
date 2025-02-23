#include "pch.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <functional>
#include <flev/command_handler.hpp>
using namespace flev;


int simple_function(const int& i)
{
    return i;
}

TEST(CommandHandlerTest, RegisterAndExecuteSimpleFunction)
{
    Command_handler.register_command(GET_FUNC_NAME(simple_function), simple_function);
     
    nlohmann::json request = { { "command", "simple_function" }, { "data", { 1 } } };
    auto response = Command_handler.execute(request.dump());
    Command_handler.unregister_command(GET_FUNC_NAME(simple_function));

    ASSERT_EQ(response["status"], "ok");
    ASSERT_EQ(response["result"], nlohmann::json(1));

}

class MyClass
{
public:
    std::string method(const double& i, const int& k)
    {
        return std::to_string(int(i + k));
    }
};

TEST(CommandHandlerTest, RegisterAndExecuteMethod)
{
    auto obj = std::make_shared<MyClass>();
    Command_handler.register_command(GET_FUNC_NAME(&MyClass::method), &MyClass::method, obj);

    nlohmann::json request = { { "command", "MyClass::method" }, { "data", {10, 20} } };
    auto response = Command_handler.execute(request.dump());
    Command_handler.unregister_command(GET_FUNC_NAME(&MyClass::method));

    ASSERT_EQ(response["status"], "ok");
    ASSERT_EQ(response["result"], nlohmann::json(std::string("30")));

}

TEST(CommandHandlerTest, UnregisterMethod)
{
    auto obj = std::make_shared<MyClass>();
    Command_handler.register_command(GET_FUNC_NAME(&MyClass::method), &MyClass::method, obj);
    Command_handler.unregister_command(GET_FUNC_NAME(&MyClass::method));

    nlohmann::json request = { { "command", "MyClass::method" }, { "data", {10, 20} } };
    auto response = Command_handler.execute(request.dump());

    ASSERT_EQ(response["status"], "error");
    ASSERT_EQ(response["message"], nlohmann::json(std::string("Unknown command: MyClass::method")));

}
TEST(CommandHandlerTest, ExecuteMethodWithExpiredObject)
{
    std::shared_ptr<MyClass> obj = std::make_shared<MyClass>();
    auto weak_obj = std::weak_ptr<MyClass>(obj);

    Command_handler.register_command(GET_FUNC_NAME(&MyClass::method), &MyClass::method, obj);

    obj.reset();  // free obj

    nlohmann::json request = { { "command", "MyClass::method" }, { "data", {1, 2} } };
    auto response = Command_handler.execute(request.dump());
    Command_handler.unregister_command(GET_FUNC_NAME(&MyClass::method));

    ASSERT_EQ(response["status"], "error");
    ASSERT_EQ(response["message"], nlohmann::json(std::string("Command execution failed: Object no longer exists")));
}

nlohmann::json function_with_two_args(const std::string& j1, const short& j2)
{
    return j1 + std::to_string(j2);
}

TEST(CommandHandlerTest, IncorrectNumberOfArguments)
{
    Command_handler.register_command(GET_FUNC_NAME(function_with_two_args), function_with_two_args);

    nlohmann::json request = { { "command", "function_with_two_args" }, { "data", {1} } };  // only one argh
    auto response = Command_handler.execute(request.dump());
    Command_handler.unregister_command(GET_FUNC_NAME(function_with_two_args));

    ASSERT_EQ(response["status"], "error");
    ASSERT_EQ(response["message"], nlohmann::json(std::string("Command execution failed: Invalid number of arguments")));
}

std::string function_with_string_and_int(const std::string& j1, const int& j2)
{
    return j1 + std::to_string(j2);
}

TEST(CommandHandlerTest, IncorrectArgumentType)
{

    Command_handler.register_command(GET_FUNC_NAME(function_with_string_and_int), function_with_string_and_int);

    nlohmann::json request = { { "command", "function_with_string_and_int" }, { "data", {"Hello", "World"}} };
    auto response = Command_handler.execute(request.dump());
    Command_handler.unregister_command(GET_FUNC_NAME(function_with_string_and_int));

    ASSERT_EQ(response["status"], "error");
    //TODO
    //ASSERT_EQ(response["message"], nlohmann::json(std::string("Field 'data' must be an array")));
}

class MyClassVoid
{
public:
    void method_void() const
    {
        // do nothing
    }
};

TEST(CommandHandlerTest, ExecuteVoidMethod)
{
    auto obj = std::make_shared<MyClassVoid>();
    Command_handler.register_command(GET_FUNC_NAME(&MyClassVoid::method_void), &MyClassVoid::method_void, obj);

    nlohmann::json request = { { "command", "MyClassVoid::method_void" }};
    auto response = Command_handler.execute(request.dump());
    Command_handler.unregister_command(GET_FUNC_NAME(&MyClassVoid::method_void));

    ASSERT_EQ(response["status"], "ok");
    ASSERT_TRUE(response["result"].is_null()); // we wait null
}
/*
TEST(CommandHandlerTest, RegisterLambdaCommand)
{
    auto lambda = [](const int& j1, const int& j2, const int& j3) 
        {
        return j1 + j2;
    };
    Command_handler.register_command("lambda_command", lambda);

    nlohmann::json request = { { "command", "lambda_command" }, { "data", {1, 2, 3} } };
    auto response = Command_handler.execute(request.dump());

    ASSERT_EQ(response["status"], "ok");
    ASSERT_EQ(response["result"], nlohmann::json(3));
    Command_handler.unregister_command("lambda_command");
}

*/
TEST(CommandHandlerTest, InvalidJSON)
{
    nlohmann::json request = { { "command", "unknown_command" }, { "data", {0}} };
    auto response = Command_handler.execute(request.dump());

    ASSERT_EQ(response["status"], "error");
    ASSERT_EQ(response["message"], nlohmann::json(std::string("Unknown command: unknown_command")));
}