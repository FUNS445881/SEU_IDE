# -*- coding: utf-8 -*-
import sys
from PySide6.QtGui import QSyntaxHighlighter, QTextCharFormat, QColor, QFont
from PySide6.QtCore import QObject
from pygments import highlight
from pygments.formatter import Formatter
from pygments.lexer import Lexer
from pygments.token import (
    Token,
    Name,
    Comment,
    Keyword,
    String,
    Number,
    Operator,
    Punctuation,
    Literal,
    Error,
)

class PygmentsHighlighter(QSyntaxHighlighter):
    """
    一个基于 Pygments 的 QSyntaxHighlighter 实现。
    """
    def __init__(self, parent: QObject, lexer: Lexer):
        """
        初始化高亮器。
        :param parent: 父对象，通常是 editor.document()。
        :param lexer: 用于语法分析的 Pygments Lexer 实例。
        """
        super().__init__(parent)
        self.lexer = lexer
        self.styles = self._get_default_styles()

    def _get_default_styles(self):
        """
        定义一个默认的、类似 VSCode 浅色主题的样式映射。
        将 Pygments Token 类型映射到 QTextCharFormat。
        """
        # 辅助函数，用于快速创建格式
        def create_format(foreground, bold=False, italic=False):
            fmt = QTextCharFormat()
            fmt.setForeground(QColor(foreground))
            if bold:
                fmt.setFontWeight(QFont.Bold)
            if italic:
                fmt.setFontItalic(True)
            return fmt

        # 样式定义
        styles = {
            Token:              create_format("#000000"),
            Comment:            create_format("#008000"),
            Comment.Preproc:    create_format("#0000FF"),
            Keyword:            create_format("#0000FF", bold=True),
            Keyword.Constant:   create_format("#0000FF"),
            Keyword.Declaration:create_format("#0000FF"),
            Keyword.Namespace:  create_format("#0000FF"),
            Keyword.Pseudo:     create_format("#0000FF"),
            Keyword.Reserved:   create_format("#0000FF"),
            Keyword.Type:       create_format("#0000FF"),
            
            Operator:           create_format("#A52A2A"),
            Operator.Word:      create_format("#0000FF", bold=True), # e.g., 'and', 'or', 'not'
            
            Punctuation:        create_format("#000000"),

            Name:               create_format("#000000"),
            Name.Builtin:       create_format("#0000FF"), # <- 这将修复 range 的颜色
            Name.Function:      create_format("#795E26"),
            Name.Class:         create_format("#267f99", bold=True),
            Name.Variable:      create_format("#001080"),
            Name.Constant:      create_format("#001080"),
            Name.Attribute:     create_format("#001080"), # <- 这将修复 self.highlighter 的颜色
            Name.Tag:           create_format("#800000"),
            
            String:             create_format("#A31515"),
            String.Doc:         create_format("#A31515"),
            String.Interpol:    create_format("#A31515"),
            String.Escape:      create_format("#A31515"),
            
            Number:             create_format("#098658"),
            Literal:            create_format("#098658"),
            
            Error:              create_format("#FF0000", bold=True),
        }
        return styles

    def highlightBlock(self, text: str):
        """
        Qt 在需要重绘文本块时会自动调用此方法。
        
        :param text: 当前需要高亮的文本块（通常是一行）。
        """
        if not text or not self.lexer:
            return
            
        start_pos = 0
        count = 0
        for start_index, token_type, token_value in self.lexer.get_tokens_unprocessed(text):
            length = len(token_value)
            
            # 查找最匹配的样式
            current_token_type = token_type
            while current_token_type not in self.styles:
                current_token_type = current_token_type.parent
                if current_token_type is None:
                    break
            
            # 使用 .get() 提供一个默认值，更安全
            style = self.styles.get(current_token_type, self.styles[Token])
            
            # 使用 Pygments 提供的精确起始位置和我们计算的长度来应用格式
            self.setFormat(start_index, length, style)