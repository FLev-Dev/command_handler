#include <iostream>
#include <flev/command_handler.hpp>

// A simple command function that returns a fixed string
nlohmann::json hello() 
{
    return "Hello, world!";
}

int main() 
{
    auto& handler = flev::Command_handler::instance();

    // Set duplicate policy to Replace (existing command will be replaced)
    handler.set_duplicate_policy(flev::Duplicate_policy::Replace);

    // Register command "greet" for the first time
    handler.register_command("greet", hello);

    // Register command "greet" again - according to the policy, it will replace the previous one
    handler.register_command("greet", hello);

    // Execute "greet" command
    std::string request_greet = R"({"command": "greet", "data": [], "id": 4})";
    auto response_greet = handler.execute(request_greet);
    std::cout << "Response for greet: " << response_greet.dump() << std::endl;

    return 0;
}
