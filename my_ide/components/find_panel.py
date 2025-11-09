from PySide6.QtWidgets import (QWidget, QHBoxLayout, QVBoxLayout, QLineEdit, 
                               QCheckBox, QPushButton, QLabel, QSpacerItem, QSizePolicy)
from PySide6.QtCore import Signal, Qt
from PySide6.QtGui import QIcon

# 查找面板
class FindPanel(QWidget):
    # signals
    find_triggered = Signal(str,bool,bool) # term, case_sensitive, whole
    find_next_triggered = Signal()
    find_previous_triggered = Signal()
    replace_all_triggered = Signal(str) # replacement
    closed = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self._init_ui()
        self._connect_signals()
        
        self.setStyleSheet("""
            FindPanel {
                background-color: #3c3c3c;
                border: 1px solid #555;
            }
            QLabel, QCheckBox {
                color: #ddd;
            }
        """)

    def _init_ui(self):
        self.root_layout = QVBoxLayout(self)
        self.root_layout.setContentsMargins(5, 5, 5, 5)
        self.root_layout.setSpacing(5)

        layout = QHBoxLayout(self)
        self.search_input = QLineEdit(self)
        self.search_input.setPlaceholderText("Find...")
        self.search_input.setMinimumWidth(200)
        
        self.results_label = QLabel("0/0", self)

        self.prev_button = QPushButton("↑", self)
        self.prev_button.setToolTip("Previous match (Shift+Enter)")
        self.prev_button.setFixedSize(24, 24)

        self.next_button = QPushButton("↓", self)
        self.next_button.setToolTip("Next match (Enter)")
        self.next_button.setFixedSize(24, 24)
        
        layout.addWidget(self.search_input)
        layout.addWidget(self.results_label)
        layout.addWidget(self.prev_button)
        layout.addWidget(self.next_button)

        replace_layout = QHBoxLayout()
        self.replace_input = QLineEdit(self)
        self.replace_input.setPlaceholderText("Replace with...")
        self.replace_input.setMinimumWidth(150) # 调整宽度
        self.replace_all_button = QPushButton("Replace All", self)
        self.replace_all_button.setToolTip("Replace all matches")
        
        replace_layout.addWidget(self.replace_input)
        replace_layout.addWidget(self.replace_all_button)

        options_layout = QHBoxLayout()
        self.case_checkbox = QCheckBox("Case Sensitive", self)
        self.whole_word_checkbox = QCheckBox("Whole Word", self)
        self.close_button = QPushButton("×", self)
        self.close_button.setToolTip("Close")
        self.close_button.setFixedSize(24, 24)
        
        options_layout.addWidget(self.case_checkbox)
        options_layout.addWidget(self.whole_word_checkbox)
        options_layout.addStretch() # 把关闭按钮推到最右边
        options_layout.addWidget(self.close_button)

        # 4. 将所有行布局添加到根布局中
        self.root_layout.addLayout(layout)
        self.root_layout.addLayout(replace_layout)
        self.root_layout.addLayout(options_layout)

    
    def _connect_signals(self):
        self.search_input.returnPressed.connect(self.next_button.click)
        self.search_input.textChanged.connect(self._on_search)
        
        self.case_checkbox.stateChanged.connect(self._on_search)
        self.whole_word_checkbox.stateChanged.connect(self._on_search)
        
        self.next_button.clicked.connect(self.find_next_triggered)
        self.prev_button.clicked.connect(self.find_previous_triggered)
        self.close_button.clicked.connect(self.closed)
        self.close_button.clicked.connect(self.hide)

        self.replace_all_button.clicked.connect(self._on_replace_all)
        self.replace_input.returnPressed.connect(self._on_replace_all)

    def _on_search(self):
        term = self.search_input.text()
        case_sensitive = self.case_checkbox.isChecked()
        whole_word = self.whole_word_checkbox.isChecked()
        self.find_triggered.emit(term, case_sensitive, whole_word)
        print("搜索信号触发:", term, case_sensitive, whole_word)

    def update_results_label(self, current, total):
        if total == 0:
            self.results_label.setText("No results")
        else:
            self.results_label.setText(f"{current + 1}/{total}")
            
    def keyPressEvent(self, event):
        if event.key() == Qt.Key_Escape:
            self.closed.emit()
            self.hide()
        # 上下箭头切换结果
        elif event.key() == Qt.Key_Up:
            self.prev_button.click()
        elif event.key() == Qt.Key_Down:
            self.next_button.click()
        else:
            super().keyPressEvent(event)
    
    def _on_replace_all(self):
        term = self.search_input.text()
        replacement = self.replace_input.text()
        if term:
            self.replace_all_triggered.emit(replacement)