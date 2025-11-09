import sys
import os
from PySide6.QtWidgets import QMenuBar, QMenu
from PySide6.QtGui import QAction, QKeySequence
from PySide6.QtCore import Signal


class MenuBar(QMenuBar):
    action_triggered = Signal(str)

    def __init__(self, parent = None):
        super().__init__()
        self._init_menu_content()
    
    def _init_menu_content(self):

        file_menu = self.addMenu("&File")
        self._add_action(file_menu,"&Open","file_open","Open a file","Ctrl+O")
        self._add_action(file_menu,"&Open Folder","file_open_folder","Open a folder","Ctrl+Shift+O")
        self._add_action(file_menu,"&New","file_new","Create a new file","Ctrl+N")
        self._add_action(file_menu,"&New Folder","file_new_folder","Create a new folder","Ctrl+Shift+N")
        self._add_action(file_menu,"&Save","file_save","Save the current file","Ctrl+S")
        self._add_action(file_menu,"&Delete","file_delete","Delete the current file","Del")
        file_menu.addSeparator() # 添加一条分割线
        self._add_action(file_menu,"&Exit","file_exit","Exit the application","Ctrl+Q")


        edit_menu = self.addMenu("&Edit")
        self._add_action(edit_menu,"&Find","edit_find","Find in the current file","Ctrl+F")
        self._add_action(edit_menu,"&Replace","edit_replace","Replace in the current file","Ctrl+H")
        self._add_action(edit_menu,"&Undo","edit_undo","Undo the last action","Ctrl+Z")
        self._add_action(edit_menu,"&Redo","edit_redo","Redo the undone action","Ctrl+Y")
        edit_menu.addSeparator()
        self._add_action(edit_menu,"&Cut","edit_cut","Cut the selected content","Ctrl+X")
        self._add_action(edit_menu,"&Copy","edit_copy","Copy the selected content","Ctrl+C")
        self._add_action(edit_menu,"&Paste","edit_paste","Paste content from clipboard","Ctrl+V")

        select_menu = self.addMenu("Selec&t")
        self._add_action(select_menu,"&Select All","select_all","Select all content","Ctrl+A")
        select_menu.addSeparator() 
        self._add_action(select_menu,"&Select All Matches","select_all_matches","Select all items matching the current selection","Ctrl+Shift+L")

        view_menu = self.addMenu("&View")

        appearance_menu = QMenu("&Appearance", self)

        # 全屏&菜单栏&主侧边栏
        self._add_action(appearance_menu,"&Fullscreen","toggle_fullscreen","Toggle Fullscreen","F11")
        self._add_action(appearance_menu,"&Primary Side Bar","toggle_sidebar","Toggle Primary Side Bar visibility","Ctrl+B",is_checkable=True,is_checked=True)
        appearance_menu.addSeparator() 

        # 重置&放大&缩小
        self._add_action(appearance_menu,"&Reset Zoom","zoom_reset","Reset editor zoom level","Ctrl+0")
        self._add_action(appearance_menu,"Zoom &In","zoom_in","Increase editor zoom level","Ctrl+=")
        self._add_action(appearance_menu,"Zoom &Out","zoom_out","Decrease editor zoom level","Ctrl+-")
        appearance_menu.addSeparator() 

        # 主题切换    
        self._add_action(appearance_menu,"Dark &Theme","toggle_dark_theme","Toggle Dark Theme","F12",is_checkable=True)
        
        view_menu.addMenu(appearance_menu)
        view_menu.addSeparator() 

        # 3. 输出 (Output)
        self._add_action(view_menu,"&Output","toggle_output","Toggle Output Panel visibility","Ctrl+Shift+U")
        
        # 4. 自动换行 (Word Wrap)
        self._add_action(view_menu,"Word &Wrap","toggle_word_wrap","Toggle Word Wrap","Alt+Z",is_checkable=True)


        # go_menu = self.addMenu("&Go")
        # # 1. 返回
        # self._add_action(go_menu,"&Back","go_back","Go back to the previous location","Alt+Left")
        # # 2. 前进
        # self._add_action(go_menu,"&Forward","go_forward","Go forward to the next location","Alt+Right")
        # go_menu.addSeparator()
        # # 3. 转到类型定义
        # self._add_action(go_menu,"&Type Definition","go_to_type_definition","Go to the type definition","Ctrl+Shift+F12")
        # self._add_action(go_menu,"&Bracket","go_to_bracket","Go to the matching bracket","Ctrl+Shift+\\")
        # go_menu.addSeparator()
        # # 5. 上一个问题
        # self._add_action(go_menu,"&Previous Problem","prev_problem","Go to the previous problem","F8")
        # # 6. 下一个问题
        # self._add_action(go_menu,"&Next Problem","next_problem","Go to the next problem","F9")
        # go_menu.addSeparator()

        run_menu = self.addMenu("&Run")
        self._add_action(run_menu,"Run Without &Debugging","run_without_debugging","Run Without Debugging","Ctrl+F5")
    
    def _add_action(self,menu:QMenu, text:str, action_name:str, status_tip:str="", shortcut:str="",is_checkable = False,is_checked = False):
        """辅助方法：创建并添加动作到指定菜单"""
        action =QAction( text, self)
        action.setStatusTip( status_tip)
        if shortcut:
            action.setShortcut(QKeySequence(shortcut))
        if is_checkable:
            action.setCheckable(True)
            action.setChecked(is_checked)
        # 连接动作的触发信号到发射自定义信号的槽函数
        action.triggered.connect(lambda: self.action_triggered.emit(action_name))
        menu.addAction(action)
        return action
