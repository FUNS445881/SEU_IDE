import sys
import os
import qdarkstyle
from PySide6.QtWidgets import (QApplication,QMainWindow,QPlainTextEdit,QFileDialog, QDockWidget, 
                                QHBoxLayout, QStackedWidget, QWidget,QDialog,QInputDialog,QLineEdit)
from PySide6.QtGui import QAction,QTextCursor,QTextOption,QResizeEvent
from PySide6.QtCore import Qt,QEvent,QTimer
from my_ide.components.file_tree import FileTreeWidget
from my_ide.components.activity_bar import ActivityBar
from my_ide.components.menu_bar import MenuBar
from my_ide.components.search_panel import SearchPanel
from my_ide.components.find_panel import FindPanel
from my_ide.controllers.editor_controller import EditorController

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.current_file_path = None  # 跟踪当前打开的文件路径
        
        self.init_ui()
        self._init_find_panel()
        self._init_controller()

        # 安装事件过滤器
        QApplication.instance().installEventFilter(self)
        self.default_palette = QApplication.instance().palette()
        self.is_dark_theme = False

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
        self.custom_menu_bar = MenuBar(self)
        self.setMenuBar(self.custom_menu_bar)
        self.custom_menu_bar.action_triggered.connect(self._handle_menu_action)
        
    def _init_sidebar(self):
        current_dir = os.getcwd()

        self.activity_bar = ActivityBar(self)
        self.stacked_widget = QStackedWidget(self)

        self.file_tree_view = FileTreeWidget(self)
        self.search_panel_view = SearchPanel(current_dir,self)

        self.views = {
            "resource_manager": self.file_tree_view,
            "search_panel": self.search_panel_view
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

        # file_tree初始化
        self.views["resource_manager"].set_root_path(current_dir)
        self.views["resource_manager"].tree_view.doubleClicked.connect(self._on_file_double_clicked)
        self.views["resource_manager"].new_file_clicked.connect(self._on_new_file)
        self.views["resource_manager"].new_folder_clicked.connect(self._on_new_folder)
        self.views["resource_manager"].delete_button_clicked.connect(self._on_file_delete)
        # search_panel初始化
        self.views["search_panel"].result_clicked.connect(self._on_search_result_clicked)
        self.views["search_panel"].error_found.connect(self._on_search_error_found)
        self.views["search_panel"].search_completed.connect(self._on_search_completed)

    def _init_controller(self):
        """
        初始化各个控制器
        """
        self.editor_controller = EditorController(self.editor)

        self.action_handlers = {
            # File actions 不用controller的原因是因为他们和main_window高度绑定
            "file_open": self._on_file_open,
            "file_open_folder": self._on_file_folder_open,
            "file_new": self._on_new_file,
            "file_new_folder": self._on_new_folder,
            "file_save": self._on_file_save,
            "file_delete": self._on_file_delete,
            "file_exit": self.close,
            # Editor actions
            "edit_undo": self.editor_controller.undo,
            "edit_redo": self.editor_controller.redo,
            "edit_cut": self.editor_controller.cut,
            "edit_copy": self.editor_controller.copy,
            "edit_paste": self.editor_controller.paste,
            "edit_find": self._show_find_dialog,
            "edit_replace":self.find_panel.replace_all_button.click,
            "select_all": self.editor_controller.select_all,
            "zoom_in": self.editor_controller.zoom_in,
            "zoom_out": self.editor_controller.zoom_out,
            "zoom_reset": self.editor_controller.zoom_reset,
            "toggle_word_wrap": self.editor_controller.toggle_word_wrap,
            "toggle_fullscreen": self._on_toggle_fullscreen,
            "toggle_sidebar": self._on_toggle_sidebar,
            "toggle_dark_theme": self._on_toggle_dark_theme,
        }

    def _init_find_panel(self):
        """初始化查找面板"""
        self.find_panel = FindPanel(self.editor) # 以editor为父组件，方便定位
        self.find_panel.hide()

        # 连接信号
        self.find_panel.find_triggered.connect(self._on_find_triggered)
        self.find_panel.find_next_triggered.connect(self._on_find_next)
        self.find_panel.find_previous_triggered.connect(self._on_find_previous)
        self.find_panel.closed.connect(self._on_find_panel_closed)
        self.find_panel.replace_all_triggered.connect(self._on_replace_all)

    def _position_find_panel(self):
        """将查找面板定位在编辑器的右上角"""
        if self.find_panel.isVisible():
            margin = 10
            panel_size = self.find_panel.sizeHint()
            x = self.editor.width() - panel_size.width() - margin
            y = margin
            self.find_panel.move(x, y)

    # 该代码是为了修复在搜索功能启用时按下enter键，同时触发下一个搜索和文本编辑器换行的问题
    def eventFilter(self, watched, event):
        if hasattr(self, 'find_panel') and self.find_panel.isVisible():
            if event.type() == QEvent.KeyPress:
                if event.key() in [Qt.Key_Enter, Qt.Key_Return]:
                    if watched is self.find_panel.search_input:
                        # 手动模拟按钮点击，确保功能被执行
                        self.find_panel.next_button.click()
                        # 返回 True，消费事件，阻止其原生处理（发射信号）和冒泡，这样就不会继续传递给QPlainTextEdit
                        return True
                    elif watched is self.find_panel.replace_input:
                        self.find_panel.replace_all_button.click()
                        return True
        
        return super().eventFilter(watched, event)

    # 确保在窗口关闭时移除过滤器，避免内存泄漏
    def closeEvent(self, event):
        QApplication.instance().removeEventFilter(self)
        super().closeEvent(event)

    def _on_new_file(self):
        """处理文件树中新建文件按钮点击的槽函数"""
        # 1. 确定要在哪个目录下新建
        selected_path = self.file_tree_view.get_selected_file_path()
        if not selected_path:
            current_dir = self.file_tree_view.model.filePath(self.file_tree_view.tree_view.rootIndex())
        elif os.path.isfile(selected_path):
            # 如果选中了一个文件，则使用该文件的父目录
            current_dir = os.path.dirname(selected_path)
        else:
            current_dir = selected_path

        # 2. 弹出输入框获取新文件名
        file_name, ok = QInputDialog.getText(self, "新建文件", "输入文件名:", QLineEdit.Normal, "untitled.txt")
        
        if ok and file_name:
            new_file_path = os.path.join(current_dir, file_name)
            try:
                # 3. 创建空文件
                with open(new_file_path, 'w', encoding='utf-8') as f:
                    f.write("")
                
                self.statusBar().showMessage(f"已创建文件: {file_name}", 3000)
                # 刷新这一块
                self.file_tree_view.model.layoutChanged.emit()
                # 展开到新建文件的父目录
                self.file_tree_view.expand_to_path(current_dir) 
                
            except Exception as e:
                self.statusBar().showMessage(f"创建文件失败: {str(e)}", 3000)
                print(f"Error creating file: {e}")

    def _on_new_folder(self):
        """处理文件树中新建文件夹按钮点击的槽函数"""
        # 1. 确定要在哪个目录下新建
        selected_path = self.file_tree_view.get_selected_file_path()
        if not selected_path:
            # 如果没选中任何东西则使用根路径
            current_dir = self.file_tree_view.model.filePath(self.file_tree_view.tree_view.rootIndex())
        elif os.path.isfile(selected_path):
            # 如果选中了一个文件，则使用该文件的父目录
            current_dir = os.path.dirname(selected_path)
        else:
            # 如果选中了一个文件夹，则使用这个文件夹
            current_dir = selected_path

        # 2. 弹出输入框获取新文件夹名
        folder_name, ok = QInputDialog.getText(self, "新建文件夹", "输入文件夹名:", QLineEdit.Normal, "New Folder")
        
        if ok and folder_name:
            new_folder_path = os.path.join(current_dir, folder_name)
            try:
                # 3. 创建文件夹 (支持多级创建)
                os.makedirs(new_folder_path, exist_ok=True)
                
                self.statusBar().showMessage(f"已创建文件夹: {folder_name}", 3000)
                # 4. 刷新文件树视图
                self.file_tree_view.model.layoutChanged.emit()
                self.file_tree_view.expand_to_path(current_dir) 
            except Exception as e:
                self.statusBar().showMessage(f"创建文件夹失败: {str(e)}", 3000)
                print(f"Error creating folder: {e}")

    def _on_file_delete(self):
        """处理文件删除动作的槽函数"""
        selected_path = self.file_tree_view.get_selected_file_path()
        print(f"Console: 尝试删除的文件路径: {selected_path}")
        if not selected_path:
            self.statusBar().showMessage("没有选中的文件可删除", 3000)
            return
        try:
            if os.path.isfile(selected_path):
                os.remove(selected_path)
                self.statusBar().showMessage(f"已删除文件: {selected_path}", 3000)
            else:
                os.rmdir(selected_path)
                self.statusBar().showMessage(f"已删除文件夹: {selected_path}", 3000)

            # 刷新文件树视图
            self.file_tree_view.model.layoutChanged.emit()
        except Exception as e:
            self.statusBar().showMessage(f"删除文件失败: {str(e)}", 3000)
            print(f"Error deleting file: {e}")

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
                self.editor_controller._clear_search()
                self.editor.setPlainText(content)
                self.current_file_path = file_path
                self.statusBar().showMessage(f"已打开文件: {file_path}", 3000)
                if self.find_panel.isVisible():
                    # 延迟执行搜索，确保文本已加载
                    QTimer.singleShot(0, self.find_panel._on_search)

        except Exception as e:
            self.statusBar().showMessage(f"打开文件失败: {str(e)}", 3000)
            print(f"Error opening file: {e}")

    def _on_file_folder_open(self):
        """处理文件夹打开动作的槽函数"""
        folder_path = QFileDialog.getExistingDirectory(
            self,  # 父窗口
            "打开文件夹",  # 对话框标题
            ""  # 默认目录
        )
        
        if folder_path:
            self.views["resource_manager"].set_root_path(folder_path)
            self.views["search_panel"].set_search_root(folder_path)
            self.editor.clear()
            self.current_file_path = None
            self.statusBar().showMessage(f"已打开文件夹: {folder_path}", 3000)

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

    def _on_file_double_clicked(self, index):
        """
        处理文件树中文件双击事件
        参数: index - 文件树中双击的项的索引
        """
        # 从模型中获取文件路径
        file_path = self.views["resource_manager"].model.filePath(index)
        
        # 检查是否是文件（非目录）
        if os.path.isfile(file_path):
            self._open_file(file_path)

    def _on_search_result_clicked(self, file_path, line_number,start_col,end_col):
        """
        处理搜索结果点击事件
        参数: file_path - 点击的结果对应的文件路径
              line_number - 点击的结果对应的行号
        """
        if self.current_file_path != file_path:
            self._open_file(file_path)
        # 定位到指定行号
        if self.editor and self.current_file_path == file_path:
            doucument = self.editor.document()
            block = doucument.findBlockByNumber(line_number - 1)
            if block.isValid():
                cursor = QTextCursor(block)
                cursor.movePosition(QTextCursor.Right, QTextCursor.MoveAnchor, start_col)
                cursor.movePosition(QTextCursor.Right, QTextCursor.KeepAnchor, end_col - start_col)
                self.editor.setTextCursor(cursor)
                self.editor.ensureCursorVisible()
                self.editor.setFocus()
            else:
                self.statusBar().showMessage(f"无法定位到指定行号 {line_number}", 3000)
        else:
            self.statusBar().showMessage("文本编辑器未初始化，无法定位行号", 3000)

    def _on_search_error_found(self, error_message):
        """
        处理搜索过程中出现的错误
        参数: error_message - 错误信息
        """
        self.statusBar().showMessage(f"搜索错误: {error_message}", 5000)
    
    def _on_search_completed(self, total_files, total_matches):
        """
        处理搜索完成事件
        参数: total_files - 搜索的总文件数
              total_matches - 找到的总匹配数
        """
        self.statusBar().showMessage(f"搜索完成: {total_files} 个文件，找到 {total_matches} 个匹配项", 5000)

    def _handle_menu_action(self, action: str):
        handler = self.action_handlers.get(action)
        if handler and callable(handler):
            handler()
        else:
            print(f"警告：没有为动作 '{action}' 找到处理函数。")

    def _on_toggle_dark_theme(self):
        self.is_dark_theme = not self.is_dark_theme
        if self.is_dark_theme:
            QApplication.instance().setStyleSheet(qdarkstyle.load_stylesheet())
            self.statusBar().showMessage("已切换到深色主题", 1500)
            print("Console: 已切换到深色主题")
        else:
            QApplication.instance().setStyleSheet("")
            QApplication.instance().setPalette(self.default_palette)
            self.statusBar().showMessage("已切换到浅色主题", 1500)
            print("Console: 已切换到浅色主题")

    def _on_select_all_matches(self):
        """处理选择所有匹配项动作的槽函数"""
        self.statusBar().showMessage("功能待实现: 选择所有匹配项", 1500)
        print("Console: 正在执行选择所有匹配项操作...")

    def _on_toggle_fullscreen(self):
        """处理查看外观里全屏动作的槽函数"""
        if self.isFullScreen():
            self.showNormal()
            self.statusBar().showMessage("已退出全屏模式", 1500)
            print("Console: 已退出全屏")
        else:
            self.showFullScreen()
            self.statusBar().showMessage("已进入全屏模式", 1500)
            print("Console: 已进入全屏")

    def _on_toggle_sidebar(self):
        """处理查看外观里主侧边栏可见性动作的槽函数"""
        is_visible = self.sidebar_dock.isVisible()
        self.sidebar_dock.setVisible(not is_visible)
        self.statusBar().showMessage(f"侧边栏 {'已隐藏' if is_visible else '已显示'}", 1500)
        print(f"Console: 侧边栏可见性切换为 {not is_visible}")

    def _show_find_dialog(self):
        """显示查找面板"""
        self._position_find_panel()
        self.find_panel.show()
        self.find_panel.raise_()
        self._position_find_panel()
        self.find_panel.search_input.setFocus()
        self.find_panel.search_input.selectAll()
        self.find_panel._on_search()  # 自动执行一次搜索

# 下面这几个其实完全可以丢进controller里面，但是我懒了,嘻嘻
    def _on_find_triggered(self, term, case_sensitive, whole_word):
        """处理查找请求"""
        current, total = self.editor_controller.edit_find(term, case_sensitive, whole_word)
        self.find_panel.update_results_label(current, total)
        if total == 0 and term:
            self.statusBar().showMessage("未找到匹配项", 1500)
        
    def _on_find_next(self):
        """处理查找下一个"""
        current, total = self.editor_controller.find_next()
        self.find_panel.update_results_label(current, total)

    def _on_find_previous(self):
        """处理查找上一个"""
        current, total = self.editor_controller.find_previous()
        self.find_panel.update_results_label(current, total)

    def _on_find_panel_closed(self):
        """当查找面板关闭时，清除搜索状态和高亮"""
        self.editor_controller._clear_search()
        self.editor.setFocus() # 将焦点还给编辑器

    def _on_replace_all(self,replace_term):
        count = self.editor_controller.replace_all(replace_term)
        if count > 0:
            self.statusBar().showMessage(f"已替换 {count} 处匹配项", 3000)
            print(f"Console: 已替换 {count} 处匹配项")
            self.find_panel.update_results_label(-1,0)
        else:
            self.statusBar().showMessage("未找到匹配项，未进行替换", 3000)
            print("Console: 未找到匹配项，未进行替换")

    def _on_toggle_output(self):
        """处理查看输出动作的槽函数"""
        self.statusBar().showMessage("功能待实现: 切换输出面板", 1500)
        print("Console: 尝试切换输出面板可见性")
        
    def _on_run_without_debugging(self):
        """处理运行以非调试模式运行动作的槽函数"""
        self.statusBar().showMessage("正在运行...", 3000)
        print("Console: 正在以非调试模式运行")

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    sys.exit(app.exec())