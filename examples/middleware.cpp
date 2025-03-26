#include <iostream>
#include <flev/command_handler.hpp>

// A free function command that subtracts two numbers
nlohmann::json subtract(const nlohmann::json& a, const nlohmann::json& b) 
{
    return a.get<int>() - b.get<int>();
}

int main() 
{
    auto& handler = flev::Command_handler::instance();

    // Set a simple logger to output log messages to console
    handler.set_logger([](const std::string& message, flev::Log_level level) {
        std::cout << "[Log " << static_cast<int>(level) << "]: " << message << std::endl;
    });

    // Register the "subtract" command
    handler.register_command("subtract", subtract);

    // Add pre-middleware for request validation or logging
    handler.add_pre_middleware([](flev::Execution_context& ctx) {
        // For example, log the command name from the request if available
        if (ctx.request.contains("command")) 
        {
            std::cout << "Pre-middleware: Processing command '"
                << ctx.request["command"].get<std::string>() << "'" << std::endl;
        }
    }); // Default priority is 0

    // Add post-middleware to add metadata to the response
    handler.add_post_middleware([](flev::Execution_context& ctx) {
        ctx.response["processed_by"] = "middleware";
    }, 0); // Also default priority

    // Execute "subtract" command
    std::string request_subtract = R"({"command": "subtract", "data": [10, 4], "id": 3})";
    auto response_subtract = handler.execute(request_subtract);
    std::cout << "Response for subtract: " << response_subtract.dump() << std::endl;

    return 0;
}
