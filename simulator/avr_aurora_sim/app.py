import sys
from PySide6.QtWidgets import QApplication
from .main_window import MainWindow
def main():
    app=QApplication(sys.argv); w=MainWindow(); w.resize(1100,720); w.show(); return app.exec()
