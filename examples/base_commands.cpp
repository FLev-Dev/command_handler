#include <iostream>
#include <flev/command_handler.hpp>

// A free function command that adds two integers
nlohmann::json add(const nlohmann::json& a, const nlohmann::json& b) 
{
    return a.get<int>() + b.get<int>();
}

// A class with a member function command
class Calculator 
{
public:
    // Multiply two numbers
    nlohmann::json multiply(const nlohmann::json& a, const nlohmann::json& b) 
    {
        return a.get<int>() * b.get<int>();
    }
};

int main() 
{
    auto& handler = flev::Command_handler::instance();

    // Register a free function command "add"
    // @note You can use macro GET_FUNC_NAME for auto convert name to cstr
    handler.register_command("add", add);

    // Create a Calculator object and register its member function "multiply"
    auto calc = std::make_shared<Calculator>();

    // @note For class metods GET_FUNC_NAME will return "class::func",
    //      but you can define FLEV_SKIP_CLASS_NAME for get only "func"
    handler.register_command("multiply", &Calculator::multiply, calc);

    // Execute "add" command
    std::string request_add = R"({"command": "add", "data": [3, 5], "id": 1})";
    auto response_add = handler.execute(request_add);
    std::cout << "Response for add: " << response_add.dump() << std::endl;

    // Execute "multiply" command
    std::string request_multiply = R"({"command": "multiply", "data": [4, 6], "id": 2})";
    auto response_multiply = handler.execute(request_multiply);
    std::cout << "Response for multiply: " << response_multiply.dump() << std::endl;

    return 0;
}