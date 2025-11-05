from PySide6.QtWidgets import QWidget, QVBoxLayout,QButtonGroup,QToolButton
from PySide6.QtGui import QIcon
from PySide6.QtCore import Signal, QSize

class ActivityBar(QWidget):
    view_changed = Signal(str)  # 信号：视图切换

    def __init__(self, parent=None):
        super().__init__(parent)
        # button_group的id映射view_id
        self.view_map = {}

        self.layout = QVBoxLayout(self)
        self.layout.setContentsMargins(5, 5, 5, 5)
        self.layout.setSpacing(10)

        self.button_group = QButtonGroup(self)
        self.button_group.setExclusive(True)

        # 添加按钮
        self.add_button("resource_manager","./my_ide/resources/activity_bar/resource_manager.png","资源管理器")
        self.add_button("search_panel","./my_ide/resources/activity_bar/search_panel.png","搜索")
        
        self.layout.itemAt(0).widget().setChecked(True)  # 默认选中第一个按钮

        self.button_group.idClicked.connect(self._on_button_clicked)

        # 按钮推到顶部
        self.layout.addStretch()

        self.setFixedWidth(50)
    
    def add_button(self, view_id,icon_path,tooltip):
        button = QToolButton(self)
        button.setIcon(QIcon(icon_path))
        button.setIconSize(QSize(24,24))
        button.setToolTip(tooltip)
        button.setCheckable(True)
        id = len(self.view_map)
        self.button_group.addButton(button,id)
        self.view_map[id] = view_id
        self.layout.addWidget(button)
    
    def _on_button_clicked(self, id):
        if id in self.view_map:
            view_id = self.view_map[id]
            self.view_changed.emit(view_id)