from PySide6.QtWidgets import (QWidget, QVBoxLayout,QHBoxLayout, QTabWidget, QPlainTextEdit,QScrollBar,
                               QTableWidget, QTableWidgetItem, QHeaderView)
from PySide6.QtCore import Qt,QCoreApplication,Signal
from PySide6.QtGui import QIcon
from termqt import Terminal
import sys
import logging

class OutputBar(QWidget):
    problem_clicked = Signal(str, int)  # 文件路径，行号

    terminal = None
    terminal_io = None
    def __init__(self, parent=None):
        super().__init__(parent)
        self.init_ui()


    def init_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        self.tabs = QTabWidget()
        layout.addWidget(self.tabs)

        # 终端面板
        logger = logging.getLogger()
        logger.setLevel(logging.DEBUG)
        handler = logging.StreamHandler()
        formatter = logging.Formatter(
            "[%(asctime)s] > "
            "[%(filename)s:%(lineno)d] %(message)s"
        )
        handler.setFormatter(formatter)
        logger.addHandler(handler)
        self.terminal_widget = QWidget()
        terminal_layout = QHBoxLayout()
        self.terminal = Terminal(400, 300, logger=logger)
        self.terminal.set_font()
        self.terminal.maximum_line_history = 2000
        scroll = QScrollBar(Qt.Vertical, self.terminal)
        self.terminal.connect_scroll_bar(scroll)
        print(self.terminal)

        terminal_layout.addWidget(self.terminal)
        terminal_layout.addWidget(scroll)
        terminal_layout.setSpacing(0)
        
        auto_wrap_enabled = False
        bin = "cmd"

        from termqt import TerminalWinptyIO
        self.terminal_io = TerminalWinptyIO(
                self.terminal.row_len,
                self.terminal.col_len,
                bin,
                logger=logger
                )

        self.terminal.enable_auto_wrap(auto_wrap_enabled)
        self.terminal_io.stdout_callback = self.terminal.stdout
        self.terminal.stdin_callback = self.terminal_io.write
        self.terminal.resize_callback = self.terminal_io.resize
        self.terminal_io.spawn()
        self.terminal_widget.setLayout(terminal_layout)

        # 问题面板
        self.problems_panel = QTableWidget()
        self.problems_panel.setColumnCount(4)
        self.problems_panel.setHorizontalHeaderLabels(["描述", "文件", "行", "严重性"])
        self.problems_panel.horizontalHeader().setSectionResizeMode(0, QHeaderView.Stretch)
        self.problems_panel.setEditTriggers(QTableWidget.NoEditTriggers)
        self.problems_panel.setSelectionBehavior(QTableWidget.SelectRows)
        # 问题面板点击事件
        self.problems_panel.cellClicked.connect(self._on_problem_row_clicked)

        # 输出面板
        self.output_panel = QPlainTextEdit()
        self.output_panel.setReadOnly(True)
        self.output_panel.setPlaceholderText("程序输出将显示在这里...")

        self.tabs.addTab(self.terminal_widget, "终端")
        self.tabs.addTab(self.problems_panel, "问题")
        self.tabs.addTab(self.output_panel, "输出")

    def _on_problem_row_clicked(self, row, column):
        """
        当用户点击表格的某一行时，获取该行的文件名和行号，并发射信号
        """
        # 获取第1列(文件)和第2列(行)的数据
        file_item = self.problems_panel.item(row, 1)
        line_item = self.problems_panel.item(row, 2)
        
        if file_item and line_item:
            file_path = file_item.text()
            try:
                line_number = int(line_item.text())
                # 发射信号给 MainWindow
                self.problem_clicked.emit(file_path, line_number)
            except ValueError:
                pass

    # --- 公共方法 ---
    def add_problem(self, description, file, line, severity):
        row_position = self.problems_panel.rowCount()
        self.problems_panel.insertRow(row_position)
        self.problems_panel.setItem(row_position, 0, QTableWidgetItem(description))
        self.problems_panel.setItem(row_position, 1, QTableWidgetItem(file))
        self.problems_panel.setItem(row_position, 2, QTableWidgetItem(str(line)))
        self.problems_panel.setItem(row_position, 3, QTableWidgetItem(severity))

    def clear_problems(self):
        print("清理问题面板")
        self.problems_panel.setRowCount(0)

    def append_output(self, text):
        self.output_panel.appendPlainText(text)

    def clear_all(self):
        self.problems_panel.setRowCount(0)
        self.output_panel.clear()
    
    def clear_output(self):
        self.output_panel.clear()

    def run_with_terminal(self):
        self.terminal_io.write(b"echo Hello from terminal!\r\n")