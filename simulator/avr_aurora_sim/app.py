def main() -> int:
    import sys
    from PySide6.QtWidgets import QApplication
    from .main_window import MainWindow

    app = QApplication(sys.argv)
    window = MainWindow()
    window.resize(1100, 720)
    window.show()
    return app.exec()
