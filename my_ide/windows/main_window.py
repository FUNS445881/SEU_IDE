import sys
import os
from PySide6.QtWidgets import QApplication,QMainWindow,QPlainTextEdit,QStatusBar,QMenuBar,QFileDialog, QDockWidget
from PySide6.QtGui import QAction
from PySide6.QtCore import Qt
from my_ide.components.file_tree import FileTreeWidget

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
        self._init_file_dock()

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
    
    def _init_file_dock(self):
        """
        初始化文件树停靠窗口
        """
        # 创建停靠窗口
        self.file_dock = QDockWidget("文件", self)
        self.file_dock.setAllowedAreas(Qt.LeftDockWidgetArea )
        
        # 禁用关闭按钮，只允许浮动和移动
        self.file_dock.setFeatures(QDockWidget.NoDockWidgetFeatures)
        
        # 创建文件树组件
        self.file_tree = FileTreeWidget(self)
        
        # 设置文件树的根路径为当前工作目录
        current_dir = os.getcwd()
        self.file_tree.set_root_path(current_dir)
        
        # 连接doubleClicked信号到槽函数
        self.file_tree.tree_view.doubleClicked.connect(self._on_file_double_clicked)
        
        # 将文件树组件添加到停靠窗口
        self.file_dock.setWidget(self.file_tree)
        self.addDockWidget(Qt.LeftDockWidgetArea, self.file_dock)    

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
        file_path = self.file_tree.model.filePath(index)
        
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