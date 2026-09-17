#ifndef TCP_HPP
#define TCP_HPP

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <map>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace tcp {

/// Один модульный тест
struct TestCase {
    std::string       id;         /// уникальный идентификатор теста
    double            cost = 1.0; /// стоимость прогона (условное время, > 0)
    double            history = 0.0; /// нормированная история отказов [0;1]
    std::vector<bool> covers;     /// битовая маска покрытых сущностей
};

/// Дефект, используемый только для ОЦЕНКИ порядка (не для приоритизации)
struct Fault {
    std::string              id;
    std::vector<std::string> detected_by; /// id тестов, выявляющих дефект
    double                   severity = 1.0; /// вес дефекта для APFD_c
};

/// Тестовый набор целиком
struct Suite {
    std::string              name;
    std::vector<std::string> entities; /// имена покрываемых сущностей
    std::vector<TestCase>    tests;
    std::vector<Fault>       faults;   /// может быть пустым

    std::size_t n() const { return tests.size(); }      // число тестов
    std::size_t m() const { return entities.size(); }   // число сущностей

    /// Индекс теста по идентификатору; -1, если не найден.
    int index_of(const std::string& tid) const {
        for (std::size_t i = 0; i < tests.size(); ++i)
            if (tests[i].id == tid) return static_cast<int>(i);
        return -1;
    }

    /// Проверка внутренней согласованности: длины масок, положительность
    /// стоимостей, существование тестов, упомянутых в описании дефектов
    void validate() const {
        if (tests.empty()) throw std::runtime_error("suite: пустой набор тестов");
        for (const auto& t : tests) {
            if (t.covers.size() != entities.size())
                throw std::runtime_error("suite: длина маски покрытия теста '" +
                                         t.id + "' не равна числу сущностей");
            if (!(t.cost > 0.0))
                throw std::runtime_error("suite: неположительная стоимость теста '" + t.id + "'");
        }
        for (const auto& f : faults)
            for (const auto& tid : f.detected_by)
                if (index_of(tid) < 0)
                    throw std::runtime_error("suite: дефект '" + f.id +
                                             "' ссылается на неизвестный тест '" + tid + "'");
    }
};

inline std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

inline std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string              cur;
    std::istringstream       is(s);
    while (std::getline(is, cur, sep)) out.push_back(trim(cur));
    return out;
}

struct JsonValue;
using JsonObject = std::map<std::string, JsonValue>;
using JsonArray  = std::vector<JsonValue>;

struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object } type = Type::Null;
    bool        b = false;
    double      num = 0.0;
    std::string str;
    JsonArray   arr;
    JsonObject  obj;

    const JsonValue& at(const std::string& k) const {
        auto it = obj.find(k);
        if (it == obj.end()) throw std::runtime_error("json: нет ключа '" + k + "'");
        return it->second;
    }
    bool has(const std::string& k) const { return obj.find(k) != obj.end(); }
};

class JsonParser {
public:
    explicit JsonParser(const std::string& text) : s_(text) {}

    JsonValue parse() {
        skip();
        JsonValue v = value();
        skip();
        return v;
    }

private:
    const std::string& s_;
    std::size_t        p_ = 0;

    void skip() {
        while (p_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[p_]))) ++p_;
    }
    [[noreturn]] void fail(const std::string& what) const {
        throw std::runtime_error("json: " + what + " (позиция " + std::to_string(p_) + ")");
    }
    void expect(char c) {
        if (p_ >= s_.size() || s_[p_] != c) fail(std::string("ожидался символ '") + c + "'");
        ++p_;
    }

    JsonValue value() {
        skip();
        if (p_ >= s_.size()) fail("неожиданный конец данных");
        switch (s_[p_]) {
            case '{': return object();
            case '[': return array();
            case '"': { JsonValue v; v.type = JsonValue::Type::String; v.str = string(); return v; }
            case 't': case 'f': return boolean();
            case 'n': literal("null"); return JsonValue{};
            default:  return number();
        }
    }

    void literal(const char* lit) {
        for (const char* c = lit; *c; ++c) {
            if (p_ >= s_.size() || s_[p_] != *c) fail("некорректный литерал");
            ++p_;
        }
    }

    JsonValue boolean() {
        JsonValue v;
        v.type = JsonValue::Type::Bool;
        if (s_[p_] == 't') { literal("true");  v.b = true;  }
        else               { literal("false"); v.b = false; }
        return v;
    }

    JsonValue number() {
        std::size_t start = p_;
        if (p_ < s_.size() && (s_[p_] == '-' || s_[p_] == '+')) ++p_;
        while (p_ < s_.size() &&
               (std::isdigit(static_cast<unsigned char>(s_[p_])) || s_[p_] == '.' ||
                s_[p_] == 'e' || s_[p_] == 'E' || s_[p_] == '-' || s_[p_] == '+'))
            ++p_;
        if (start == p_) fail("ожидалось число");
        JsonValue v;
        v.type = JsonValue::Type::Number;
        v.num  = std::stod(s_.substr(start, p_ - start));
        return v;
    }

    std::string string() {
        expect('"');
        std::string out;
        while (p_ < s_.size() && s_[p_] != '"') {
            if (s_[p_] == '\\') {
                ++p_;
                if (p_ >= s_.size()) fail("обрыв эскейп-последовательности");
                char c = s_[p_++];
                switch (c) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    default:  out += c;    break;
                }
            } else {
                out += s_[p_++];
            }
        }
        expect('"');
        return out;
    }

    JsonValue array() {
        JsonValue v;
        v.type = JsonValue::Type::Array;
        expect('[');
        skip();
        if (p_ < s_.size() && s_[p_] == ']') { ++p_; return v; }
        while (true) {
            v.arr.push_back(value());
            skip();
            if (p_ < s_.size() && s_[p_] == ',') { ++p_; continue; }
            expect(']');
            break;
        }
        return v;
    }

    JsonValue object() {
        JsonValue v;
        v.type = JsonValue::Type::Object;
        expect('{');
        skip();
        if (p_ < s_.size() && s_[p_] == '}') { ++p_; return v; }
        while (true) {
            skip();
            std::string k = string();
            skip();
            expect(':');
            v.obj[k] = value();
            skip();
            if (p_ < s_.size() && s_[p_] == ',') { ++p_; continue; }
            expect('}');
            break;
        }
        return v;
    }
};

inline std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("не удалось открыть файл: " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

inline Suite load_json(const std::string& path) {
    JsonValue root = JsonParser(read_file(path)).parse();
    Suite     s;
    s.name = root.has("name") ? root.at("name").str : "suite";

    for (const auto& e : root.at("entities").arr) s.entities.push_back(e.str);

    // Отображение "имя сущности" -> индекс столбца в маске покрытия.
    std::map<std::string, std::size_t> col;
    for (std::size_t i = 0; i < s.entities.size(); ++i) col[s.entities[i]] = i;

    for (const auto& tj : root.at("tests").arr) {
        TestCase t;
        t.id      = tj.at("id").str;
        t.cost    = tj.has("cost")    ? tj.at("cost").num    : 1.0;
        t.history = tj.has("history") ? tj.at("history").num : 0.0;
        t.covers.assign(s.entities.size(), false);
        if (tj.has("covers"))
            for (const auto& c : tj.at("covers").arr) {
                auto it = col.find(c.str);
                if (it == col.end())
                    throw std::runtime_error("json: тест '" + t.id +
                                             "' покрывает неизвестную сущность '" + c.str + "'");
                t.covers[it->second] = true;
            }
        s.tests.push_back(std::move(t));
    }

    if (root.has("faults"))
        for (const auto& fj : root.at("faults").arr) {
            Fault f;
            f.id       = fj.at("id").str;
            f.severity = fj.has("severity") ? fj.at("severity").num : 1.0;
            for (const auto& d : fj.at("detected_by").arr) f.detected_by.push_back(d.str);
            s.faults.push_back(std::move(f));
        }

    s.validate();
    return s;
}

/// Загрузка из CSV
inline Suite load_csv(const std::string& path) {
    std::istringstream in(read_file(path));
    Suite              s;
    s.name = "csv-suite";
    std::string line;
    bool        header_read = false;

    while (std::getline(in, line)) {
        std::string ln = trim(line);
        if (ln.empty() || ln[0] == '#') continue;
        std::vector<std::string> f = split(ln, ',');
        if (!header_read) {                       // заголовок задаёт сущности
            if (f.size() < 4)
                throw std::runtime_error("csv: заголовок должен содержать не менее 4 полей");
            for (std::size_t i = 3; i < f.size(); ++i) s.entities.push_back(f[i]);
            header_read = true;
            continue;
        }
        if (f.size() != s.entities.size() + 3)
            throw std::runtime_error("csv: в строке '" + f[0] + "' неверное число полей");
        TestCase t;
        t.id      = f[0];
        t.cost    = std::stod(f[1]);
        t.history = std::stod(f[2]);
        t.covers.assign(s.entities.size(), false);
        for (std::size_t i = 0; i < s.entities.size(); ++i)
            t.covers[i] = (f[i + 3] == "1" || f[i + 3] == "true");
        s.tests.push_back(std::move(t));
    }
    if (!header_read) throw std::runtime_error("csv: файл не содержит данных");
    s.validate();
    return s;
}

/// Выбор загрузчика по расширению файла
inline Suite load(const std::string& path) {
    if (path.size() >= 5 && path.compare(path.size() - 5, 5, ".json") == 0) return load_json(path);
    if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".csv") == 0)  return load_csv(path);
    throw std::runtime_error("неподдерживаемое расширение файла: " + path);
}

struct Params {
    double alpha = 1.0;  /// вес приращения покрытия
    double beta  = 0.0;  /// вес истории отказов
    bool   use_cost = false; /// делить ли оценку на нормированную стоимость
};

/// Одна строка протокола пошагового выполнения (для отчёта и отладки)
struct TraceStep {
    int                 iteration;   ///< номер итерации, с 1
    std::string         chosen;      ///< выбранный тест
    bool                reset;       ///< был ли сброс множества непокрытых
    std::vector<std::string> ids;    ///< кандидаты (в исходном порядке)
    std::vector<int>    delta;       ///< D(t) для каждого кандидата
    std::vector<double> score;       ///< g(t) для каждого кандидата
};

/// Гибкий жадный алгоритм приоритизации
/// При alpha=1, beta=0, use_cost=false вырождается в классический additional greedy
inline std::vector<int> prioritize_greedy(const Suite& s, const Params& p,
                                          std::vector<TraceStep>* trace = nullptr) {
    const std::size_t n = s.n(), m = s.m();

    double c_avg = 0.0;
    for (const auto& t : s.tests) c_avg += t.cost;
    c_avg /= static_cast<double>(n);

    std::vector<bool> uncovered(m, true);   // ещё не покрытые сущности
    std::vector<bool> used(n, false);       // уже выбранные тесты
    std::vector<int>  order;
    order.reserve(n);

    for (std::size_t step = 0; step < n; ++step) {
        // Сброс U, если всё покрыто, но тесты остались
        bool reset = false;
        if (std::none_of(uncovered.begin(), uncovered.end(), [](bool b) { return b; })) {
            std::fill(uncovered.begin(), uncovered.end(), true);
            reset = true;
        }

        TraceStep ts;
        ts.iteration = static_cast<int>(step) + 1;
        ts.reset     = reset;

        int    best = -1;
        double best_score = -1.0;
        for (std::size_t i = 0; i < n; ++i) {
            if (used[i]) continue;
            const TestCase& t = s.tests[i];

            int d = 0;
            for (std::size_t j = 0; j < m; ++j)
                if (t.covers[j] && uncovered[j]) ++d;

            double g = p.alpha * (m ? static_cast<double>(d) / static_cast<double>(m) : 0.0)
                     + p.beta * t.history;
            if (p.use_cost) g /= (t.cost / c_avg);

            if (trace) {
                ts.ids.push_back(t.id);
                ts.delta.push_back(d);
                ts.score.push_back(g);
            }
            if (g > best_score) { best_score = g; best = static_cast<int>(i); }
        }

        used[best] = true;
        order.push_back(best);
        for (std::size_t j = 0; j < m; ++j)
            if (s.tests[best].covers[j]) uncovered[j] = false;

        if (trace) { ts.chosen = s.tests[best].id; trace->push_back(std::move(ts)); }
    }
    return order;
}

/// "Total" -- сортировка по убыванию абсолютного покрытия (без учёта пересечений)
inline std::vector<int> prioritize_total(const Suite& s) {
    std::vector<int> order(s.n());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        auto cnt = [&](int i) {
            return std::count(s.tests[i].covers.begin(), s.tests[i].covers.end(), true);
        };
        return cnt(a) > cnt(b);
    });
    return order;
}

/// Исходный порядок набора (контрольная стратегия)
inline std::vector<int> prioritize_original(const Suite& s) {
    std::vector<int> order(s.n());
    std::iota(order.begin(), order.end(), 0);
    return order;
}

/// Позиция (1..n) первого теста в порядке `order`, выявляющего дефект f.
/// Возвращает 0, если дефект не выявляется ни одним тестом набора.
inline int first_detect_position(const Suite& s, const std::vector<int>& order, const Fault& f) {
    for (std::size_t pos = 0; pos < order.size(); ++pos) {
        const std::string& tid = s.tests[order[pos]].id;
        if (std::find(f.detected_by.begin(), f.detected_by.end(), tid) != f.detected_by.end())
            return static_cast<int>(pos) + 1;
    }
    return 0;
}

/// APFD = 1 - (TF_1 + ... + TF_m) / (n*m) + 1/(2n)
inline double apfd(const Suite& s, const std::vector<int>& order) {
    if (s.faults.empty()) throw std::runtime_error("apfd: в наборе не заданы дефекты");
    const double n = static_cast<double>(s.n());
    const double m = static_cast<double>(s.faults.size());
    double sum = 0.0;
    for (const auto& f : s.faults) {
        int tf = first_detect_position(s, order, f);
        if (tf == 0) throw std::runtime_error("apfd: дефект '" + f.id + "' не выявляется набором");
        sum += tf;
    }
    return 1.0 - sum / (n * m) + 1.0 / (2.0 * n);
}

/// APFD_c -- версия APFD с учётом стоимости тестов и важности дефектов
///   APFD_c = SUM_i sev_i * ( SUM_{j=TF_i..n} c_j - 0.5 * c_{TF_i} ) / ( SUM_j c_j * SUM_i sev_i )
inline double apfd_c(const Suite& s, const std::vector<int>& order) {
    if (s.faults.empty()) throw std::runtime_error("apfd_c: в наборе не заданы дефекты");
    double total_cost = 0.0, total_sev = 0.0, num = 0.0;
    for (const auto& t : s.tests) total_cost += t.cost;
    for (const auto& f : s.faults) total_sev += f.severity;

    for (const auto& f : s.faults) {
        int tf = first_detect_position(s, order, f);
        if (tf == 0) throw std::runtime_error("apfd_c: дефект '" + f.id + "' не выявляется набором");
        double tail = 0.0;                       // стоимость "хвоста" от TF_i до n
        for (std::size_t pos = static_cast<std::size_t>(tf) - 1; pos < order.size(); ++pos)
            tail += s.tests[order[pos]].cost;
        num += f.severity * (tail - 0.5 * s.tests[order[tf - 1]].cost);
    }
    return num / (total_cost * total_sev);
}

/// Доля покрытых сущностей после первых k тестов порядка
inline double coverage_after(const Suite& s, const std::vector<int>& order, std::size_t k) {
    std::vector<bool> cov(s.m(), false);
    for (std::size_t i = 0; i < k && i < order.size(); ++i)
        for (std::size_t j = 0; j < s.m(); ++j)
            if (s.tests[order[i]].covers[j]) cov[j] = true;
    return s.m() ? static_cast<double>(std::count(cov.begin(), cov.end(), true)) /
                       static_cast<double>(s.m())
                 : 0.0;
}

/// Преобразование порядка (индексы) в список идентификаторов тестов
inline std::vector<std::string> to_ids(const Suite& s, const std::vector<int>& order) {
    std::vector<std::string> out;
    out.reserve(order.size());
    for (int i : order) out.push_back(s.tests[i].id);
    return out;
}

/// Обратное преобразование: список идентификаторов -> порядок (индексы)
inline std::vector<int> from_ids(const Suite& s, const std::vector<std::string>& ids) {
    std::vector<int> order;
    for (const auto& id : ids) {
        int i = s.index_of(id);
        if (i < 0) throw std::runtime_error("порядок содержит неизвестный тест '" + id + "'");
        order.push_back(i);
    }
    return order;
}

} // namespace tcp

#endif // TCP_HPP
