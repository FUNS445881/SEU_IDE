import os
import re
from PySide6.QtCore import QObject,Signal,QThread,Qt
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout,QHBoxLayout,QLineEdit,
    QPushButton,QCheckBox,QTreeWidget,QTreeWidgetItem,QHeaderView
)


# 搜索功能实现，非UI
class SearchWorker(QObject):
    """
    不阻塞主线程的情况下执行文件搜索任务
    """
    match_found = Signal(str, int, str)  # 信号：找到匹配项，参数为文件路径、行号、行内容
    search_finished = Signal(int,int) # 信号：搜索完成，参数为总文件数和总匹配数
    error_occurred = Signal(str)  # 信号：发生错误，参数为错误信息

    def __init__(self,root_path,search_term,is_case_sensitive,use_regex):
        super().__init__()
        self.root_path = root_path
        self.search_term = search_term
        self.is_case_sensitive = is_case_sensitive
        self.use_regex = use_regex
        self._is_running = True
    
    def run_search(self):
        """
        执行搜索任务
        """
        files_searched = 0
        matches_found = 0
        try:
            flags = 0 if self.is_case_sensitive else re.IGNORECASE
            if self.use_regex:
                pattern = re.compile(self.search_term, flags)
            else:
                pattern = re.compile(re.escape(self.search_term), flags)
            for dirpath, _, filenames in os.walk(self.root_path):
                if not self._is_running:
                    break
                for filename in filenames:
                    if not self._is_running:
                        break
                    file_path = os.path.join(dirpath, filename)
                    try:
                        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                            for line_num, line in enumerate(f, start=1):
                                if pattern.search(line):
                                    self.match_found.emit(file_path, line_num, line.strip())
                                    matches_found += 1
                        files_searched += 1
                    except Exception as e:
                        # 忽略无法读取的文件
                        continue
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
    result_clicked = Signal(str,int) # 参数为文件路径和行号

    # 故意和worker不同名，为了体现层级关系.同时也是为了封装
    search_completed = Signal(int,int) # 参数为总文件数和总匹配数
    error_found = Signal(str)  # 参数为错误信息

    def __init__(self,root_path,parent=None):
        super().__init__(parent)
        self.root_path = root_path
        self.search_thread = None
        self.search_worker = None

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
        self.regex_checkbox = QCheckBox("使用正则表达式")
        options_layout.addWidget(self.case_sensitive_checkbox)
        options_layout.addWidget(self.regex_checkbox)
        layout.addLayout(options_layout)

        # 结果显示区域
        self.results_tree = QTreeWidget()
        self.results_tree.setHeaderLabels(["文件路径", "内容", "行号"])
        self.results_tree.header().setSectionResizeMode(0, QHeaderView.ResizeToContents)
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
        
        self.results_tree.clear()
        self.search_button.setEnabled(False)
        self.search_button.setText("搜索中...")
        # 创建新的搜索任务
        self.search_worker = SearchWorker(
            self.root_path,
            search_term,
            self.case_sensitive_checkbox.isChecked(),
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
        
        self.search_thread.start()
    
    def add_match(self,file_path,line_number,line_content):
        """
        添加搜索结果项
        """
        top_level_items = self.results_tree.findItems(file_path, Qt.MatchExactly, 0)
        if top_level_items:
            parent_item = top_level_items[0]
        else:
            parent_item = QTreeWidgetItem(self.results_tree, [file_path])
            parent_item.setData(0, Qt.UserRole, file_path)
        # 添加子项
        child_item = QTreeWidgetItem(parent_item, ["", line_content, f"行{line_number}"])
        child_item.setData(0, Qt.UserRole, (file_path, line_number))
    
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

    def on_result_clicked(self,item,column):
        """
        处理结果项双击事件
        """
        data = item.data(0, Qt.UserRole)
        if isinstance(data, tuple) and len(data) == 2:
            file_path, line_number = data
            self.result_clicked.emit(file_path, line_number)
        else:
            print("点击的不是结果项")
            print(type(data), data)
            print(len(data) if isinstance(data, (list, tuple)) else "N/A")