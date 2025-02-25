#pragma once
#include <unordered_map>
#include <shared_mutex>
#include <nlohmann/json.hpp>

// TODO: Expand command handler functionality  
// 1. Implement middleware support  
//    - Add pre/post-execution hooks for logging, auth, validation, etc.  
//    - Example: Allow users to chain middleware like `Command_handler.add_auth_middleware(...)`  
//  
// 2. Add asynchronous command execution  
//    - Support thread pools or std::async for long-running operations  
//  
// 3. Validate function arguments during registration  
//    - Static checks for JSON-serializable argument types  
//    - Example: Reject `void func(int&)` if `int&` can't be deserialized  
//  
// 4. Handle duplicate command names  
//    - Add `overwrite` flag to register_command()  
//    - Throw error/warning if a command already exists (configurable behavior)  
// 
// 5. Custom error handling and diagnostics  
//    - Allow users to register custom exception handlers (e.g., map exceptions to JSON error codes).  
//    - Add optional error codes (e.g., "error_429: Too Many Requests") and stack traces for debugging.  
//  
// 6. Rate limiting and call quotas  
//    - Implement per-command or per-user call limits (e.g., 100 requests/minute).  
//    - Integrate with middleware to reject early (return JSON error 429).  
//  
// 7. Optimize JSON parsing/validation  (Opt)
//    - Replace nlohmann::json with simdjson for faster parsing (or add optional SIMD support).  
//    - Pre-validate JSON structure during registration to reduce runtime overhead. 


/**
 * @brief Platform-specific calling convention macro.
 * @details Uses __cdecl on MSVC compilers, empty otherwise.
 */
#ifdef _MSC_VER
#   define flev_cdecl __cdecl 
#else
#   define flev_cdecl 
#endif

/**
 * @brief Removes class/namespace prefixes and reference symbols from a type name string.
 * @param str Input string (e.g., "&MyClass::method").
 * @return Cleaned name without prefixes (e.g., "method").
 */
constexpr const char* remove_class_prefix(const char* str) 
{
    const char* result = str;
    // Skip leading '&' if present
    if (result[0] == '&') 
    {
        ++result;
    }
    const char* last_colons = nullptr;
#ifdef FLEV_SKIP_CLASS_NAME
    // Find last occurrence of "::"
    for (const char* p = result; *p != '\0'; ++p) 
    {
        if (p[0] == ':' && p[1] == ':') 
        {
            last_colons = p;
            ++p; // Skip second colon
        }
    }
#endif
    return last_colons ? (last_colons + 2) : result;
}

/**
 * @brief Macro to get the cleaned name of a function/method.
 * @param func Function or method.
 * @return String without class/namespace prefixes.
 */
#define GET_FUNC_NAME(func) remove_class_prefix(#func)

namespace flev {
namespace functional {

/**
 * @brief Traits class to detect if a lambda captures context.
 * @tparam T Type to check (lambda, function, etc.).
 */
template <typename T>
struct has_capture_t {
private:
    template <typename U>
    static constexpr bool is_function_or_member_ptr_v =
        std::is_function_v<std::remove_pointer_t<std::decay_t<U>>> ||
        std::is_member_pointer_v<std::decay_t<U>>;

    template <typename U>
    static auto test(int) -> decltype(+std::declval<U>(), std::true_type{});

    template <typename U>
    static std::false_type test(...);

public:
    /// @brief True if the type is a lambda with captures.
    static constexpr bool value =
        !is_function_or_member_ptr_v<T> &&
        std::is_same_v<decltype(test<T>(0)), std::false_type>;
};

/// @brief Helper variable template for has_capture_t.
template <typename T>
inline constexpr bool has_capture = has_capture_t<T>::value;

/**
 * @brief Checks if a type can be serialized to nlohmann::json.
 * @tparam T Type to check.
 */
template <typename T, typename = void>
inline constexpr bool can_be_serialized = false;

/// @brief Specialization for serializable types.
template <typename T>
inline constexpr bool can_be_serialized<T, 
    std::void_t<decltype(nlohmann::json(std::declval<T>()))>> = true;

/**
 * @brief Traits class to extract function/method signature details.
 * @tparam Func Function, method, or lambda type.
 */
template <typename Func>
struct Function_traits;

/// @brief Specialization for lambdas and functors.
template <typename Func>
struct Function_traits : Function_traits<decltype(&Func::operator())> {};

/// @brief Specialization for free functions.
template <typename Ret, typename... Args>
struct Function_traits<Ret(flev_cdecl*)(Args...)>
{
    using args_tuple = std::tuple<std::decay_t<Args>...>; ///< Tuple of decayed argument types.
    using return_type = Ret; ///< Return type.
    static constexpr size_t args_count = sizeof...(Args); ///< Number of arguments.
};

/// @brief Specialization for noexcept free functions.
template <typename Ret, typename... Args>
struct Function_traits<Ret(flev_cdecl*)(Args...) noexcept>
{
    using args_tuple = std::tuple<std::decay_t<Args>...>;
    using return_type = Ret;
    static constexpr size_t args_count = sizeof...(Args);
};

/// @brief Specialization for non-const class methods.
template <typename Ret, typename C, typename... Args>
struct Function_traits<Ret(flev_cdecl C::*)(Args...)>
{
    using args_tuple = std::tuple<std::decay_t<Args>...>;
    using return_type = Ret;
    static constexpr size_t args_count = sizeof...(Args);
};

/// @brief Specialization for noexcept non-const class methods.
template <typename Ret, typename C, typename... Args>
struct Function_traits<Ret(flev_cdecl C::*)(Args...) noexcept>
{
    using args_tuple = std::tuple<std::decay_t<Args>...>;
    using return_type = Ret;
    static constexpr size_t args_count = sizeof...(Args);
};

/// @brief Specialization for const class methods.
template <typename Ret, typename C, typename... Args>
struct Function_traits<Ret(flev_cdecl C::*)(Args...) const>
{
    using args_tuple = std::tuple<std::decay_t<Args>...>;
    using return_type = Ret;
    static constexpr size_t args_count = sizeof...(Args);
};

/// @brief Specialization for const noexcept class methods.
template <typename Ret, typename C, typename... Args>
struct Function_traits<Ret(flev_cdecl C::*)(Args...) const noexcept>
{
    using args_tuple = std::tuple<std::decay_t<Args>...>;
    using return_type = Ret;
    static constexpr size_t args_count = sizeof...(Args);
};

} // namespace functional

using namespace functional;

/**
 * @brief Thread-safe JSON-RPC command handler.
 * @details Supports registration of free functions, class methods, and stateless lambdas.
 */
class T_Command_handler
{
    using command_function = std::function<nlohmann::json(const nlohmann::json&)>; ///< Command function type.

    /// @brief Internal command entry structure.
    struct Command
    {
        std::weak_ptr<void> weak_obj; ///< Weak pointer to bound object (for methods).
        command_function function;    ///< Wrapped function or method.
        bool is_method = false;       ///< True if the command is a method.
    };

    std::map<std::string, Command> commands; ///< Registered commands.
    mutable std::shared_mutex M_commands;    ///< Mutex for thread safety.

    /**
     * @brief Creates a command function wrapper for JSON processing.
     * @tparam Is_method True if the command is a class method.
     * @tparam Func Function/method type.
     * @tparam WeakObj Weak pointer type (for methods).
     * @param func Function/method pointer.
     * @param weak_obj Weak pointer to the bound object.
     * @return Command function wrapper.
     */
    template <bool Is_method, typename Func, typename Weak_obj>
    auto make_command_function(Func func, Weak_obj weak_obj)
    {
        return [func, weak_obj](const nlohmann::json& j) -> nlohmann::json
            {
            using Traits = Function_traits<decltype(func)>;
            using Args_tuple = typename Traits::args_tuple;
            using Return_type = typename Traits::return_type;

            // Check argument count
            if (j.size() != Traits::args_count)
            {
                throw std::invalid_argument("Invalid number of arguments");
            }

            Args_tuple args{};
            // Extract arguments from JSON using index sequence
            [&] <size_t... I>(std::index_sequence<I...>)
            {
                ((std::get<I>(args) = j[I].get<std::tuple_element_t<I, Args_tuple>>()), ...);
            }
            (std::make_index_sequence<Traits::args_count>{});

            if constexpr (std::is_void_v<Return_type>)
            {
                if constexpr (Is_method)
                {
                    // Check object existence for methods
                    if (auto obj = weak_obj.lock())
                    {
                        std::apply([&](auto&&... args)
                            {
                                std::invoke(func, *obj, args...);
                            }, args);
                    }
                    else
                    {
                        throw std::runtime_error("Object no longer exists");
                    }
                }
                else
                {
                    std::apply(func, args);
                }
                return nlohmann::json();
            }
            else
            {
                if constexpr (Is_method)
                {
                    if (auto obj = weak_obj.lock())
                    {
                        return std::apply([&](auto&&... args)
                            {
                                return nlohmann::json(std::invoke(func, obj.get(), args...));
                            }, args);
                    }
                    else
                    {
                        throw std::runtime_error("Object no longer exists");
                    }
                }
                else
                {
                    return nlohmann::json(std::apply(func, args));
                }
            }
        };
    }

    /**
     * @brief Internal command registration implementation.
     * @tparam Is_method True for class methods.
     * @tparam Func Function/method type.
     * @tparam WeakObj Weak pointer type.
     * @param name Command name.
     * @param func Function/method pointer.
     * @param weak_obj Weak pointer to the bound object.
     */
    template <bool Is_method, typename Func, typename WeakObj>
    void register_command_impl(const std::string& name, Func func, WeakObj weak_obj) noexcept
    {
        std::unique_lock lock(M_commands);

        using Return_type = typename Function_traits<decltype(func)>::return_type;
        static_assert(can_be_serialized<Return_type> || std::is_void_v<Return_type>,
            "Return type must be convertible to nlohmann::json");
        commands[name] = { weak_obj, make_command_function<Is_method>(func, weak_obj), Is_method };
    }

public:
    /**
     * @brief Registers a free function or stateless lambda as a command.
     * @tparam Func Function type.
     * @param name Command name.
     * @param func Function pointer or lambda.
     * @throws static_assert if Func is a lambda with captures.
     */
    template <typename Func>
    void register_command(const std::string& name, Func func) noexcept
    {
        static_assert(
            !has_capture<Func>,
            "Captured lambdas are not allowed. Use stateless lambdas or free functions."
        );
        register_command_impl<false>(name, func, std::weak_ptr<void>{});
    }

    /**
     * @brief Registers a class method as a command.
     * @tparam Func Method type.
     * @tparam Obj Class type.
     * @param name Command name.
     * @param func Method pointer.
     * @param obj Shared pointer to the bound object.
     */
    template <typename Func, typename Obj>
    void register_command(const std::string& name, Func func, std::shared_ptr<Obj> obj) noexcept
    {
        register_command_impl<true>(name, func, std::weak_ptr<Obj>(obj));
    }

    /**
     * @brief Unregisters a command by name.
     * @param name Command name to remove.
     */
    void unregister_command(const std::string& name) noexcept
    {
        std::unique_lock lock(M_commands);
        commands.erase(name);
    }

    /**
     * @brief Executes a JSON-RPC request.
     * @param request_str JSON input string.
     * @return JSON response with status, result, or error message.
     */
    nlohmann::json execute(const std::string& request_str) const noexcept
    {
        nlohmann::json response;
        nlohmann::json request;

        if (!nlohmann::json::accept(request_str))
        {
            response["status"] = "error";
            response["message"] = "Invalid json format";
            return response;
        }
        request = nlohmann::json::parse(request_str);

        if (request.contains("id"))
        {
            response["id"] = request["id"];
        }

        //get function name from entry json
        if (!request.contains("command"))
        {
            response["status"] = "error";
            response["message"] = "Missing 'command' field";
            return response;
        }
        if (!request["command"].is_string())
        {
            response["status"] = "error";
            response["message"] = "'command' field not a string";
            return response;
        }
        const std::string command_name = request["command"];

        try
        {
            nlohmann::json args = request.value("data", nlohmann::json::array());
            if (!args.is_array())
            {
                throw std::invalid_argument("Field 'data' must be an array");
            }

            //safety get function from commands
            std::shared_lock lock(M_commands);
            auto it = commands.find(command_name);
            if (it == commands.end())
            {
                response["status"] = "error";
                response["message"] = "Unknown command: " + command_name;
                return response;
            }
            auto command = it->second; //copy for thread safe
            lock.unlock();

            if (nlohmann::json result = command.function(args); !result.is_null())
            {
                response["result"] = std::move(result);
            }
            response["status"] = "ok";
        }
        catch (const std::exception& ex)
        {
            response["status"] = "error";
            response["message"] = std::string("Command execution failed: ") + ex.what();
        }
        return response;
    }
};

/// @brief Global instance of the command handler.
static T_Command_handler Command_handler;
} // namespace flev