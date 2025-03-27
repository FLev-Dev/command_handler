# Command Handler

**Description:**  
This project is a thread-safe JSON-RPC command handler that supports the registration of free functions, class methods, and stateless lambdas. It facilitates processing requests and responses using the [nlohmann::json](https://github.com/nlohmann/json) library, and supports middleware for additional processing, logging, and duplicate command policy configuration.

## Key Features

- **Thread Safety:**  
  Utilizes mutexes and atomic variables to protect internal data structures.

- **Flexible Command Registration:**  
  Supports free functions, stateless lambdas (captured lambdas are disallowed), and class methods with automatic object lifetime verification.

- **Automatic Argument Deserialization:**  
  Validates the number and types of arguments extracted from JSON. The return value is automatically serialized into JSON (if not `void`).

- **Middleware Support:**  
  Allows registration of pre- and post-processing middleware functions for additional logic (e.g., authentication, logging, modifying requests/responses).

- **Logging:**  
  Supports a custom logger with multiple log levels: Debug, Info, Warning, Error, Critical.

- **Duplicate Policy Handling:**  
  Configurable behavior for duplicate command registrations (Throw, Skip, Replace).

## Requirements

- A C++20 compliant compiler (e.g., MSVC 2022, GCC, Clang)
- [nlohmann/json](https://github.com/nlohmann/json) library
- 
## Usage

The `Command_handler` class expects commands to be sent as JSON messages. Example:

```json
{
  "command": "your_command_name",
  "data": [/* function arguments as an array */],
  "id": 0  // optional request identifier
}
```
*Note:* You can include additional fields for custom middleware data without causing errors

#### Basic Functionality

- **Command Registration:**  
  Register a command using the `register_command` method by providing the command name and a function pointer (and a `shared_ptr` to a class object if it’s a method).

- **Command Execution:**  
  Call the `execute` method with a string containing the JSON command.

#### Advanced Configuration

- **Logging:**  
  Configure logging via `set_logger` and `set_log_level`.

- **Duplicate Command Policy:**  
  Control how duplicate commands are handled using `set_duplicate_policy`.

- **Middleware:**  
  Enhance processing by registering middleware functions with `add_pre_middleware` and `add_post_middleware`.

*Note:* You can also use the macro `GET_FUNC_NAME(func)` and define `FLEV_SKIP_CLASS_NAME` for additional customization.
