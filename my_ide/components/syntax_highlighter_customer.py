import sys
from PySide6.QtGui import QSyntaxHighlighter, QTextCharFormat, QColor, QFont
from PySide6.QtCore import QObject

from pygments.lexer import Lexer
from pygments.styles import get_style_by_name
from pygments.util import ClassNotFound
from pygments.token import Token

from my_ide.config.settings import DEFAULT_STYLE, DEFAULT_BACKGROUND_COLOR, DEFAULT_TEXT_COLOR

class CustomHighlighter(QSyntaxHighlighter):
    def __init__(self, parent: QObject, lexer: Lexer, style_name: str = 'default'):
        super().__init__(parent)
        self.lexer = lexer
        self.formats = {}

        if style_name.lower() == 'default':
            # 如果是我们的自定义默认风格
            self.background_color = QColor(DEFAULT_BACKGROUND_COLOR)
            self.text_color = QColor(DEFAULT_TEXT_COLOR)
            self._create_formats_from_dict(DEFAULT_STYLE)
        else:
            # 否则，从 Pygments 加载
            try:
                style = get_style_by_name(style_name)
                self.background_color = QColor(style.background_color)
                # 计算默认文本颜色
                text_style_def = style.style_for_token(Token.Text) or style.style_for_token(Token)
                self.text_color = QColor(f"#{text_style_def['color']}")
                self._create_formats_from_pygments_style(style)
            except ClassNotFound:
                # 如果Pygments风格不存在，回退到我们的默认风格
                self.background_color = QColor(DEFAULT_BACKGROUND_COLOR)
                self.text_color = QColor(DEFAULT_TEXT_COLOR)
                self._create_formats_from_dict(DEFAULT_STYLE)

    def _create_formats_from_dict(self, style_dict):
        """从我们自定义的字典构建格式"""
        for token_type, style_str in style_dict.items():
            self.formats[token_type] = self._parse_style_str(style_str)
        self.formats[Token] = self.formats.get(Token, QTextCharFormat())

    def _parse_style_str(self, style_str):
        """解析 '#RRGGBB bold italic' 这样的字符串"""
        parts = style_str.split()
        fmt = QTextCharFormat()
        fmt.setForeground(QColor(parts[0]))
        if 'bold' in parts:
            fmt.setFontWeight(QFont.Bold)
        if 'italic' in parts:
            fmt.setFontItalic(True)
        return fmt

    def _create_formats_from_pygments_style(self, style):
        """从Pygments的style对象构建格式"""
        for token_type, style_def in style:
            self.formats[token_type] = self._create_format_from_token_style(style_def)
        self.formats[Token] = self.formats.get(Token, QTextCharFormat())

    def _create_format_from_token_style(self, token_style):
        """将Pygments的风格定义转换为QTextCharFormat"""
        fmt = QTextCharFormat()
        if token_style['color']:
            fmt.setForeground(QColor(f"#{token_style['color']}"))
        if token_style['bgcolor']:
            fmt.setBackground(QColor(f"#{token_style['bgcolor']}"))
        if token_style['bold']:
            fmt.setFontWeight(QFont.Bold)
        if token_style['italic']:
            fmt.setFontItalic(True)
        if token_style['underline']:
            fmt.setFontUnderline(True)
        return fmt

    def highlightBlock(self, text: str):
        if not text or not self.lexer: return
        for start_index, token_type, token_value in self.lexer.get_tokens_unprocessed(text):
            length = len(token_value)
            current_token_type = token_type
            while current_token_type not in self.formats:
                current_token_type = current_token_type.parent
                if current_token_type is None: break
            style = self.formats.get(current_token_type, self.formats.get(Token))
            if style:
                self.setFormat(start_index, length, style)