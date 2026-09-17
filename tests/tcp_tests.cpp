#include "tcp.hpp"

#include <cmath>
#include <iostream>
#include <string>

namespace {

int g_failed = 0;
int g_total  = 0;

void check(bool ok, const std::string& name, const std::string& detail = "") {
    ++g_total;
    if (ok) {
        std::cout << "[  OK  ] " << name << "\n";
    } else {
        ++g_failed;
        std::cout << "[ FAIL ] " << name << (detail.empty() ? "" : ("  -- " + detail)) << "\n";
    }
}

void check_near(double got, double want, double eps, const std::string& name) {
    check(std::fabs(got - want) <= eps, name,
          "получено " + std::to_string(got) + ", ожидалось " + std::to_string(want));
}

std::string data(const std::string& file) { return std::string(TCP_DATA_DIR) + "/" + file; }

std::string join(const std::vector<std::string>& v) {
    std::string s;
    for (std::size_t i = 0; i < v.size(); ++i) { if (i) s += ","; s += v[i]; }
    return s;
}

// Тест 1. Загрузка JSON: размеры, стоимости, маски покрытия
void test_load_json() {
    tcp::Suite s = tcp::load(data("author_example.json"));
    check(s.n() == 5,  "load_json: число тестов = 5");
    check(s.m() == 8,  "load_json: число сущностей = 8");
    check(s.faults.size() == 4, "load_json: число дефектов = 4");
    check(s.tests[1].id == "T2" && std::fabs(s.tests[1].cost - 3.0) < 1e-12,
          "load_json: стоимость T2 = 3.0");
    check(s.tests[0].covers == std::vector<bool>({1, 1, 1, 0, 0, 0, 0, 0}),
          "load_json: маска покрытия T1");
}

// Тест 2. Эквивалентность импорта из CSV и из JSON
void test_load_csv_equals_json() {
    tcp::Suite j = tcp::load(data("author_example.json"));
    tcp::Suite c = tcp::load(data("author_example.csv"));
    bool same = (j.n() == c.n() && j.m() == c.m());
    for (std::size_t i = 0; same && i < j.n(); ++i)
        same = (j.tests[i].id == c.tests[i].id) && (j.tests[i].covers == c.tests[i].covers) &&
               std::fabs(j.tests[i].cost - c.tests[i].cost) < 1e-12;
    check(same, "csv == json: матрицы покрытия и стоимости совпадают");
}

// Тест 3. Диагностика некорректных входных данных
void test_invalid_input() {
    bool thrown = false;
    try { tcp::load(data("no_such_file.json")); } catch (const std::exception&) { thrown = true; }
    check(thrown, "load: отсутствующий файл вызывает исключение");

    thrown = false;
    try {
        tcp::Suite s;
        s.entities = {"e1", "e2"};
        s.tests.push_back({"T1", 1.0, 0.0, {true}});   // маска короче числа сущностей
        s.validate();
    } catch (const std::exception&) { thrown = true; }
    check(thrown, "validate: несогласованная длина маски покрытия отвергается");
}

// Тест 4. APFD на эталонном стороннем примере

void test_apfd_reference() {
    tcp::Suite s = tcp::load(data("rothermel2001.json"));
    check_near(tcp::apfd(s, tcp::from_ids(s, {"A", "B", "C", "D", "E"})), 0.50, 1e-9,
               "APFD(A,B,C,D,E) = 0.50 (эталон)");
    check_near(tcp::apfd(s, tcp::from_ids(s, {"E", "D", "C", "B", "A"})), 0.64, 1e-9,
               "APFD(E,D,C,B,A) = 0.64 (эталон)");
    check_near(tcp::apfd(s, tcp::from_ids(s, {"C", "E", "B", "A", "D"})), 0.84, 1e-9,
               "APFD(C,E,B,A,D) = 0.84 (эталон)");
}

// Тест 5. Граничные свойства APFD
void test_apfd_bounds() {
    tcp::Suite s = tcp::load(data("rothermel2001.json"));
    double best = tcp::apfd(s, tcp::from_ids(s, {"C", "E", "A", "B", "D"}));
    double worst = tcp::apfd(s, tcp::from_ids(s, {"A", "B", "D", "C", "E"}));
    check(best > worst, "APFD: лучший порядок строго выше худшего");
    check(best <= 1.0 && worst >= 0.0, "APFD: значения лежат в [0;1]");
}

// Тест 6. Additional greedy воспроизводит оптимальный порядок
void test_additional_greedy() {
    tcp::Suite  s = tcp::load(data("rothermel2001.json"));
    tcp::Params p; p.alpha = 1.0; p.beta = 0.0; p.use_cost = false;
    auto ids = tcp::to_ids(s, tcp::prioritize_greedy(s, p));
    check(join(ids) == "C,E,A,B,D", "additional greedy: порядок C,E,A,B,D", join(ids));
    check_near(tcp::apfd(s, tcp::from_ids(s, ids)), 0.84, 1e-9,
               "additional greedy: APFD = 0.84 (совпадает с оптимальным из статьи)");
}

// Тест 7. Алгоритм возвращает перестановку (полноту и отсутствие повторов)
void test_is_permutation() {
    tcp::Suite  s = tcp::load(data("author_example.json"));
    tcp::Params p; p.alpha = 1.0; p.beta = 0.5; p.use_cost = true;
    auto order = tcp::prioritize_greedy(s, p);
    auto sorted = order;
    std::sort(sorted.begin(), sorted.end());
    bool ok = (sorted.size() == s.n());
    for (std::size_t i = 0; ok && i < sorted.size(); ++i) ok = (sorted[i] == static_cast<int>(i));
    check(ok, "prioritize_greedy: результат -- перестановка индексов тестов");
}

// Тест 8. Учёт стоимости меняет порядок на авторском примере
void test_cost_changes_order() {
    tcp::Suite  s = tcp::load(data("author_example.json"));
    tcp::Params a; a.alpha = 1.0; a.beta = 0.0; a.use_cost = false;  // additional
    tcp::Params h; h.alpha = 1.0; h.beta = 0.5; h.use_cost = true;   // hybrid
    auto ia = tcp::to_ids(s, tcp::prioritize_greedy(s, a));
    auto ih = tcp::to_ids(s, tcp::prioritize_greedy(s, h));
    check(join(ia) == "T2,T4,T1,T3,T5", "additional greedy на авторском примере", join(ia));
    check(join(ih) == "T5,T3,T1,T2,T4", "hybrid (стоимость+история) на авторском примере", join(ih));
    check(tcp::apfd(s, tcp::from_ids(s, ih)) > tcp::apfd(s, tcp::from_ids(s, ia)),
          "hybrid даёт APFD выше, чем additional greedy");
}

// Тест 9. Детерминированность разрешения ничьих
void test_deterministic_ties() {
    tcp::Suite  s = tcp::load(data("author_example.json"));
    tcp::Params p; p.alpha = 1.0; p.beta = 0.5; p.use_cost = true;
    auto r1 = tcp::prioritize_greedy(s, p);
    auto r2 = tcp::prioritize_greedy(s, p);
    check(r1 == r2, "повторный запуск даёт тот же порядок (детерминированность)");
}

// Тест 10. Монотонность кривой покрытия
void test_coverage_monotone() {
    tcp::Suite s = tcp::load(data("author_example.json"));
    auto order = tcp::prioritize_greedy(s, tcp::Params{});
    bool ok = true;
    for (std::size_t k = 1; k < s.n(); ++k)
        ok = ok && tcp::coverage_after(s, order, k) <= tcp::coverage_after(s, order, k + 1) + 1e-12;
    check(ok, "coverage_after: неубывающая функция от k");
    check_near(tcp::coverage_after(s, order, s.n()), 1.0, 1e-12,
               "coverage_after: полный набор покрывает все сущности");
}

// Тест 11. APFD_c при равных стоимостях согласован с APFD
void test_apfd_c_equal_costs() {
    tcp::Suite s = tcp::load(data("rothermel2001.json")); // все стоимости = 1
    auto order = tcp::from_ids(s, {"C", "E", "B", "A", "D"});
    check_near(tcp::apfd_c(s, order), tcp::apfd(s, order), 1e-9,
               "APFD_c = APFD при единичных стоимостях и весах дефектов");
}

} // namespace

int main() {
    test_load_json();
    test_load_csv_equals_json();
    test_invalid_input();
    test_apfd_reference();
    test_apfd_bounds();
    test_additional_greedy();
    test_is_permutation();
    test_cost_changes_order();
    test_deterministic_ties();
    test_coverage_monotone();
    test_apfd_c_equal_costs();

    std::cout << "\nИтого: " << (g_total - g_failed) << " / " << g_total << " проверок пройдено\n";
    return g_failed == 0 ? 0 : 1;
}
