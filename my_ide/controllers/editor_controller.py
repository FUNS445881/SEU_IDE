# -*- coding: utf-8 -*-
# 耦合是必要的，因为他是管理类，和main_window强绑定，是main_window的一部分。
# 负责各个功能的内部实现,让main_window更简洁
from PySide6.QtGui import QTextDocument,QTextOption,QTextCharFormat,QColor
from PySide6.QtWidgets import QTextEdit

class EditorController:
    def __init__(self, editor_widget):
        self.editor = editor_widget
        self.last_search_term = None
        self.last_search_flags = QTextDocument.FindFlags(0)
        self.last_search_results = []
        self.current_search_index = -1
        self.original_font = self.editor.font()

        # 高亮格式
        self.match_format = QTextCharFormat()
        self.match_format.setBackground(QColor("yellow").darker(120))
        self.current_match_format = QTextCharFormat()
        self.current_match_format.setBackground(QColor("orange"))

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

    def zoom_reset(self):
        """重置编辑器缩放比例"""
        self.editor.setFont(self.original_font)

    def zoom_in(self):
        """放大编辑器内容"""
        self.editor.zoomIn()

    def zoom_out(self):
        """缩小编辑器内容"""
        self.editor.zoomOut()

    def toggle_word_wrap(self):
        """切换自动换行"""
        current_state = self.editor.wordWrapMode()
        if current_state == QTextOption.WrapAtWordBoundaryOrAnywhere:
            self.editor.setWordWrapMode(QTextOption.NoWrap)
            print("Console: 自动换行已关闭")
        else:
            self.editor.setWordWrapMode(QTextOption.WrapAtWordBoundaryOrAnywhere)
            print("Console: 自动换行已开启")

    def edit_find(self, term, case_sensitive=False, whole_word=False):
        """查找功能的实现"""
        if not term:
            return -1,0
        print("执行查找:", term, case_sensitive, whole_word)
        # 1, 设置查找标志
        flags = QTextDocument.FindFlags(0)
        if case_sensitive:
            flags |= QTextDocument.FindCaseSensitively
        if whole_word:
            flags |= QTextDocument.FindWholeWords
        # 如果搜索条件未变，则无需重新搜索
        if term == self.last_search_term and flags == self.last_search_flags:
            if not self.last_search_results:
                return -1, 0
            # 确保当前高亮正确
            self._highlight_all_matches()
            return self.current_search_index, len(self.last_search_results)
        
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
            self._highlight_all_matches()
        else:
            self.editor.setExtraSelections([]) # 清除高亮
        
        return self.current_search_index, len(self.last_search_results)

    def find_next(self):
        if not self.last_search_results:
            print("next没有查找结果")
            return -1, 0
        self.current_search_index = (self.current_search_index + 1) % len(self.last_search_results)
        self._highlight_all_matches()
        return self.current_search_index, len(self.last_search_results)

    def find_previous(self):
        if not self.last_search_results:
            print("previous没有查找结果")
            return -1, 0
        self.current_search_index = (self.current_search_index - 1) % len(self.last_search_results)
        self._highlight_all_matches()
        return self.current_search_index, len(self.last_search_results)

    def _clear_search(self):
        """清除上次的查找结果"""
        # 清除高亮
        self.editor.setExtraSelections([])
        # 还原状态
        self.last_search_term = None
        self.last_search_flags = QTextDocument.FindFlags(0)
        self.last_search_results = []
        self.current_search_index = -1

    def _highlight_all_matches(self):
        """高亮当前匹配项"""
        selections = []
        for i, cursor in enumerate(self.last_search_results):
            selection = QTextEdit.ExtraSelection()
            selection.cursor = cursor
            if i == self.current_search_index:
                selection.format = self.current_match_format
                self.editor.setTextCursor(cursor) # 将光标移动到当前匹配项
                self.editor.ensureCursorVisible()
            else:
                selection.format = self.match_format
            selections.append(selection)
        self.editor.setExtraSelections(selections)

    def edit_replace(self):
        """替换功能的实现"""
        # 这里可以添加替换逻辑
        pass