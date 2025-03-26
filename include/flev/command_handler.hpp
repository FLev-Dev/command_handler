#pragma once
#include <unordered_map>
#include <shared_mutex>
#include <nlohmann/json.hpp>

/**
 * @brief Platform-specific calling convention macro.
 * 
 * @details Uses __cdecl on MSVC compilers, empty otherwise.
 */
#ifdef _MSC_VER
#   define flev_cdecl __cdecl 
#else
#   define flev_cdecl 
#endif


/**
 * @brief Macro to get the cleaned name of a function/method.
 * 
 * @param func[in] - Function or method.
 * 
 * @returns String without class/namespace prefixes.
 */
#define GET_FUNC_NAME(func) flev::detail::remove_class_prefix(#func)

namespace flev {

enum class Duplicate_policy
{
    Throw,
    Skip,
    Replace
};

enum class Log_level
{
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

/**
 * @brief Execution context shared between middleware and commands.
 */
struct Execution_context
{
    nlohmann::json request;
    nlohmann::json response;
    /// @brief User-defined storage for passing data between middleware.
    std::unordered_map<std::string, std::any> storage;
};

namespace detail {

/**
 * @brief Removes class/namespace prefixes and reference symbols from a type name string.
 * 
 * @param str[in] - Input string (e.g., "&MyClass::method").
 * 
 * @returns Cleaned name without prefixes (e.g., "method").
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
* 
* @tparam T - Type to check.
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
 * 
 * @tparam T - Type to check (lambda, function, etc.).
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
 * 
 * @tparam T - Type to check.
 */
template <typename T, typename = void>
inline constexpr bool can_be_serialized = false;

/// @brief Specialization for serializable types.
template <typename T>
inline constexpr bool can_be_serialized<T, 
    std::void_t<decltype(nlohmann::json(std::declval<T>()))>> = true;

/**
 * @brief Traits class to extract function/method signature details.
 * 
 * @tparam Func - Function, method, or lambda type.
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

} // namespace detail

using namespace detail;

/**
 * @brief Thread-safe JSON-RPC command handler.
 * 
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
    using middleware_function = std::function<void(Execution_context&)>;
    using log_function = std::function<void(const std::string&, Log_level)>;
    /**
     * @brief Internal middleware entry with priority.
     */
    struct Middleware_entry 
    {
        int priority;              ///< Execution order (lower = earlier)
        middleware_function func;  ///< Middleware function
    };

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

    std::vector<Middleware_entry> pre_middlewares; ///< Pre-processing middleware chain
    std::vector<Middleware_entry> post_middlewares;///< Post-processing middleware chain
    mutable std::shared_mutex M_middlewares;       ///< Mutex for thread safety 

    Duplicate_policy duplicate_policy = Duplicate_policy::Throw; ///< Current duplicate policy

    log_function logger;                                 ///< User-provided logger
    mutable std::shared_mutex M_logger;                  ///< Mutex for thread-safe logger access
    std::atomic<Log_level> log_level = Log_level::Debug;

    // =============================================
    // Command processing internals
    // =============================================

    /**
     * @brief Internal logging method with level check
     */
    void log(const std::string& message, Log_level level) const noexcept
    {
        if (level < log_level.load(std::memory_order_acquire))
        {
            return;
        }

        std::shared_lock lock(M_logger);
        if (logger) 
        {
            try {
                logger(message, level);
            }
            catch (...) {
                // Prevent exceptions from propagating
            }
        }
    }

    /**
     * @brief Creates a JSON-processing wrapper for a function/method.
     * 
     * @tparam Is_method - True for class methods
     * 
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
     * 
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
            "Return type must be serializable to JSON. "
            "For custom types, specialize nlohmann::adl_serializer.");
        static_assert(all_args_deserializable<Args_tuple>(), 
            "All function arguments must be deserializable from nlohmann::json. "
            "Ensure that:\n"
            "1) Argument types are supported by nlohmann::json (e.g., int, std::string).\n"
            "2) Custom types have `adl_serializer` specialization.\n");

        // Handle duplicates according to policy
        if (commands.contains(name))
        {
            log("Command '" + name + "' already exists", Log_level::Debug);
            switch (duplicate_policy)
            {
            case Duplicate_policy::Replace:
                commands.erase(name);
                log("Replacing existing command: " + name, Log_level::Warning);
                break;
            case Duplicate_policy::Skip :
                log("Skipping duplicate command: " + name, Log_level::Info);
                return;
            case Duplicate_policy::Throw :
                log("Duplicate command registration attempted: " + name, Log_level::Error);
                throw std::runtime_error("Command with " + name + " already exists");
            default:
                log("Invalid duplicate policy: " +
                    std::to_string(static_cast<int>(duplicate_policy)), Log_level::Critical);
                throw std::runtime_error("Invalid duplicate policy");
            }
        }
        commands[name] = { weak_obj, 
                           make_command_function<Is_method>(func, weak_obj), 
                           Is_method 
        };

        log("Command '" + name + "' registered successfully. "
            "Arguments: " + std::to_string(Traits::args_count) + ", "
            "Return type: " + typeid(Return_type).name(),
            Log_level::Info);
    }

public:

    // =============================================
    // Public API
    // =============================================

    /**
     * @brief Checks if a command is registered.
     * 
     * @param name[in] - Command name to check
     * 
     * @returns True if command exists (thread-safe)
     */
    bool command_exists(const std::string& name) const noexcept
    {
        std::shared_lock lock(M_commands);
        return commands.contains(name);
    }

    /**
     * @brief Registers a free function or stateless lambda as a command.
     * 
     * @tparam Func - Function type.
     * 
     * @param name[in] - Command name.
     * @param func[in] - Function pointer or lambda.
     * 
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
     * 
     * @tparam Func - Method type.
     * @tparam Obj - Class type.
     * 
     * @param name[in] - Command name.
     * @param func[in] - Method pointer.
     * @param obj[in]  - Shared pointer to the bound object.
     */
    template <typename Func, typename Obj>
    void register_command(const std::string& name, Func func, std::shared_ptr<Obj> obj)
    {
        register_command_impl<true>(name, func, std::weak_ptr<Obj>(obj));
    }

    /**
     * @brief Unregisters a command by name.
     * 
     * @param name[in] - Command name to remove.
     */
    void unregister_command(const std::string& name) noexcept
    {
        std::unique_lock lock(M_commands);
        commands.erase(name);
        log("Unregistered command: " + name, Log_level::Info);
    }

    /**
     * @brief Executes a JSON-RPC request.
     * 
     * @param request_str[in] - JSON input string.
     * 
     * @returns JSON response with status, result, or error message.
     */
    nlohmann::json execute(const std::string& request_str) const noexcept
    {
        Execution_context context; // Stores request/response data and middleware state
        nlohmann::json response;

        log("Processing request: " + request_str, Log_level::Debug);

        try
        {
            // Parse JSON input (throws nlohmann::json::parse_error on failure)
            context.request = nlohmann::json::parse(request_str);

            // Validate mandatory fields
            if (!context.request.contains("command"))
            {
                throw std::invalid_argument("Missing 'command' field");
            }
            if (!context.request["command"].is_string())
            {
                throw std::invalid_argument("'command' field must be a string");
            }

            // Copy request ID to response (if provided)
            if (context.request.contains("id"))
            {
                context.response["id"] = context.request["id"];
            }

            // Execute pre-middleware chain (e.g., auth, logging)
            {
                std::shared_lock lock(M_middlewares);
                log("Processing pre-middleware chain (" +
                    std::to_string(pre_middlewares.size()) + " items)",
                    Log_level::Debug);
                for (const auto& middleware : pre_middlewares)
                {
                    middleware.func(context);  // Pass context to each middleware
                }
            }

            // Extract command name and arguments
            const std::string command_name = context.request["command"];
            const auto& args = context.request.value("data", nlohmann::json::array());

            // Validate arguments format
            if (!args.is_array())
            {
                throw std::invalid_argument("'data' field must be a JSON array");
            }

            // Find registered command
            std::shared_lock lock(M_commands);
            auto command_it = commands.find(command_name);
            if (command_it == commands.end())
            {
                throw std::runtime_error("Unknown command: " + command_name);
            }
            auto command = command_it->second;  // Copy command for thread safety
            lock.unlock();

            // Execute command and store result
            auto result = command.function(args);
            if (!result.is_null())
            {
                context.response["result"] = result;
            }
            context.response["status"] = "ok";  // Default success status

            // Execute post-middleware chain (e.g., add metadata)
            {
                std::shared_lock lock(M_middlewares);
                log("Processing post-middleware chain (" +
                    std::to_string(post_middlewares.size()) + " items)",
                    Log_level::Debug);
                for (const auto& middleware : post_middlewares)
                {
                    middleware.func(context);  // Modify response if needed
                }
            }
        }
        catch (const nlohmann::json::parse_error& ex)
        {
            // Handle JSON parsing errors
            context.response["status"] = "error";
            context.response["message"] = "Invalid JSON: " + std::string(ex.what());
            log("JSON parse error: " + std::string(ex.what()), Log_level::Error);
        }
        catch (const std::exception& ex)
        {
            // Handle all other exceptions
            context.response["status"] = "error";
            context.response["message"] = "Error: " + std::string(ex.what());
            log("Execution error: " + std::string(ex.what()), Log_level::Error);
        }
        catch (...) 
        {
            context.response["status"] = "error";
            context.response["message"] = "Unknown error";
            log("Unknown error occurred", Log_level::Critical);
        }
        return context.response;
    }

    // =============================================
    //  Duplicate Policy API
    // =============================================

    /**
     * @brief Sets policy for handling duplicate commands.
     * 
     * @param new_policy[in] - Policy to apply (Throw/Skip/Replace)
     */
    void set_duplicate_policy(const Duplicate_policy new_policy) noexcept
    {
        std::unique_lock lock(M_commands);
        duplicate_policy = new_policy;
    }

    /**
     * @returns Current duplicate policy.
     */
    Duplicate_policy get_duplicate_policy() const noexcept
    {
        std::shared_lock lock(M_commands);
        return duplicate_policy;
    }

    // =============================================
    //  Middleware Registration API
    // =============================================

    /**
     * @brief Adds a pre-middleware with optional priority.
     * 
     * @param func[in] - Middleware function
     * @param priority[in][opt] - Execution order (lower = earlier). Default: 0.
     * 
     * @note If middleware has the same priority, 
     *   they will be called in the order they are added.
     */
    void add_pre_middleware(middleware_function func, int priority = 0)
    {
        std::unique_lock lock(M_middlewares);
        pre_middlewares.push_back({ priority, std::move(func) });

        // Sort by priority (ascending)
        std::stable_sort(pre_middlewares.begin(), pre_middlewares.end(),
            [](const auto& a, const auto& b) { return a.priority < b.priority; });
        
        log("Added pre-middleware (priority: " + std::to_string(priority) + ")",
            Log_level::Debug);
    }

    /**
     * @brief Clears all registered pre-middlewares.
     */
    void clear_pre_middlewares() noexcept
    {
        std::unique_lock lock(M_middlewares);
        const size_t count = pre_middlewares.size();
        pre_middlewares.clear();
        log("Cleared " + std::to_string(count) + " pre-middlewares", Log_level::Debug);
    }

    /**
     * @brief Adds a post-middleware with optional priority.
     * 
     * @param func[in] - Middleware function
     * @param priority[in][opt] - Execution order (lower = earlier). Default: 0.
     * 
     * @note If middleware has the same priority, 
     *   they will be called in the order they are added.
     */
    void add_post_middleware(middleware_function func, int priority = 0)
    {
        std::unique_lock lock(M_middlewares);
        post_middlewares.push_back({ priority, std::move(func) });

        // Sort by priority (ascending)
        std::stable_sort(post_middlewares.begin(), post_middlewares.end(),
            [](const auto& a, const auto& b) { return a.priority < b.priority; });

        log("Added post-middleware (priority: " + std::to_string(priority) + ")",
            Log_level::Debug);
    }

    /**
     * @brief Clears all registered post-middlewares.
     */
    void clear_post_middlewares() noexcept
    {
        std::unique_lock lock(M_middlewares);
        const size_t count = post_middlewares.size();
        post_middlewares.clear();
        log("Cleared " + std::to_string(count) + " post-middlewares", Log_level::Debug);
    }

    // =============================================
    //  Logging API
    // =============================================

    /**
     * @brief Sets custom logger function
     *
     * @param func[in] - Logger function to install
     */
    void set_logger(log_function func) noexcept
    {
        std::unique_lock lock(M_logger);
        logger = std::move(func);
    }

    /**
     * @brief Removes current logger
     */
    void remove_logger() noexcept
    {
        std::unique_lock lock(M_logger);
        logger = nullptr;
    }

    /**
     * @brief Returns current log level
     */
    Log_level get_log_level() const noexcept
    {
        return log_level.load(std::memory_order_relaxed);
    }

    /**
     * @brief Sets log level
     *
     * @param level[in] - new Log_level
     * 
     * @see flev::Log_level enum class
     */
    void set_log_level(Log_level level) noexcept
    {
        log_level.store(level, std::memory_order_release);
    }

};
} // namespace flev