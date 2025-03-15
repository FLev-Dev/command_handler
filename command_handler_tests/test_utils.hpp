#pragma once

#include <nlohmann/json.hpp>
#include <string>

// -----------------------------------------------------------------------------
// Custom Structures
// -----------------------------------------------------------------------------

struct User_data 
{
    std::string name;
    int age;
    bool is_active;
};

// -----------------------------------------------------------------------------
// Additional JSON types
// -----------------------------------------------------------------------------
namespace nlohmann 
{
    template <>
    struct adl_serializer<std::vector<uint8_t>> 
    {
        static void to_json(json& j, const std::vector<uint8_t>& v) 
        {
            j = json::binary(v);
        }
        static void from_json(const json& j, std::vector<uint8_t>& v) 
        {
            if (j.is_binary()) 
            {
                v = j.get_binary(); 
            }
            else if (j.is_object() && j.contains("bytes")) 
            {
                v.clear();
                for (const auto& byte : j["bytes"]) 
                {
                    v.push_back(byte.get<uint8_t>());
                }
            }
            else 
            {
                throw json::type_error::create(302, "type must be binary or object with 'bytes' field", &j);
            }
        }
    };

    template <>
    struct adl_serializer<User_data> 
    {
        static void to_json(nlohmann::json& json, const User_data& data)
        {
            json = nlohmann::json{
                {"name", data.name},
                {"age" , data.age},
                {"is_active", data.is_active}
            };
        }

        static void from_json(const nlohmann::json& json, User_data& data) 
        {
            data.name = json.at("name").get<std::string>();
            data.age  = json.at("age").get<int>();
            data.is_active = json.at("is_active").get<bool>();
        }
    };
} // namespace nlohmann

// -----------------------------------------------------------------------------
// Test Classes
// -----------------------------------------------------------------------------

class Test_class
{
public:
    int method(int x) { return x * 2; }
    static nlohmann::json static_method(const nlohmann::json& j) 
    {
        nlohmann::json result = j;
        result["processed"] = true;
        return result;
    }
};

// -----------------------------------------------------------------------------
// Test Functions
// -----------------------------------------------------------------------------

void void_void() { /*Do nothing*/ }
void void_int(int x) {(void)x;}
int int_void() { return 42; }
int int_int(const int& i) { return i; }
double sum_two(double a, int b) { return a + b; }
nlohmann::json json_to_json(const nlohmann::json& json) { return json; }
nlohmann::json get_default_json() { return { {"status", "ok"}, {"code", 200} }; }
User_data echo_user_data(const User_data& data) { return data; }
User_data parse_user_data(const nlohmann::json& json) { return json.get<User_data>(); }