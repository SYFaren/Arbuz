# Набор приёмов: диапазоны, листы, события, команды.

import arbuz


@arbuz.function(
    "PYSUM",
    syntax="PYSUM(range)",
    help_ru="Сумма чисел диапазона (пример плагина)",
    help_en="Sum of numbers in a range (plugin example)",
)
def pysum(values):
    nums = arbuz.numbers(values)
    return sum(nums) if nums else 0


@arbuz.function(
    "PYAVG",
    syntax="PYAVG(range)",
    help_ru="Среднее чисел диапазона",
    help_en="Average of numbers in a range",
)
def pyavg(values):
    nums = arbuz.numbers(values)
    if not nums:
        return arbuz.error("#DIV/0!")
    return sum(nums) / len(nums)


@arbuz.command("tools.fill_demo", "Демо: числа 1..5 в A1:A5", "Demo: 1..5 into A1:A5")
def fill_demo():
    arbuz.set_range("A1:A5", [1, 2, 3, 4, 5])


@arbuz.command("tools.stamp", "Записать адрес выделения в ячейку", "Write selection address into cell")
def stamp():
    arbuz.set(arbuz.current_cell(), arbuz.selection())


@arbuz.on("app_start")
def _started(_msg):
    pass
