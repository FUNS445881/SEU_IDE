from PySide6.QtWidgets import QWidget,QHBoxLayout,QLineEdit,QPushButton

class FindPanel(QWidget):
    find_clicked = Signal(str,bool,bool) # 查找框内容，是否匹配大小写，是否全字匹配
    def __init__(self, parent=None):
        super().__init__(parent)
        # 模仿现代IDE Ctrl+F的弹出界面,悬浮在右上角，并且有大小写按钮，全字匹配按钮，上一个按钮，下一个按钮
        self.setWindowFlags(Qt.Window | Qt.WindowStaysOnTopHint)
        self.setFixedSize(100, 50)
        layout = QHBoxLayout(self)
        # 输入框
        self.find_input = QLineEdit(self)