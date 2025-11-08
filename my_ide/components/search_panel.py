import os
import re
import html
from PySide6.QtGui import QFontMetrics,QColor
from PySide6.QtCore import QObject,Signal,QThread,Qt
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout,QHBoxLayout,QLineEdit,
    QPushButton,QCheckBox,QTreeWidget,QTreeWidgetItem,QHeaderView,QLabel,
    QAbstractItemView
)
from my_ide.config.settings import ignored_dirs, ignored_exts

# 搜索功能实现，非UI
class SearchWorker(QObject):
    """
    不阻塞主线程的情况下执行文件搜索任务
    """
    match_found = Signal(str, int, str,int,int)  # 信号：找到匹配项，参数为文件路径、行号、行内容、匹配开始位置、匹配结束位置
    search_finished = Signal(int,int) # 信号：搜索完成，参数为总文件数和总匹配数
    error_occurred = Signal(str)  # 信号：发生错误，参数为错误信息

    def __init__(self,root_path,search_term,is_case_sensitive,is_strict_match,use_regex):
        super().__init__()
        self.root_path = root_path
        self.search_term = search_term
        self.is_case_sensitive = is_case_sensitive
        self.is_strict_match = is_strict_match
        self.use_regex = use_regex
        self._is_running = True
    
    def run_search(self):
        """
        执行搜索任务
        """
        files_searched = 0
        matches_found = 0
        try:
            root_dir_name = os.path.basename(self.root_path)
            if root_dir_name in ignored_dirs:
                self.search_finished.emit(0, 0)
                return
            final_search_term = self.search_term
            if not self.use_regex:
                final_search_term = re.escape(self.search_term)
            if self.is_strict_match:
                final_search_term = r'\b' + final_search_term + r'\b'
            flags = 0 if self.is_case_sensitive else re.IGNORECASE
            pattern = re.compile(final_search_term, flags)               
            for dirpath, dirs, filenames in os.walk(self.root_path):
                if not self._is_running:
                    break
                if dirpath.split(os.sep)[-1] in ignored_dirs:
                    continue
                dirs[:] = [d for d in dirs if d not in ignored_dirs]
                for filename in filenames:
                    if not self._is_running:
                        break
                    _, ext = os.path.splitext(filename)
                    if ext in ignored_exts:
                        continue # 跳过这个文件
                    file_path = os.path.join(dirpath, filename)
                    try:
                        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                            for line_num, line in enumerate(f, start=1):
                                for match in pattern.finditer(line):
                                    self.match_found.emit(file_path, line_num, line.rstrip(), match.start(), match.end())
                                    matches_found += 1
                        files_searched += 1
                    except Exception as e:
                        # 忽略无法读取的文件
                        continue
            self.search_finished.emit(files_searched, matches_found)
        except re.error as e:
            self.error_occurred.emit(f"正则表达式错误: {str(e)}")
        except Exception as e:
            if self._is_running:
                self.error_occurred.emit(f"搜索过程中发生错误: {str(e)}")

    def stop(self):
        """
        停止搜索任务
        """
        self._is_running = False

class SearchPanel(QWidget):
    # 用户点击信号
    result_clicked = Signal(str,int,int,int) # 参数为文件路径,行号,匹配开始位置,匹配结束位置

    # 故意和worker不同名，为了体现层级关系.同时也是为了封装
    search_completed = Signal(int,int) # 参数为总文件数和总匹配数
    error_found = Signal(str)  # 参数为错误信息

    def __init__(self,root_path,parent=None):
        super().__init__(parent)
        self.root_path = root_path
        self.search_thread = None
        self.search_worker = None

        # 折叠字典
        self.file_items = {}

        self._init_ui()

    def _init_ui(self):
        layout = QVBoxLayout(self)
        # 搜索输入和选项
        search_input_layout = QHBoxLayout()
        self.search_input = QLineEdit()
        self.search_input.setPlaceholderText("输入搜索内容...")
        self.search_button = QPushButton("搜索")
        search_input_layout.addWidget(self.search_input)
        search_input_layout.addWidget(self.search_button)
        layout.addLayout(search_input_layout)

        options_layout = QHBoxLayout()
        self.case_sensitive_checkbox = QCheckBox("区分大小写")
        self.strict_checkbox = QCheckBox("严格匹配")
        self.regex_checkbox = QCheckBox("使用正则表达式")
        options_layout.addWidget(self.case_sensitive_checkbox)
        options_layout.addWidget(self.strict_checkbox)
        options_layout.addWidget(self.regex_checkbox)

        layout.addLayout(options_layout)
        #使用双列布局 ---
        self.results_tree = QTreeWidget()
        self.results_tree.setColumnCount(2)
        self.results_tree.setHeaderHidden(True)
        self.results_tree.setIndentation(5) # 设置子项的缩进距离

        header = self.results_tree.header()
        # 第0列（行号）：宽度根据内容自适应
        header.setSectionResizeMode(0, QHeaderView.ResizeToContents)
        # 第1列（内容）：占据所有剩余空间
        header.setSectionResizeMode(1, QHeaderView.Stretch)

        layout.addWidget(self.results_tree)

        # 连接信号
        self.search_button.clicked.connect(self.start_search)
        self.search_input.returnPressed.connect(self.start_search)
        self.results_tree.itemDoubleClicked.connect(self.on_result_clicked)
    
    def start_search(self):
        """
        启动搜索任务
        """
        search_term = self.search_input.text().strip()
        if not search_term:
            return  # 不搜索空内容
        if self.search_thread and self.search_thread.isRunning():
            # 终止上一次搜索任务
            self.search_worker.stop()
            self.search_thread.quit()
            self.search_thread.wait()
        
        self.file_items.clear()

        self.results_tree.clear()
        self.search_button.setEnabled(False)
        self.search_button.setText("搜索中...")
        # 创建新的搜索任务
        self.search_worker = SearchWorker(
            self.root_path,
            search_term,
            self.case_sensitive_checkbox.isChecked(),
            self.strict_checkbox.isChecked(),
            self.regex_checkbox.isChecked()
        )
        self.search_thread = QThread()
        self.search_worker.moveToThread(self.search_thread)

        # 连接信号
        self.search_thread.started.connect(self.search_worker.run_search)
        self.search_worker.match_found.connect(self.add_match)
        self.search_worker.error_occurred.connect(self.on_search_error)
        self.search_worker.search_finished.connect(self.on_search_finished)
        
        # 线程清理
        self.search_worker.search_finished.connect(self.search_thread.quit)
        self.search_thread.finished.connect(self.search_worker.deleteLater)
        self.search_thread.finished.connect(self.search_thread.deleteLater)
        
        # 线程解绑
        self.search_thread.finished.connect(self._on_thread_finished)

        self.search_thread.start()

    def add_match(self, file_path, line_number, line_content, match_start, match_end):
        """
        使用双列布局添加搜索结果项
        """
        # 查找或创建父项
        if file_path in self.file_items:
            parent_item = self.file_items[file_path]
        else:
            # 父项跨越所有列，所以我们用 setFirstColumnSpanned
            parent_item = QTreeWidgetItem(self.results_tree)
            parent_item.setFirstColumnSpanned(True)
            parent_item.setData(0, Qt.UserRole, file_path)
            parent_item.setData(0, Qt.UserRole + 1, 0)
            parent_item.setToolTip(0, file_path)
            self.file_items[file_path] = parent_item
            parent_item.setExpanded(True)

        # 更新父项的匹配计数和文本
        match_count = parent_item.data(0, Qt.UserRole + 1) + 1
        parent_item.setData(0, Qt.UserRole + 1, match_count)
        parent_item.setText(0, f"{os.path.basename(file_path)} ({match_count})")

        # --- 创建子项，分别设置两列的内容 ---
        child_item = QTreeWidgetItem(parent_item)
        child_item.setFirstColumnSpanned(False)
        item_data = (file_path, line_number, match_start, match_end, line_content)
        child_item.setData(0, Qt.UserRole, item_data)

        # 第0列: 设置行号文本
        child_item.setText(0, str(line_number))
        child_item.setTextAlignment(0, Qt.AlignRight | Qt.AlignTop)
        child_item.setForeground(0, QColor("grey")) # 用灰色显示行号

        # 第1列: 设置带高亮的内容QLabel        
        prefix = line_content[:match_start]
        match = line_content[match_start:match_end]
        suffix = line_content[match_end:]
        highlighted_content = (
            html.escape(prefix, quote=False) +
            f'<span style="background-color: yellow; color: black;">' +
            html.escape(match, quote=False) +
            '</span>' +
            html.escape(suffix, quote=False)
        )
        content_label = QLabel(highlighted_content)
        content_label.setTextFormat(Qt.RichText)
        content_label.setWordWrap(False)
        content_label.setContentsMargins(4, 1, 4, 1) # 微调边距
        content_label.setToolTip(line_content)

        self.results_tree.setItemWidget(child_item, 1, content_label)

    def on_search_error(self,error_message):
        """
        处理搜索错误
        """
        self.on_search_finished(0,0)
        # 显示错误信息
        self.error_found.emit(error_message)
        # 调试信息
        print(f"搜索错误: {error_message}")

    def on_search_finished(self,files_searched,matches_found):
        """
        处理搜索完成
        """
        self.search_button.setEnabled(True)
        self.search_button.setText("搜索")

        self.search_completed.emit(files_searched, matches_found)
        print(f"搜索完成: 搜索了 {files_searched} 个文件，找到 {matches_found} 个匹配项")

    def on_result_clicked(self, item, column):
        """
        处理结果项双击事件
        """
        # 提取数据并发送信号
        is_child_item = bool(item.parent())
        if is_child_item:
            data = item.data(0, Qt.UserRole)
            if isinstance(data, tuple) and len(data) == 5:
                file_path, line_number, match_start, match_end, line_content = data
                self.result_clicked.emit(file_path, line_number, match_start, match_end)
        else:
            file_path = item.data(0, Qt.UserRole)
            line_number = 1
            self.result_clicked.emit(file_path, line_number, 0, 0)

    def _on_thread_finished(self):
        """
        线程完全结束后调用的槽函数，用于清理python引用
        """
        self.search_thread = None
        self.search_worker = None
        print("搜索线程已完全结束")

    def set_search_root(self, folder_path):
        """
        设置搜索根目录
        """
        self.root_path = folder_path
        print(f"搜索根目录已设置为: {folder_path}")