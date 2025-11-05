import sys
import os
from PySide6.QtWidgets import QApplication,QMainWindow,QPlainTextEdit,QFileDialog, QDockWidget, QHBoxLayout, QStackedWidget,QWidget
from PySide6.QtGui import QAction
from PySide6.QtCore import Qt
from my_ide.components.file_tree import FileTreeWidget
from my_ide.components.activity_bar import ActivityBar

# --- B. (可选但推荐) 创建一个占位符搜索视图 ---
# 这样我们的代码结构更完整，即使现在搜索功能还没做
class SearchView(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        # 暂时可以是一个空的QWidget，之后再实现具体功能
        # from PySide6.QtWidgets import QLabel, QVBoxLayout
        # layout = QVBoxLayout(self)
        # layout.addWidget(QLabel("搜索功能待实现"))

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.current_file_path = None  # 跟踪当前打开的文件路径
        self.init_ui()
    
    def init_ui(self):
        """
        初始化UI界面
        """
        self.setWindowTitle("My IDE")
        self.resize(1280,720)
        
        self._init_editor()
        self._init_status_bar()
        self._init_menu_bar()
        self._init_sidebar()

        self.show()

    def _init_editor(self):
        """
        初始化代码编辑器
        """
        self.editor = QPlainTextEdit(self)
        self.setCentralWidget(self.editor)

    def _init_status_bar(self):
        """
        初始化状态栏
        """
        statusBar = self.statusBar()
        statusBar.showMessage("Ready",3000)

    def _init_menu_bar(self):
        """
        初始化菜单栏
        """
        menuBar = self.menuBar()
        file_menu = menuBar.addMenu("&File")

        # --- 创建 "File" 菜单下的动作 (Actions) ---
        # 1. 打开文件的动作
        open_action = QAction("&Open", self)
        open_action.setStatusTip("Open a file")
        open_action.triggered.connect(self._on_file_open) 

        # 2. 保存文件的动作
        save_action = QAction("&Save", self)
        save_action.setStatusTip("Save the current file")
        save_action.triggered.connect(self._on_file_save)

        # 3. 退出的动作
        exit_action = QAction("&Exit", self)
        exit_action.setStatusTip("Exit the application")
        exit_action.triggered.connect(self.close)

        # --- 将动作添加到 "File" 菜单中 ---
        file_menu.addAction(open_action)
        file_menu.addAction(save_action)
        file_menu.addSeparator() # 添加一条分割线
        file_menu.addAction(exit_action)
    
    def _init_sidebar(self):
        self.activity_bar = ActivityBar(self)
        self.stacked_widget = QStackedWidget(self)

        self.file_tree_view = FileTreeWidget(self)
        self.search_panel = SearchView(self)

        self.views = {
            "resource_manager": self.file_tree_view,
            "search_panel": self.search_panel
        }
        self.stacked_widget.addWidget(self.views["resource_manager"])
        self.stacked_widget.addWidget(self.views["search_panel"])

        sidebar_container = QWidget()
        sidebar_layout = QHBoxLayout(sidebar_container)
        sidebar_layout.setContentsMargins(0, 0, 0, 0)
        sidebar_layout.setSpacing(0)

        sidebar_layout.addWidget(self.activity_bar)
        sidebar_layout.addWidget(self.stacked_widget)

        # 放入QDockWidget
        self.sidebar_dock = QDockWidget("Sidebar", self)
        self.sidebar_dock.setWidget(sidebar_container)
        self.sidebar_dock.setTitleBarWidget(QWidget())  # 隐藏标题栏
        self.addDockWidget(Qt.LeftDockWidgetArea, self.sidebar_dock)

        self.activity_bar.view_changed.connect(self.switch_sidebar_view)

        current_dir = os.getcwd()
        self.file_tree_view.set_root_path(current_dir)
        self.file_tree_view.tree_view.doubleClicked.connect(self._on_file_double_clicked)

    def switch_sidebar_view(self, view_id):
        if view_id in self.views:
            widget = self.views[view_id]
            self.stacked_widget.setCurrentWidget(widget)

    def _on_file_open(self):
        """处理文件打开动作的槽函数"""
        file_path, _ = QFileDialog.getOpenFileName(
            self,  # 父窗口
            "打开文件",  # 对话框标题
            "",  # 默认目录
            "所有文件 (*);;文本文件 (*.txt);;Python文件 (*.py);;JavaScript文件 (*.js);;HTML文件 (*.html);;CSS文件 (*.css)"  # 文件过滤器
        )
        
        if file_path:
            self._open_file(file_path)
    
    def _open_file(self, file_path):
        """
        打开指定路径的文件
        参数: file_path - 要打开的文件路径
        """
        try:
            with open(file_path, 'r', encoding='utf-8') as file:
                content = file.read()
                self.editor.setPlainText(content)
                self.current_file_path = file_path
                self.setWindowTitle(f"My IDE - {file_path}")
                self.statusBar().showMessage(f"已打开文件: {file_path}", 3000)
        except Exception as e:
            self.statusBar().showMessage(f"打开文件失败: {str(e)}", 3000)
            print(f"Error opening file: {e}")

    def _on_file_double_clicked(self, index):
        """
        处理文件树中文件双击事件
        参数: index - 文件树中双击的项的索引
        """
        # 从模型中获取文件路径
        file_path = self.file_tree_view.model.filePath(index)
        
        # 检查是否是文件（非目录）
        if os.path.isfile(file_path):
            self._open_file(file_path)

    def _on_file_save(self):
        """处理文件保存动作的槽函数"""
        # 如果已有文件路径，则直接保存
        if self.current_file_path:
            try:
                with open(self.current_file_path, 'w', encoding='utf-8') as file:
                    file.write(self.editor.toPlainText())
                self.statusBar().showMessage(f"文件已保存: {self.current_file_path}", 3000)
            except Exception as e:
                self.statusBar().showMessage(f"保存文件失败: {str(e)}", 3000)
                print(f"Error saving file: {e}")
        else:
            # 如果没有文件路径，则调用另存为对话框
            file_path, _ = QFileDialog.getSaveFileName(
                self,  # 父窗口
                "保存文件",  # 对话框标题
                "",  # 默认目录
                "所有文件 (*);;文本文件 (*.txt);;Python文件 (*.py);;JavaScript文件 (*.js);;HTML文件 (*.html);;CSS文件 (*.css)"  # 文件过滤器
            )
            
            if file_path:
                try:
                    with open(file_path, 'w', encoding='utf-8') as file:
                        file.write(self.editor.toPlainText())
                    self.current_file_path = file_path
                    self.setWindowTitle(f"My IDE - {file_path}")
                    self.statusBar().showMessage(f"文件已保存: {file_path}", 3000)
                except Exception as e:
                    self.statusBar().showMessage(f"保存文件失败: {str(e)}", 3000)
                    print(f"Error saving file: {e}")
    

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    sys.exit(app.exec())