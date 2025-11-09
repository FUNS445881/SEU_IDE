import re
from PySide6.QtWidgets import QPlainTextEdit
from PySide6.QtGui import QKeyEvent, QTextCursor
from PySide6.QtCore import Qt

class CodeEditor(QPlainTextEdit):
    """
    一个增强的 QPlainTextEdit，支持自动缩进功能。
    """
    def __init__(self, parent=None):
        super().__init__(parent)
        self.tab_stop_width_spaces = 4  # 以空格计的Tab宽度

    def keyPressEvent(self, event: QKeyEvent):
        """
        重写 keyPressEvent 来处理回车键，以实现自动缩进。
        """
        # 只处理回车键事件 (Enter 或 Return)
        if event.key() in (Qt.Key_Enter, Qt.Key_Return):
            cursor = self.textCursor()
            
            # 获取当前行
            current_line_block = cursor.block()
            prev_line_text = current_line_block.text()
            
            # 计算上一行的缩进
            # 使用正则表达式匹配行首的空白字符
            leading_whitespace_match = re.match(r'^(\s*)', prev_line_text)
            
            # 默认为上一行的缩进
            indentation = leading_whitespace_match.group(1) if leading_whitespace_match else ""

            # 检查上一行是否以特定字符结尾
            # .strip() 移除末尾的空白
            stripped_line = prev_line_text.strip()
            if stripped_line.endswith(('{', '(', ':')):
                # 如果是，增加一级缩进
                indentation += ' ' * self.tab_stop_width_spaces
            
            # 插入新行和计算好的缩进
            cursor.beginEditBlock() # 开始一个可撤销的操作
            cursor.insertBlock() # 插入一个新行
            cursor.insertText(indentation) # 插入缩进
            cursor.endEditBlock() # 结束操作

            # 将光标滚动到可见区域
            self.ensureCursorVisible()
            
            # 事件已处理，不再传递
            return

        # 对于其他所有按键，执行默认行为
        super().keyPressEvent(event)