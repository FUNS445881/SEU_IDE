import sys
from PySide6.QtWidgets import QApplication
from my_ide.windows.main_window import MainWindow

def main():
    """
    程序入口
    """
    app = QApplication(sys.argv)
    window = MainWindow()
    sys.exit(app.exec())

if __name__ == "__main__":
    main()