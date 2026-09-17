import argparse
import json
import subprocess
import sys

def run_tcp(exe, args):
    proc = subprocess.run([exe] + args, capture_output=True, text=True)
    if proc.returncode != 0:
        raise RuntimeError(f"tcp завершился с кодом {proc.returncode}: {proc.stderr.strip()}")
    return json.loads(proc.stdout)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", required=True, help="путь к исполняемому файлу tcp")
    ap.add_argument("--input", required=True, help="входной файл набора тестов")
    ap.add_argument("--expected", required=True, help="файл с эталонным выходом")
    a = ap.parse_args()

    expected = json.load(open(a.expected, encoding="utf-8"))
    eps = expected.get("tolerance", 1e-9)
    failures = []

    # 1. Значения APFD для порядков, опубликованных в источнике
    for case in expected["cases"]:
        order = ",".join(case["order"])
        got = run_tcp(a.exe, ["--input", a.input, "--order", order, "--json"])
        if abs(got["apfd"] - case["apfd"]) > eps:
            failures.append(f"APFD({order}): получено {got['apfd']}, эталон {case['apfd']}")
        else:
            print(f"OK  APFD({order}) = {got['apfd']:.4f}")

    # 2. Порядок и метрика, которые должен построить additional greedy
    ref = expected["additional_greedy"]
    got = run_tcp(a.exe, ["--input", a.input, "--strategy", "additional", "--json"])
    if got["order"] != ref["order"]:
        failures.append(f"порядок additional greedy: получено {got['order']}, эталон {ref['order']}")
    elif abs(got["apfd"] - ref["apfd"]) > eps:
        failures.append(f"APFD additional greedy: получено {got['apfd']}, эталон {ref['apfd']}")
    else:
        print(f"OK  additional greedy = {'-'.join(got['order'])}, APFD = {got['apfd']:.4f}")

    if failures:
        print("\nРасхождения с эталоном:", file=sys.stderr)
        for f in failures:
            print("  " + f, file=sys.stderr)
        return 1
    print("\nВсе значения совпали с эталонным выходом.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
