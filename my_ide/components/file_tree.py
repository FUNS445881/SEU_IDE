from PySide6.QtWidgets import QTreeView, QVBoxLayout, QWidget, QLabel, QFileSystemModel,QHBoxLayout, QPushButton
from PySide6.QtCore import QDir, Qt,Signal,QSize
from PySide6.QtGui import QIcon
import os

class FileTreeWidget(QWidget):

    new_file_clicked = Signal()
    new_folder_clicked = Signal()
    delete_button_clicked = Signal()
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.init_ui()
    
    def init_ui(self):
        root_layout = QVBoxLayout(self)
        root_layout.setContentsMargins(0, 0, 0, 0)
        
        # 标题和按钮
        top_bar_layout = QHBoxLayout()
        top_bar_layout.setContentsMargins(5, 5, 5, 5)
        top_bar_layout.setSpacing(5)


        title_label = QLabel("文件树")
        title_label.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        top_bar_layout.addWidget(title_label)

        # 挤向右侧
        top_bar_layout.addStretch() 
        file_icon_path = "./my_ide/resources/activity_bar/new_file.png"
        folder_icon_path = "./my_ide/resources/activity_bar/new_folder.png"
        delete_icon_path = "./my_ide/resources/activity_bar/delete.png"

        self.new_file_button = QPushButton()
        self.new_file_button.setIcon(QIcon(file_icon_path))
        self.new_file_button.setToolTip("新建文件")
        self.new_file_button.setFixedSize(24, 24)
        self.new_file_button.setStyleSheet("QPushButton { border: none; background: transparent; }")
        top_bar_layout.addWidget(self.new_file_button)

        self.new_folder_button = QPushButton()
        self.new_folder_button.setIcon(QIcon(folder_icon_path))
        self.new_folder_button.setToolTip("新建文件夹")
        self.new_folder_button.setFixedSize(24, 24)
        self.new_folder_button.setStyleSheet("QPushButton { border: none; background: transparent; }")
        top_bar_layout.addWidget(self.new_folder_button)

        self.delete_button = QPushButton()
        self.delete_button.setIcon(QIcon(delete_icon_path))
        self.delete_button.setToolTip("删除文件或文件夹")
        self.delete_button.setFixedSize(24, 24)
        self.delete_button.setStyleSheet("QPushButton { border: none; background: transparent; }")
        top_bar_layout.addWidget(self.delete_button)

        self.new_file_button.clicked.connect(self.new_file_clicked.emit)
        self.new_folder_button.clicked.connect(self.new_folder_clicked.emit)
        self.delete_button.clicked.connect(self.delete_button_clicked.emit)

        # 创建文件系统模型
        self.model = QFileSystemModel()
        # 设置过滤器，只显示目录和文件
        self.model.setFilter(QDir.AllDirs | QDir.Files | QDir.NoDotAndDotDot)
        # 设置名称过滤器模式为通配符
        self.model.setNameFilterDisables(False)
        
        # 设置根路径为用户主目录
        self.model.setRootPath(QDir.homePath())
        
        # 创建树视图
        self.tree_view = QTreeView()
        self.tree_view.setModel(self.model)
        self.tree_view.setExpandsOnDoubleClick(False)
        self.tree_view.clicked.connect(self._on_item_clicked)
        self.tree_view.setItemsExpandable(True)
        self.tree_view.setAnimated(True)
        # 设置树视图属性
        self.tree_view.setHeaderHidden(True)  # 隐藏标题行
        # 只显示名称列，隐藏其他列（大小、类型、修改日期等）
        for column in range(1, self.model.columnCount()):
            self.tree_view.hideColumn(column)
        
        # 设置根索引为模型的根路径
        self.tree_view.setRootIndex(self.model.index(QDir.homePath()))
        
        # 允许选择多个项目
        self.tree_view.setSelectionMode(QTreeView.SingleSelection)
        
        # 【增加】将标题和按钮行添加到顶层布局
        root_layout.addLayout(top_bar_layout)

        # 添加到布局
        root_layout.addWidget(self.tree_view)
    
    def set_root_path(self, path):
        """设置文件树的根路径"""
        if QDir(path).exists():
            self.model.setRootPath(path)
            self.tree_view.setRootIndex(self.model.index(path))
            return True
        return False
    
    def get_selected_file_path(self):
        """获取当前选中的文件路径"""
        selected_index = self.tree_view.currentIndex()
        if selected_index.isValid():
            return self.model.filePath(selected_index)
        return None
    
    def expand_to_path(self, path):
        """展开到指定的路径"""
        if QDir(path).exists():
            index = self.model.index(path)
            if index.isValid():
                self.tree_view.expand(index)
                self.tree_view.setCurrentIndex(index)
                return True
        return False

    def _on_item_clicked(self, index):
        if self.model.isDir(index):
            if self.tree_view.isExpanded(index):
                self.tree_view.collapse(index)
            else:
                self.tree_view.expand(index)