#pragma once
#include <unordered_map>
#include <shared_mutex>
#include <nlohmann/json.hpp>


// Platform-specific calling convention
#ifdef _MSC_VER
#   define flev_cdecl __cdecl 
#else
#   define flev_cdecl 
#endif

constexpr const char* remove_class_prefix(const char* str) 
{
    const char* result = str;
    // Пропускаем ведущий '&', если он есть
    if (result[0] == '&') 
    {
        ++result;
    }
    const char* last_colons = nullptr;
#ifdef FLEV_SKIP_CLASS_NAME
    for (const char* p = result; *p != '\0'; ++p) 
    {
        if (p[0] == ':' && p[1] == ':') 
        {
            last_colons = p;
            ++p; // пропускаем второй ':'
        }
    }
#endif
    return last_colons ? (last_colons + 2) : result;
}

#define GET_FUNC_NAME(func) remove_class_prefix(#func)

namespace flev {
namespace functional {
// Checks JSON serialization capability
template <typename T, typename = void>
inline constexpr bool can_be_serialized = false;

template <typename T>
inline constexpr bool can_be_serialized<T, std::void_t<decltype(nlohmann::json(std::declval<T>()))>> = true;

//for args parse
template <typename Func>
struct Function_traits;

//for usual funcs
template <typename Ret, typename... Args>
struct Function_traits<Ret(flev_cdecl*)(Args...)>
{
    using args_tuple = std::tuple<std::decay_t<Args>...>;
    using return_type = Ret;
    static constexpr size_t args_count = sizeof...(Args);
};

//for class methods support
template <typename Ret, typename C, typename... Args>
struct Function_traits<Ret(flev_cdecl C::*)(Args...)>
{
    using args_tuple = std::tuple<std::decay_t<Args>...>;
    using return_type = Ret;
    static constexpr size_t args_count = sizeof...(Args);
};

// Specialization for const methods
template <typename Ret, typename C, typename... Args>
struct Function_traits<Ret(flev_cdecl C::*)(Args...) const>
{
    using args_tuple = std::tuple<std::decay_t<Args>...>;
    using return_type = Ret;
    static constexpr size_t args_count = sizeof...(Args);
};
}

using namespace functional;
class T_Command_handler
{
    using command_function = std::function<nlohmann::json(const nlohmann::json&)>;
    struct Command
    {
        std::weak_ptr<void> weak_obj; //for methods
        command_function function;    //func or method
        bool is_method = false;
    };
    std::map<std::string, Command> commands;
    //if we will change commands after start
    mutable std::shared_mutex M_commands;

    // Template helper to create command function lambda
    template <bool Is_method, typename Func, typename WeakObj>
    auto make_command_function(Func func, WeakObj weak_obj)
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

    // Internal implementation that registers a command
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

    // Public API for free functions
    template <typename Func>
    void register_command(const std::string& name, Func func) noexcept
    {
        // Use an empty weak_ptr for free functions
        register_command_impl<false>(name, func, std::weak_ptr<void>{});
    }
    

    // Public API for methods
    template <typename Func, typename Obj>
    void register_command(const std::string& name, Func func, std::shared_ptr<Obj> obj) noexcept
    {
        // Convert shared_ptr to weak_ptr
        register_command_impl<true>(name, func, std::weak_ptr<Obj>(obj));
    }

    void unregister_command(const std::string& name) noexcept
    {
        std::unique_lock lock(M_commands);
        commands.erase(name);
    }

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

static T_Command_handler Command_handler;
}