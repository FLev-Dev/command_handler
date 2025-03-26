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
