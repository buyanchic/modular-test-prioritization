# tcp — система приоритизации модульных тестов

Практическое задание №2 по дисциплине «Методы тестирования программного
обеспечения». СПбПУ, ИКНК, ВШПИ, 2026.
Смирнова Я. А., гр. з5130903/40002.

Программа строит порядок выполнения модульных тестов, максимизирующий
скорость выявления дефектов, и оценивает качество порядка метриками
APFD и APFD_c.

## Сборка и запуск (Linux / macOS / Windows)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Требуется только компилятор C++17 и CMake ≥ 3.16. Внешних библиотек нет.
Python 3 нужен лишь для приёмочного теста `reference` (стандартная
библиотека, см. `requirements.txt`).

## Примеры использования

```bash
# классический additional greedy на эталонном стороннем примере
./build/tcp --input data/rothermel2001.json --strategy additional

# оценка заданного порядка (сверка с опубликованными значениями APFD)
./build/tcp --input data/rothermel2001.json --order C,E,B,A,D --json

# гибридная эвристика с учётом стоимости и истории, с протоколом шагов
./build/tcp --input data/author_example.json --strategy hybrid --beta 0.5 --cost --trace

# импорт того же примера из CSV
./build/tcp --input data/author_example.csv --strategy hybrid

python3 scripts/check_reference.py --exe build/tcp \
            --input data/rothermel2001.json \
            --expected data/rothermel2001.expected.json
```

## Структура проекта

```
CMakeLists.txt              сборка и регистрация тестов в CTest
requirements.txt            зависимости вспомогательных скриптов
src/tcp.hpp                 ядро: модель, загрузка CSV/JSON, алгоритмы, метрики
src/main.cpp                консольный интерфейс
tests/tcp_tests.cpp         модульные тесты ядра (11 групп проверок)
scripts/check_reference.py  приёмочная сверка с эталонным выходом
data/rothermel2001.json     эталонный вход (сторонний пример, IEEE TSE 2001)
data/rothermel2001.expected.json  эталонный выход
data/author_example.json    авторский иллюстративный пример
data/author_example.csv     он же в формате CSV
```

## Форматы входных данных

Размерность задачи не фиксирована: число тестов и покрываемых сущностей
определяется содержимым файла.

**JSON**

```json
{
  "name": "suite",
  "entities": ["e1", "e2"],
  "tests":  [{"id": "T1", "cost": 1.0, "history": 0.1, "covers": ["e1"]}],
  "faults": [{"id": "F1", "detected_by": ["T1"], "severity": 1.0}]
}
```

Секция `faults` необязательна; без неё метрики APFD/APFD_c не вычисляются.

**CSV**

```
test,cost,history,e1,e2
T1,1.0,0.10,1,0
```

