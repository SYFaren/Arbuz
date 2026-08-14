# Команда в меню «Плагины»: пишет приветствие в текущую ячейку A1.

import arbuz


@arbuz.command("hello.fill", "Вставить приветствие в A1", "Insert greeting into A1")
def hello():
    arbuz.set("A1", "Какой же SYFaren прекрасный")


@arbuz.on("app_start")
def started(_msg):
    pass
