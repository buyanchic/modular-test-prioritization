#include "tcp.hpp"

#include <iomanip>
#include <iostream>

namespace {

void usage() {
    std::cout <<
        "Использование: tcp --input <файл.json|файл.csv> [опции]\n"
        "Опции:\n"
        "  --strategy <s>   original | total | additional | hybrid  (по умолчанию additional)\n"
        "  --alpha <a>      вес приращения покрытия для hybrid (по умолчанию 1.0)\n"
        "  --beta <b>       вес истории отказов для hybrid       (по умолчанию 0.5)\n"
        "  --cost           учитывать стоимость тестов (для hybrid включено по умолчанию)\n"
        "  --no-cost        не учитывать стоимость тестов\n"
        "  --order <спис.>  оценить заданный порядок, напр. --order A,B,C,D,E\n"
        "  --trace          вывести пошаговое выполнение алгоритма\n"
        "  --json           вывод результата в формате JSON (для автопроверки)\n"
        "  --help           эта справка\n";
}

/// Печать протокола пошагового выполнения в виде таблицы.
void print_trace(const std::vector<tcp::TraceStep>& trace) {
    std::cout << "\nПошаговое выполнение:\n";
    for (const auto& st : trace) {
        std::cout << "  Итерация " << st.iteration;
        if (st.reset) std::cout << "  [сброс множества непокрытых сущностей]";
        std::cout << "\n";
        for (std::size_t i = 0; i < st.ids.size(); ++i)
            std::cout << "    " << std::setw(6) << std::left << st.ids[i]
                      << " D=" << std::setw(3) << st.delta[i]
                      << " g=" << std::fixed << std::setprecision(4) << st.score[i] << "\n";
        std::cout << "    --> выбран " << st.chosen << "\n";
    }
}

std::string join(const std::vector<std::string>& v, const std::string& sep) {
    std::string out;
    for (std::size_t i = 0; i < v.size(); ++i) { if (i) out += sep; out += v[i]; }
    return out;
}

} // namespace

int main(int argc, char** argv) try {
    std::string input, strategy = "additional", order_arg;
    tcp::Params params;
    params.beta     = 0.5;
    params.use_cost = true;
    bool trace_on = false, json_out = false, cost_set = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) throw std::runtime_error("отсутствует значение для опции " + a);
            return argv[++i];
        };
        if      (a == "--input")    input     = next();
        else if (a == "--strategy") strategy  = next();
        else if (a == "--alpha")    params.alpha = std::stod(next());
        else if (a == "--beta")     params.beta  = std::stod(next());
        else if (a == "--cost")   { params.use_cost = true;  cost_set = true; }
        else if (a == "--no-cost"){ params.use_cost = false; cost_set = true; }
        else if (a == "--order")    order_arg = next();
        else if (a == "--trace")    trace_on  = true;
        else if (a == "--json")     json_out  = true;
        else if (a == "--help")   { usage(); return 0; }
        else throw std::runtime_error("неизвестная опция: " + a);
    }
    if (input.empty()) { usage(); return 2; }

    tcp::Suite suite = tcp::load(input);

    // Стратегии original/total/additional -- частные случаи общей схемы,
    // поэтому параметры для них фиксируются здесь явно.
    std::vector<tcp::TraceStep> trace;
    std::vector<int>            order;
    if (!order_arg.empty()) {
        order = tcp::from_ids(suite, tcp::split(order_arg, ','));
        strategy = "given";
    } else if (strategy == "original") {
        order = tcp::prioritize_original(suite);
    } else if (strategy == "total") {
        order = tcp::prioritize_total(suite);
    } else if (strategy == "additional") {
        tcp::Params p; p.alpha = 1.0; p.beta = 0.0; p.use_cost = false;
        order = tcp::prioritize_greedy(suite, p, trace_on ? &trace : nullptr);
    } else if (strategy == "hybrid") {
        if (!cost_set) params.use_cost = true;
        order = tcp::prioritize_greedy(suite, params, trace_on ? &trace : nullptr);
    } else {
        throw std::runtime_error("неизвестная стратегия: " + strategy);
    }

    const std::vector<std::string> ids = tcp::to_ids(suite, order);
    const bool have_faults = !suite.faults.empty();
    const double a  = have_faults ? tcp::apfd(suite, order)   : 0.0;
    const double ac = have_faults ? tcp::apfd_c(suite, order) : 0.0;

    if (json_out) {
        std::cout << "{\n"
                  << "  \"suite\": \"" << suite.name << "\",\n"
                  << "  \"strategy\": \"" << strategy << "\",\n"
                  << "  \"order\": [\"" << join(ids, "\", \"") << "\"],\n"
                  << std::fixed << std::setprecision(6)
                  << "  \"apfd\": "   << a  << ",\n"
                  << "  \"apfd_c\": " << ac << "\n}\n";
    } else {
        std::cout << "Набор:      " << suite.name << "  (тестов: " << suite.n()
                  << ", сущностей: " << suite.m()
                  << ", дефектов: " << suite.faults.size() << ")\n"
                  << "Стратегия:  " << strategy << "\n"
                  << "Порядок:    " << join(ids, " -> ") << "\n";
        if (have_faults)
            std::cout << std::fixed << std::setprecision(4)
                      << "APFD:       " << a  << "\n"
                      << "APFD_c:     " << ac << "\n";
        std::cout << "Покрытие после первых k тестов:\n";
        for (std::size_t k = 1; k <= suite.n(); ++k)
            std::cout << "  k=" << k << ": " << std::fixed << std::setprecision(3)
                      << tcp::coverage_after(suite, order, k) << "\n";
        if (trace_on && !trace.empty()) print_trace(trace);
    }
    return 0;
} catch (const std::exception& e) {
    std::cerr << "Ошибка: " << e.what() << "\n";
    return 1;
}
