import sys
import os
from PySide6.QtWidgets import QApplication,QMainWindow,QPlainTextEdit,QFileDialog, QDockWidget, QHBoxLayout, QStackedWidget,QWidget
from PySide6.QtGui import QAction,QTextCursor,QTextOption
from PySide6.QtCore import Qt
from ..components.file_tree import FileTreeWidget
from ..components.activity_bar import ActivityBar
from ..components.menu_bar import MenuBar
from ..components.search_panel import SearchPanel


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
        self.file_tree_view.set_root_path(current_dir)
        self.file_tree_view.tree_view.doubleClicked.connect(self._on_file_double_clicked)

        # search_panel初始化
        self.search_panel_view.result_clicked.connect(self._on_search_result_clicked)
        self.search_panel_view.error_found.connect(self._on_search_error_found)
        self.search_panel_view.search_completed.connect(self._on_search_completed)

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
        pass

    def _on_edit_undo(self):
        """处理编辑撤销动作的槽函数"""
        self.editor.undo()
        self.statusBar().showMessage("已执行: 撤销", 1500)
        print("Console: 已执行撤销操作")

    def _on_edit_redo(self):
        """处理编辑恢复动作的槽函数"""
        self.editor.redo()
        self.statusBar().showMessage("已执行: 恢复", 1500)
        print("Console: 已执行恢复操作")

    def _on_edit_cut(self):
        """处理编辑剪切动作的槽函数"""
        self.editor.cut()
        self.statusBar().showMessage("已执行: 剪切", 1500)
        print("Console: 已执行剪切操作")

    def _on_edit_copy(self):
        """处理编辑复制动作的槽函数"""
        self.editor.copy()
        self.statusBar().showMessage("已执行: 复制", 1500)
        print("Console: 已执行复制操作")

    def _on_edit_paste(self):
        """处理编辑粘贴动作的槽函数"""
        self.editor.paste()
        self.statusBar().showMessage("已执行: 粘贴", 1500)
        print("Console: 已执行粘贴操作")

    def _on_select_all(self):
        """处理选择全选动作的槽函数"""
        self.editor.selectAll()
        self.statusBar().showMessage("已执行: 全选", 1500)
        print("Console: 已执行全选操作")

    def _on_select_repeat(self):
        """处理选择重复选择动作的槽函数"""
        self.statusBar().showMessage("功能待实现: 重复选择", 1500)
        print("Console: 正在执行重复选择操作...")

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

    def _on_toggle_menubar(self):
        """处理查看外观里菜单栏可见性动作的槽函数"""
        is_visible = self.menuBar().isVisible()
        self.menuBar().setVisible(not is_visible)
        self.statusBar().showMessage(f"菜单栏 {'已隐藏' if is_visible else '已显示'}", 1500)
        print(f"Console: 菜单栏可见性切换为 {not is_visible}")

    def _on_toggle_sidebar(self):
        """处理查看外观里主侧边栏可见性动作的槽函数"""
        is_visible = self.sidebar_dock.isVisible()
        self.sidebar_dock.setVisible(not is_visible)
        self.statusBar().showMessage(f"侧边栏 {'已隐藏' if is_visible else '已显示'}", 1500)
        print(f"Console: 侧边栏可见性切换为 {not is_visible}")

    def _on_zoom_in(self):
        """处理查看外观里放大编辑器字体动作的槽函数"""
        self.editor.zoomIn(1) 
        self.statusBar().showMessage("已放大", 1500)
        print("Console: 放大字体")

    def _on_zoom_out(self):
        """处理查看外观里缩小编辑器字体的槽函数"""
        self.editor.zoomOut(1) 
        self.statusBar().showMessage("已缩小", 1500)
        print("Console: 缩小字体")

    def _on_set_editor_layout(self, layout_type):
        """处理查看编辑器布局动作的槽函数"""
        self.statusBar().showMessage(f"功能待实现: 设置编辑器布局为 {layout_type}", 1500)
        print(f"Console: 尝试设置编辑器布局为 {layout_type}")

    def _on_toggle_output(self):
        """处理查看输出动作的槽函数"""
        self.statusBar().showMessage("功能待实现: 切换输出面板", 1500)
        print("Console: 尝试切换输出面板可见性")
        
    def _on_toggle_word_wrap(self):
        """处理查看自动换行动作的槽函数"""
        current_mode = self.editor.wordWrapMode()
        if current_mode == QTextOption.WrapAtWordBoundaryOrAnywhere:
            self.editor.setWordWrapMode(QTextOption.NoWrap)
            self.statusBar().showMessage("已关闭自动换行", 1500)
            print("Console: 自动换行已关闭")
        else:
            self.editor.setWordWrapMode(QTextOption.WrapAtWordBoundaryOrAnywhere)
            self.statusBar().showMessage("已开启自动换行", 1500)
            print("Console: 自动换行已开启")

    def _on_go_back(self):
        """处理转到里返回动作的槽函数"""
        self.statusBar().showMessage("功能待实现: 返回上一个位置", 1500)
        print("Console: 正在执行返回操作...")
        
    def _on_go_forward(self):
        """处理转到里前进动作的槽函数"""
        self.statusBar().showMessage("功能待实现: 前进到下一个位置", 1500)
        print("Console: 正在执行前进操作...")
        
    def _on_go_to_type_definition(self):
        """处理转到类型定义动作的槽函数"""
        self.statusBar().showMessage("功能待实现: 转到类型定义", 1500)
        print("Console: 正在执行转到类型定义操作...")
        
    def _on_go_to_bracket(self):
        """处理转到括号动作的槽函数"""
        self.statusBar().showMessage("功能待实现: 转到匹配的括号", 1500)
        print("Console: 正在执行转到括号操作...")
        
    def _on_prev_problem(self):
        """处理转到上一个问题动作的槽函数"""
        self.statusBar().showMessage("功能待实现: 上一个问题", 1500)
        print("Console: 正在执行上一个问题操作...")
        
    def _on_next_problem(self):
        """处理转到下一个问题动作的槽函数"""
        self.statusBar().showMessage("功能待实现: 下一个问题", 1500)
        print("Console: 正在执行下一个问题操作...")

    def _on_prev_change(self):
        """处理转到上一个更改动作的槽函数"""
        self.statusBar().showMessage("功能待实现: 上一个更改", 1500)
        print("Console: 正在执行上一个更改操作...")

    def _on_next_change(self):
        """处理转到下一个更改动作的槽函数"""
        self.statusBar().showMessage("功能待实现: 下一个更改", 1500)
        print("Console: 正在执行下一个更改操作...")

    def _on_start_debugging(self):
        """处理运行启动调试动作的槽函数"""
        self.statusBar().showMessage("正在调试...", 3000)
        print("Console: 正在启动调试")

    def _on_run_without_debugging(self):
        """处理运行以非调试模式运行动作的槽函数"""
        self.statusBar().showMessage("正在运行...", 3000)
        print("Console: 正在以非调试模式运行")

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    sys.exit(app.exec())