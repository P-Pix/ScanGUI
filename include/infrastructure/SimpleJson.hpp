/**
 * @file SimpleJson.hpp
 * @brief Petit parseur/constructeur JSON autonome utilise par ScanGUI.
 *
 * Le projet evite maintenant les regex fragiles pour les payloads applicatifs. Ce header
 * implemente un parseur recursif minimal suffisant pour les objets, tableaux, chaines,
 * nombres et booleens manipules par l'API locale, sans ajouter de dependance externe.
 */
#ifndef SCANGUI_INFRASTRUCTURE_SIMPLE_JSON_HPP
#define SCANGUI_INFRASTRUCTURE_SIMPLE_JSON_HPP

#include <cctype>
#include <cmath>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace scangui::json {

/**
 * @brief Valeur JSON minimale manipulée par le parseur interne.
 *
 * Le type couvre uniquement les formes nécessaires au projet : null, booléen, nombre, chaîne,
 * tableau et objet. Il évite une dépendance externe tout en centralisant les traitements JSON.
 */
struct Value {
    using Object = std::map<std::string, Value>;
    using Array = std::vector<Value>;
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> data{nullptr};

    [[nodiscard]] bool is_null() const { return std::holds_alternative<std::nullptr_t>(data); }
    [[nodiscard]] bool is_bool() const { return std::holds_alternative<bool>(data); }
    [[nodiscard]] bool is_number() const { return std::holds_alternative<double>(data); }
    [[nodiscard]] bool is_string() const { return std::holds_alternative<std::string>(data); }
    [[nodiscard]] bool is_array() const { return std::holds_alternative<Array>(data); }
    [[nodiscard]] bool is_object() const { return std::holds_alternative<Object>(data); }

    [[nodiscard]] const std::string& as_string() const { return std::get<std::string>(data); }
    [[nodiscard]] double as_number() const { return std::get<double>(data); }
    [[nodiscard]] bool as_bool() const { return std::get<bool>(data); }
    [[nodiscard]] const Array& as_array() const { return std::get<Array>(data); }
    [[nodiscard]] const Object& as_object() const { return std::get<Object>(data); }
};

/**
 * @brief Échappe une chaîne pour l'insérer dans un JSON.
 *
 * @param value Texte brut à sérialiser.
 * @return Texte échappé sans guillemets englobants.
 */
inline std::string escape(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (unsigned char c : value) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (c < 0x20) {
                    const char* hex = "0123456789abcdef";
                    out += "\\u00";
                    out.push_back(hex[(c >> 4) & 0x0f]);
                    out.push_back(hex[c & 0x0f]);
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    return out;
}

/**
 * @brief Parseur récursif descendant pour les payloads JSON simples de ScanGUI.
 *
 * Il reste volontairement limité aux besoins du projet et les helpers publics convertissent les
 * erreurs de parsing en `std::optional` lorsque l'appelant préfère un échec maîtrisé.
 */
class Parser {
public:
    explicit Parser(const std::string& text) : text_(text) {}

    [[nodiscard]] Value parse() {
        skip_spaces();
        Value value = parse_value();
        skip_spaces();
        if (pos_ != text_.size()) {
            throw std::runtime_error("JSON invalide: caracteres supplementaires");
        }
        return value;
    }

private:
    const std::string& text_;
    std::size_t pos_{0};

    void skip_spaces() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
    }

    [[nodiscard]] char peek() const {
        return pos_ < text_.size() ? text_[pos_] : '\0';
    }

    char consume() {
        if (pos_ >= text_.size()) {
            throw std::runtime_error("JSON invalide: fin inattendue");
        }
        return text_[pos_++];
    }

    void expect(char expected) {
        if (consume() != expected) {
            throw std::runtime_error("JSON invalide: caractere inattendu");
        }
    }

    bool consume_literal(const std::string& literal) {
        if (text_.compare(pos_, literal.size(), literal) == 0) {
            pos_ += literal.size();
            return true;
        }
        return false;
    }

    [[nodiscard]] Value parse_value() {
        skip_spaces();
        const char c = peek();
        if (c == '"') return Value{parse_string()};
        if (c == '{') return Value{parse_object()};
        if (c == '[') return Value{parse_array()};
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return Value{parse_number()};
        if (consume_literal("true")) return Value{true};
        if (consume_literal("false")) return Value{false};
        if (consume_literal("null")) return Value{nullptr};
        throw std::runtime_error("JSON invalide: valeur inconnue");
    }

    [[nodiscard]] std::string parse_string() {
        expect('"');
        std::string out;
        while (pos_ < text_.size()) {
            const char c = consume();
            if (c == '"') {
                return out;
            }
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            const char escaped = consume();
            switch (escaped) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    // Decodage Unicode simplifie: les payloads ScanGUI utilisent surtout ASCII/UTF-8.
                    // On conserve une representation UTF-8 correcte pour le plan latin basique.
                    int code = 0;
                    for (int i = 0; i < 4; ++i) {
                        const char h = consume();
                        code <<= 4;
                        if (h >= '0' && h <= '9') code += h - '0';
                        else if (h >= 'a' && h <= 'f') code += 10 + h - 'a';
                        else if (h >= 'A' && h <= 'F') code += 10 + h - 'A';
                        else throw std::runtime_error("JSON invalide: sequence unicode");
                    }
                    if (code <= 0x7f) out.push_back(static_cast<char>(code));
                    else if (code <= 0x7ff) {
                        out.push_back(static_cast<char>(0xc0 | ((code >> 6) & 0x1f)));
                        out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
                    } else {
                        out.push_back(static_cast<char>(0xe0 | ((code >> 12) & 0x0f)));
                        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
                        out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
                    }
                    break;
                }
                default:
                    throw std::runtime_error("JSON invalide: echappement inconnu");
            }
        }
        throw std::runtime_error("JSON invalide: chaine non terminee");
    }

    [[nodiscard]] double parse_number() {
        const std::size_t start = pos_;
        if (peek() == '-') ++pos_;
        while (std::isdigit(static_cast<unsigned char>(peek()))) ++pos_;
        if (peek() == '.') {
            ++pos_;
            while (std::isdigit(static_cast<unsigned char>(peek()))) ++pos_;
        }
        if (peek() == 'e' || peek() == 'E') {
            ++pos_;
            if (peek() == '+' || peek() == '-') ++pos_;
            while (std::isdigit(static_cast<unsigned char>(peek()))) ++pos_;
        }
        return std::stod(text_.substr(start, pos_ - start));
    }

    [[nodiscard]] Value::Array parse_array() {
        expect('[');
        Value::Array array;
        skip_spaces();
        if (peek() == ']') {
            consume();
            return array;
        }
        while (true) {
            array.push_back(parse_value());
            skip_spaces();
            const char c = consume();
            if (c == ']') break;
            if (c != ',') throw std::runtime_error("JSON invalide: virgule attendue");
        }
        return array;
    }

    [[nodiscard]] Value::Object parse_object() {
        expect('{');
        Value::Object object;
        skip_spaces();
        if (peek() == '}') {
            consume();
            return object;
        }
        while (true) {
            skip_spaces();
            if (peek() != '"') throw std::runtime_error("JSON invalide: cle attendue");
            std::string key = parse_string();
            skip_spaces();
            expect(':');
            object.emplace(std::move(key), parse_value());
            skip_spaces();
            const char c = consume();
            if (c == '}') break;
            if (c != ',') throw std::runtime_error("JSON invalide: virgule attendue");
        }
        return object;
    }
};

/**
 * @brief Parse un document JSON sans propager les exceptions.
 *
 * @return Valeur racine ou `std::nullopt` si le document est invalide.
 */
inline std::optional<Value> parse(const std::string& text) {
    try {
        return Parser(text).parse();
    } catch (...) {
        return std::nullopt;
    }
}

inline const Value* find_field(const Value& value, const std::string& key) {
    if (!value.is_object()) return nullptr;
    const auto& object = value.as_object();
    auto it = object.find(key);
    return it == object.end() ? nullptr : &it->second;
}

inline std::optional<std::string> extract_string(const std::string& json, const std::string& key) {
    auto root = parse(json);
    if (!root) return std::nullopt;
    const Value* value = find_field(*root, key);
    if (value == nullptr || !value->is_string()) return std::nullopt;
    return value->as_string();
}

inline std::optional<int> extract_int(const std::string& json, const std::string& key) {
    auto root = parse(json);
    if (!root) return std::nullopt;
    const Value* value = find_field(*root, key);
    if (value == nullptr || !value->is_number()) return std::nullopt;
    return static_cast<int>(std::llround(value->as_number()));
}

inline std::optional<long long> extract_long(const std::string& json, const std::string& key) {
    auto root = parse(json);
    if (!root) return std::nullopt;
    const Value* value = find_field(*root, key);
    if (value == nullptr || !value->is_number()) return std::nullopt;
    return static_cast<long long>(std::llround(value->as_number()));
}

inline std::optional<bool> extract_bool(const std::string& json, const std::string& key) {
    auto root = parse(json);
    if (!root) return std::nullopt;
    const Value* value = find_field(*root, key);
    if (value == nullptr || !value->is_bool()) return std::nullopt;
    return value->as_bool();
}

inline std::optional<std::string> extract_object(const std::string& json, const std::string& key) {
    auto root = parse(json);
    if (!root) return std::nullopt;
    const Value* value = find_field(*root, key);
    if (value == nullptr || !value->is_object()) return std::nullopt;
    // L'ancienne API renvoyait une sous-chaine JSON; on reconstruit uniquement ce qui est utile
    // dans les tests historiques en exposant un objet minimal via stringify.
    std::ostringstream out;
    out << '{';
    bool first = true;
    for (const auto& [k, v] : value->as_object()) {
        if (!first) out << ',';
        first = false;
        out << '"' << escape(k) << "\":";
        if (v.is_string()) out << '"' << escape(v.as_string()) << '"';
        else if (v.is_bool()) out << (v.as_bool() ? "true" : "false");
        else if (v.is_number()) out << static_cast<long long>(std::llround(v.as_number()));
        else out << "null";
    }
    out << '}';
    return out.str();
}

/**
 * @brief Extrait les objets contenus dans un tableau JSON et les ressérialise un par un.
 *
 * @param json Document JSON racine.
 * @param key Nom du champ tableau, `items` par défaut pour les endpoints API.
 * @return Liste d'objets JSON sous forme de chaînes.
 */
inline std::vector<std::string> extract_object_array(const std::string& json, const std::string& key = "items") {
    std::vector<std::string> objects;
    auto root = parse(json);
    if (!root) return objects;
    const Value* arrayValue = find_field(*root, key);
    if (arrayValue == nullptr || !arrayValue->is_array()) return objects;
    for (const auto& item : arrayValue->as_array()) {
        if (!item.is_object()) continue;
        std::ostringstream out;
        out << '{';
        bool first = true;
        for (const auto& [k, v] : item.as_object()) {
            if (!first) out << ',';
            first = false;
            out << '"' << escape(k) << "\":";
            if (v.is_string()) out << '"' << escape(v.as_string()) << '"';
            else if (v.is_bool()) out << (v.as_bool() ? "true" : "false");
            else if (v.is_number()) out << static_cast<long long>(std::llround(v.as_number()));
            else out << "null";
        }
        out << '}';
        objects.push_back(out.str());
    }
    return objects;
}

inline std::string string_field(const std::string& json, const std::string& key, std::string fallback = {}) {
    return extract_string(json, key).value_or(std::move(fallback));
}

inline int int_field(const std::string& json, const std::string& key, int fallback = 0) {
    return extract_int(json, key).value_or(fallback);
}

inline long long long_field(const std::string& json, const std::string& key, long long fallback = 0) {
    return extract_long(json, key).value_or(fallback);
}

} // namespace scangui::json

#endif
