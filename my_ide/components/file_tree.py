from PySide6.QtWidgets import QTreeView, QVBoxLayout, QWidget, QLabel, QFileSystemModel,QHBoxLayout, QPushButton
from PySide6.QtCore import QDir, Qt,Signal,QSize
from PySide6.QtGui import QIcon
import os

class FileTreeWidget(QWidget):

    new_file_clicked = Signal()
    new_folder_clicked = Signal()
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.init_ui()
    
    def init_ui(self):
        # 创建主布局
        root_layout = QVBoxLayout(self)
        root_layout.setContentsMargins(0, 0, 0, 0)
        
        #添加标题和按钮
        top_bar_layout = QHBoxLayout()
        top_bar_layout.setContentsMargins(5, 5, 5, 5)
        top_bar_layout.setSpacing(5)

        # 添加标题标签
        title_label = QLabel("文件树")
        title_label.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        top_bar_layout.addWidget(title_label)

        # 【增加】核心：水平弹簧 (将右侧按钮推向最右边)
        top_bar_layout.addStretch() 

        # 【增加】新建文件按钮 (靠右，ICON + Tooltip) 
        self.new_file_button = QPushButton()

        current_dir = os.path.dirname(os.path.abspath(__file__))
        project_root = os.path.dirname(os.path.dirname(current_dir))
        resources_dir = os.path.join(project_root, "resources", "activity_bar")
        
        file_icon_path = os.path.join(resources_dir, "new_file.png")
        folder_icon_path = os.path.join(resources_dir, "new_folder.png")

        print(f"文件图标路径: {file_icon_path}")
        print(f"文件图标存在: {os.path.exists(file_icon_path)}")
        print(f"文件夹图标路径: {folder_icon_path}")
        print(f"文件夹图标存在: {os.path.exists(folder_icon_path)}")

        self.new_file_button.setIcon(QIcon(file_icon_path))
        self.new_file_button.setToolTip("新建文件")
        self.new_file_button.setFixedSize(24, 24)
        self.new_file_button.setStyleSheet("QPushButton { border: none; background: transparent; }")
        top_bar_layout.addWidget(self.new_file_button)

        # 【增加】新建文件夹按钮 (靠右，ICON + Tooltip)
        self.new_folder_button = QPushButton()
        self.new_folder_button.setIcon(QIcon(folder_icon_path))
        self.new_folder_button.setToolTip("新建文件夹")
        self.new_folder_button.setFixedSize(24, 24)
        self.new_folder_button.setStyleSheet("QPushButton { border: none; background: transparent; }")
        top_bar_layout.addWidget(self.new_folder_button)

        self.new_file_button.clicked.connect(self.new_file_clicked.emit)
        self.new_folder_button.clicked.connect(self.new_folder_clicked.emit)

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