# -*- coding: utf-8 -*-
# 耦合是必要的，因为他是管理类，和main_window强绑定，是main_window的一部分。
# 负责各个功能的内部实现,让main_window更简洁
from PySide6.QtGui import QTextDocument

class EditorController:
    def __init__(self, editor_widget):
        self.editor = editor_widget
        self.last_search_term = None
        self.last_search_flags = QTextDocument.FindFlags(0)
        self.last_search_results = []
        self.current_search_index = -1

    def undo(self):
        """撤销上一步操作"""
        self.editor.undo()

    def redo(self):
        """重做上一步操作"""
        self.editor.redo()

    def cut(self):
        """剪切选中的内容"""
        self.editor.cut()

    def copy(self):
        """复制选中的内容"""
        self.editor.copy()

    def paste(self):
        """粘贴剪贴板内容"""
        self.editor.paste()

    def select_all(self):
        """全选内容"""
        self.editor.selectAll()

    def zoom_in(self):
        """放大编辑器内容"""
        self.editor.zoomIn()

    def zoom_out(self):
        """缩小编辑器内容"""
        self.editor.zoomOut()

    def edit_find(self, term, case_sensitive=False, whole_word=False):
        """查找功能的实现"""
        if not term:
            return
        
        # 1, 设置查找标志
        flags = QTextDocument.FindFlags(0)
        if case_sensitive:
            flags |= QTextDocument.FindCaseSensitively
        if whole_word:
            flags |= QTextDocument.FindWholeWords
        if term == self.last_search_term and flags == self.last_search_flags:
            # 继续上次的查找
            self.find_next()
            return
        
        self._clear_search()
        self.last_search_term = term
        self.last_search_flags = flags
        # 从文档头开始查找
        cursor = self.editor.document().find(term, 0, flags)
        while not cursor.isNull():
            self.last_search_results.append(cursor)
            cursor = self.editor.document().find(term, cursor, flags)
        
        if self.last_search_results:
            self.current_search_index = 0
            self._highlight_current_match()
        else:
            print("未找到匹配项")

    def find_next(self):
        if not self.last_search_results:
            print("没有查找结果")
            return
        self.current_search_index = (self.current_search_index + 1) % len(self.last_search_results)
        self._highlight_current_match()

    def find_previous(self):
        if not self.last_search_results:
            print("没有查找结果")
            return
        self.current_search_index = (self.current_search_index - 1) % len(self.last_search_results)
        self._highlight_current_match()

    def _clear_search(self):
        """清除上次的查找结果"""
        self.last_search_term = None
        self.last_search_flags = QTextDocument.FindFlags(0)
        self.last_search_results = []
        self.current_search_index = -1

    def _highlight_current_match(self):
        """高亮当前匹配项"""
        if 0 <= self.current_search_index < len(self.last_search_results):
            cursor = self.last_search_results[self.current_search_index]
            self.editor.setTextCursor(cursor)
            self.editor.ensureCursorVisible()

    def edit_replace(self):
        """替换功能的实现"""
        # 这里可以添加替换逻辑
        pass