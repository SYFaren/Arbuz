# НДС 20% — пример функции плагина.
# В ячейке: =VAT(100)  →  20

import arbuz


@arbuz.function(
    "VAT",
    syntax="VAT(amount)",
    help_ru="НДС 20% от суммы",
    help_en="20% VAT of amount",
)
def vat(amount):
    nums = arbuz.numbers(amount)
    return (nums[0] if nums else 0) * 0.20
