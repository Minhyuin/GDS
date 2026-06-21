import sys

from PySide6.QtWidgets import QApplication

from .modules.gui import MainWindow


def main():
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    return app.exec()
