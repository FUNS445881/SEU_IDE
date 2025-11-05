from PySide6.QtWidgets import QTreeView, QVBoxLayout, QWidget, QLabel, QFileSystemModel
from PySide6.QtCore import QDir, Qt

class FileTreeWidget(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.init_ui()
    
    def init_ui(self):
        # 创建主布局
        layout = QVBoxLayout(self)
        
        # 添加标题标签
        title_label = QLabel("文件树")
        title_label.setAlignment(Qt.AlignCenter)
        layout.addWidget(title_label)
        
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
        
        # 添加到布局
        layout.addWidget(self.tree_view)
        
        # 设置布局边距为0
        layout.setContentsMargins(0, 0, 0, 0)
    
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