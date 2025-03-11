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
// 3. Validate function arguments during registration (+)
//    - Static checks for JSON-serializable argument types  
//    - Example: Reject `void func(int&)` if `int&` can't be deserialized  
//  
// 4. Handle duplicate command names (+)
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
 * @brief Macro to get the cleaned name of a function/method.
 * @param func Function or method.
 * @return String without class/namespace prefixes.
 */
#define GET_FUNC_NAME(func) flev::functional::remove_class_prefix(#func)

namespace flev {

enum class Duplicate_policy
{
    Throw,
    Skip,
    Replace
};

namespace functional {
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
* @brief Checks if a type can be deserialized from nlohmann::json.
* @tparam T Type to check.
*/
template <typename T, typename = void>
inline constexpr bool can_be_deserialized = false;

template <typename T>
inline constexpr bool can_be_deserialized<T,
    std::void_t<decltype(std::declval<const nlohmann::json&>().get<T>())>> = true;

template <typename Tuple, size_t... I>
constexpr bool all_args_deserializable(std::index_sequence<I...>) 
{
    return (can_be_deserialized<std::tuple_element_t<I, Tuple>> && ...);
}

template <typename Tuple>
constexpr bool all_args_deserializable() 
{
    return all_args_deserializable<Tuple>(
        std::make_index_sequence<std::tuple_size_v<Tuple>>{}
    );
}

/**
 * @brief Traits class to detect if a lambda captures context.
 * @tparam T Type to check (lambda, function, etc.).
 */
template <typename T>
struct has_capture_t 
{
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
    using return_type = Ret;                              ///< Return type.
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
class Command_handler
{
    // =============================================
    // Construction control (Singleton pattern)
    // =============================================

    Command_handler() = default;
    ~Command_handler() = default;
    Command_handler(const Command_handler&) = delete;
    Command_handler& operator=(const Command_handler&) = delete;
    Command_handler(Command_handler&&) = delete;
    Command_handler& operator=(Command_handler&&) = delete;

public:

    /**
     * @brief Returns the singleton instance (thread-safe initialization).
     */
    static Command_handler& instance() noexcept 
    {
        static Command_handler handler;
        return handler;
    }

private:
    // =============================================
    // Internal types
    // =============================================

    using command_function = std::function<nlohmann::json(const nlohmann::json&)>; 

    /**
     * @brief Internal command entry with object lifetime tracking for methods.
     */
    struct Command
    {
        std::weak_ptr<void> weak_obj; ///< Weak pointer to bound object (for methods).
        command_function function;    ///< Wrapped function or method.
        bool is_method = false;       ///< True if the command is a method.
    };

    // =============================================
    // Data members
    // =============================================
    std::map<std::string, Command> commands; ///< Registered commands
    mutable std::shared_mutex M_commands;    ///< Mutex for thread safety

    Duplicate_policy duplicate_policy = Duplicate_policy::Throw; ///< Current duplicate policy

    // =============================================
    // Command processing internals
    // =============================================

    /**
     * @brief Creates a JSON-processing wrapper for a function/method.
     * @tparam Is_method True for class methods
     * @details Performs argument count validation and type conversion.
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
            else // return type not a void
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
     * @brief Internal command registration with duplicate policy handling.
     * @throws std::runtime_error if policy is Throw and command exists
     */
    template <bool Is_method, typename Func, typename Weak_obj>
    void register_command_impl(const std::string& name, Func func, Weak_obj weak_obj)
    {
        std::unique_lock lock(M_commands);

        using Traits = Function_traits<decltype(func)>;
        using Args_tuple = typename Traits::args_tuple;
        using Return_type = typename Traits::return_type;

        static_assert(can_be_serialized<Return_type> || std::is_void_v<Return_type>,
            "Return type must be convertible to nlohmann::json");
        static_assert(all_args_deserializable<Args_tuple>(), 
            "All function arguments must be deserializable from nlohmann::json. "
            "Ensure that:\n"
            "1) Argument types are supported by nlohmann::json (e.g., int, std::string).\n"
            "2) Custom types have `adl_serializer` specialization.\n");

        // Handle duplicates according to policy
        if (commands.contains(name))
        {
            switch (duplicate_policy)
            {
            case Duplicate_policy::Replace:
                commands.erase(name);
                break;
            case Duplicate_policy::Skip :
                return;
            case Duplicate_policy::Throw :
                throw std::runtime_error("Command with " + name + " already exists");
            default:
                throw std::runtime_error("Invalid duplicate policy");
            }
        }
        commands[name] = { weak_obj, 
                           make_command_function<Is_method>(func, weak_obj), 
                           Is_method 
        };
    }

public:

    // =============================================
    // Public API
    // =============================================

    /**
     * @brief Checks if a command is registered.
     * @param name Command name to check
     * @returns True if command exists (thread-safe)
     */
    bool command_exists(const std::string& name) const noexcept
    {
        std::shared_lock lock(M_commands);
        return commands.contains(name);
    }

    /**
     * @brief Sets policy for handling duplicate commands.
     * @param new_policy Policy to apply (Throw/Skip/Replace)
     */
    void set_duplicate_policy(Duplicate_policy new_policy) noexcept
    {
        std::unique_lock lock(M_commands);
        duplicate_policy = new_policy;
    }

    /**
     * @returns current duplicate policy.
     */
    Duplicate_policy get_duplicate_policy() const noexcept
    {
        std::shared_lock lock(M_commands);
        return duplicate_policy;
    }

    /**
     * @brief Registers a free function or stateless lambda as a command.
     * @tparam Func Function type.
     * @param name Command name.
     * @param func Function pointer or lambda.
     * @throws static_assert if Func is a lambda with captures.
     */
    template <typename Func>
    void register_command(const std::string& name, Func func)
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
    void register_command(const std::string& name, Func func, std::shared_ptr<Obj> obj)
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
} // namespace flev