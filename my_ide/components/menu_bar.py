import sys
import os
from PySide6.QtWidgets import QMenuBar, QMenu
from PySide6.QtGui import QAction
from PySide6.QtCore import Signal
from PyQt5.QtGui import QKeySequence

class MenuBar(QMenuBar):
    action_triggered = Signal(str)

    def __init__(self):
        super().__init__()
        self.init_meun()
    
    def _init_menu_bar(self):
       
        menuBar = self.menuBar()
    
        edit_menu = menuBar.addMenu("&Edit")
    
        # 1. 撤销的动作
        undo_action = QAction("&Undo", self)
        undo_action.setStatusTip("Undo the last action")
        undo_action.setShortcut(QKeySequence("Ctrl+Z"))
        undo_action.triggered.connect(self._on_edit_undo) 

        # 2. 恢复的动作
        redo_action = QAction("&Redo", self)
        redo_action.setStatusTip("Redo the undone action")
        redo_action.setShortcut(QKeySequence("Ctrl+Y"))
        redo_action.triggered.connect(self._on_edit_redo)

        # 3. 剪切的动作
        cut_action = QAction("Cu&t", self)
        cut_action.setStatusTip("Cut the selected content")
        cut_action.setShortcut(QKeySequence("Ctrl+X"))
        cut_action.triggered.connect(self._on_edit_cut)

        # 4. 复制的动作
        copy_action = QAction("&Copy", self)
        copy_action.setStatusTip("Copy the selected content")
        copy_action.setShortcut(QKeySequence("Ctrl+C"))
        copy_action.triggered.connect(self._on_edit_copy)

        # 5.粘贴的动作
        paste_action = QAction("&Paste", self)
        paste_action.setStatusTip("Paste content from clipboard")
        paste_action.setShortcut(QKeySequence("Ctrl+V"))
        paste_action.triggered.connect(self._on_edit_paste)

        # --- 将动作添加到 "Edit" 菜单中 ---
    
        edit_menu.addAction(undo_action)
        edit_menu.addAction(redo_action)
        edit_menu.addSeparator()
        edit_menu.addAction(cut_action)
        edit_menu.addAction(copy_action)
        edit_menu.addAction(paste_action)

        select_menu = menuBar.addMenu("Selec&t")

        # 1.全选
        select_all_action = QAction("&Select All", self)
        select_all_action.setStatusTip("Select all content")
        select_all_action.setShortcut(QKeySequence.SelectAll) 
        select_all_action.triggered.connect(self._on_select_all)

        # 2.重复选择
        repeat_selection_action = QAction("&Repeat Selection", self)
        repeat_selection_action.setStatusTip("Repeat the last selection")
        repeat_selection_action.setShortcut(QKeySequence.SelectAll) 
        repeat_selection_action.triggered.connect(self._on_repeat_selection)

        # 3.选择所有匹配项
        select_all_matches_action = QAction("Select All Matches", self)
        select_all_matches_action.setStatusTip("Select all items matching the current selection")
        select_all_matches_action.setShortcut(QKeySequence("Ctrl+Shift+L")) 
        select_all_matches_action.triggered.connect(self._on_select_all_matches) 

        select_menu.addAction(select_all_action)
        select_menu.addSeparator()
        select_menu.addAction(repeat_selection_action)
        select_menu.addSeparator() 
        select_menu.addAction(select_all_matches_action)

        view_menu = menuBar.addMenu("&View")

        appearance_menu = QMenu("&Appearance", self)

        # a.全屏
        fullscreen_action = QAction("&Fullscreen", self)
        fullscreen_action.setStatusTip("Toggle Fullscreen")
        fullscreen_action.setShortcut(QKeySequence("F11")) 
        fullscreen_action.triggered.connect(self._on_toggle_fullscreen)
        appearance_menu.addAction(fullscreen_action)

        select_menu.addSeparator()

        # b.菜单栏
        menubar_action = QAction("&Menu Bar", self)
        menubar_action.setStatusTip("Toggle Menu Bar visibility")
        menubar_action.setCheckable(True) 
        menubar_action.setChecked(True)
        menubar_action.triggered.connect(self._on_toggle_menubar)
        appearance_menu.addAction(menubar_action)

        # c.主侧边栏
        sidebar_action = QAction("&Primary Side Bar", self)
        sidebar_action.setStatusTip("Toggle Primary Side Bar visibility")
        sidebar_action.setShortcut(QKeySequence("Ctrl+B"))
        sidebar_action.setCheckable(True) 
        sidebar_action.setChecked(True) 
        sidebar_action.triggered.connect(self._on_toggle_sidebar)
        appearance_menu.addAction(sidebar_action)
    
        appearance_menu.addSeparator() 

        # d.放大
        zoom_in_action = QAction("Zoom &In", self)
        zoom_in_action.setStatusTip("Increase editor zoom level")
        zoom_in_action.setShortcut(QKeySequence("Ctrl+="))
        zoom_in_action.triggered.connect(self._on_zoom_in)
        appearance_menu.addAction(zoom_in_action)

        # e.缩小
        zoom_out_action = QAction("Zoom &Out", self)
        zoom_out_action.setStatusTip("Decrease editor zoom level")
        zoom_out_action.setShortcut(QKeySequence("Ctrl+-")) 
        zoom_out_action.triggered.connect(self._on_zoom_out)
        appearance_menu.addAction(zoom_out_action)

        view_menu.addMenu(appearance_menu)

        # 创建 "Editor Layout"子菜单
        editor_layout_menu = QMenu("Editor &Layout", self)
    
        # a.单列
        single_column_action = QAction("&Single", self)
        single_column_action.setStatusTip("Use a single editor column layout")
        single_column_action.triggered.connect(lambda: self._on_set_editor_layout("single")) # 假设连接
        editor_layout_menu.addAction(single_column_action)

        # b.两列
        two_columns_action = QAction("&Two Columns", self)
        two_columns_action.setStatusTip("Use a two editor column layout")
        two_columns_action.triggered.connect(lambda: self._on_set_editor_layout("two_cols")) # 假设连接
        editor_layout_menu.addAction(two_columns_action)
    
        # 将 "Editor Layout" 子菜单添加到 "View" 菜单
        view_menu.addMenu(editor_layout_menu)
    
        view_menu.addSeparator() 

        # 3. 输出 (Output)
        output_action = QAction("&Output", self)
        output_action.setStatusTip("Toggle Output Panel visibility")
        output_action.setShortcut(QKeySequence("Ctrl+Shift+U")) # 参考 VS Code 快捷键
        output_action.setCheckable(True) 
        output_action.triggered.connect(self._on_toggle_output) # 假设连接
        view_menu.addAction(output_action)

        # 4. 自动换行 (Word Wrap)
        word_wrap_action = QAction("Word &Wrap", self)
        word_wrap_action.setStatusTip("Toggle Word Wrap")
        word_wrap_action.setShortcut(QKeySequence("Alt+Z")) # 参考 VS Code 快捷键
        word_wrap_action.setCheckable(True) 
        word_wrap_action.triggered.connect(self._on_toggle_word_wrap) # 假设连接
        view_menu.addAction(word_wrap_action)