import sys
import os
from PySide6.QtWidgets import QMenuBar, QMenu
from PySide6.QtGui import QAction, QKeySequence
from PySide6.QtCore import Signal


class MenuBar(QMenuBar):
    action_triggered = Signal(str)

    def __init__(self, main_window_instance):
        super().__init__(main_window_instance)
        self.main_window = main_window_instance
        self._init_menu_content()
    
    def _init_menu_content(self):

        file_menu = self.addMenu("&File")

        # --- 创建 "File" 菜单下的动作 (Actions) ---
        # 1. 打开文件的动作
        open_action = QAction("&Open", self)
        open_action.setStatusTip("Open a file")
        open_action.triggered.connect(self.main_window._on_file_open) 

        # 2. 保存文件的动作
        save_action = QAction("&Save", self)
        save_action.setStatusTip("Save the current file")
        save_action.triggered.connect(self.main_window._on_file_save)

        # 3. 退出的动作
        exit_action = QAction("&Exit", self)
        exit_action.setStatusTip("Exit the application")
        exit_action.triggered.connect(self.main_window.close)

        # --- 将动作添加到 "File" 菜单中 ---
        file_menu.addAction(open_action)
        file_menu.addAction(save_action)
        file_menu.addSeparator() # 添加一条分割线
        file_menu.addAction(exit_action)
    
        edit_menu = self.addMenu("&Edit")
    
        # 1. 撤销的动作
        undo_action = QAction("&Undo", self)
        undo_action.setStatusTip("Undo the last action")
        undo_action.setShortcut(QKeySequence("Ctrl+Z"))
        undo_action.triggered.connect(self.main_window._on_edit_undo) 

        # 2. 恢复的动作
        redo_action = QAction("&Redo", self)
        redo_action.setStatusTip("Redo the undone action")
        redo_action.setShortcut(QKeySequence("Ctrl+Y"))
        redo_action.triggered.connect(self.main_window._on_edit_redo)

        # 3. 剪切的动作
        cut_action = QAction("Cu&t", self)
        cut_action.setStatusTip("Cut the selected content")
        cut_action.setShortcut(QKeySequence("Ctrl+X"))
        cut_action.triggered.connect(self.main_window._on_edit_cut)

        # 4. 复制的动作
        copy_action = QAction("&Copy", self)
        copy_action.setStatusTip("Copy the selected content")
        copy_action.setShortcut(QKeySequence("Ctrl+C"))
        copy_action.triggered.connect(self.main_window._on_edit_copy)

        # 5.粘贴的动作
        paste_action = QAction("&Paste", self)
        paste_action.setStatusTip("Paste content from clipboard")
        paste_action.setShortcut(QKeySequence("Ctrl+V"))
        paste_action.triggered.connect(self.main_window._on_edit_paste)

        # --- 将动作添加到 "Edit" 菜单中 ---
    
        edit_menu.addAction(undo_action)
        edit_menu.addAction(redo_action)
        edit_menu.addSeparator()
        edit_menu.addAction(cut_action)
        edit_menu.addAction(copy_action)
        edit_menu.addAction(paste_action)

        select_menu = self.addMenu("Selec&t")

        # 1.全选
        select_all_action = QAction("&Select All", self)
        select_all_action.setStatusTip("Select all content")
        select_all_action.setShortcut(QKeySequence.SelectAll) 
        select_all_action.triggered.connect(self.main_window._on_select_all)

        # 2.重复选择
        repeat_selection_action = QAction("&Repeat Selection", self)
        repeat_selection_action.setStatusTip("Repeat the last selection")
        repeat_selection_action.setShortcut(QKeySequence("Ctrl+D")) 
        repeat_selection_action.triggered.connect(self.main_window._on_repeat_selection)

        # 3.选择所有匹配项
        select_all_matches_action = QAction("Select All Matches", self)
        select_all_matches_action.setStatusTip("Select all items matching the current selection")
        select_all_matches_action.setShortcut(QKeySequence("Ctrl+Shift+L")) 
        select_all_matches_action.triggered.connect(self.main_window._on_select_all_matches) 

        select_menu.addAction(select_all_action)
        select_menu.addSeparator()
        select_menu.addAction(repeat_selection_action)
        select_menu.addSeparator() 
        select_menu.addAction(select_all_matches_action)

        view_menu = self.addMenu("&View")

        appearance_menu = QMenu("&Appearance", self)

        # a.全屏
        fullscreen_action = QAction("&Fullscreen", self)
        fullscreen_action.setStatusTip("Toggle Fullscreen")
        fullscreen_action.setShortcut(QKeySequence("F11")) 
        fullscreen_action.triggered.connect(self.main_window._on_toggle_fullscreen)
        appearance_menu.addAction(fullscreen_action)


        # b.菜单栏
        menubar_action = QAction("&Menu Bar", self)
        menubar_action.setStatusTip("Toggle Menu Bar visibility")
        menubar_action.setCheckable(True) 
        menubar_action.setChecked(True)
        menubar_action.triggered.connect(self.main_window._on_toggle_menubar)
        appearance_menu.addAction(menubar_action)

        # c.主侧边栏
        sidebar_action = QAction("&Primary Side Bar", self)
        sidebar_action.setStatusTip("Toggle Primary Side Bar visibility")
        sidebar_action.setShortcut(QKeySequence("Ctrl+B"))
        sidebar_action.setCheckable(True) 
        sidebar_action.setChecked(True) 
        sidebar_action.triggered.connect(self.main_window._on_toggle_sidebar)
        appearance_menu.addAction(sidebar_action)
    
        appearance_menu.addSeparator() 

        # d.放大
        zoom_in_action = QAction("Zoom &In", self)
        zoom_in_action.setStatusTip("Increase editor zoom level")
        zoom_in_action.setShortcut(QKeySequence("Ctrl+="))
        zoom_in_action.triggered.connect(self.main_window._on_zoom_in)
        appearance_menu.addAction(zoom_in_action)

        # e.缩小
        zoom_out_action = QAction("Zoom &Out", self)
        zoom_out_action.setStatusTip("Decrease editor zoom level")
        zoom_out_action.setShortcut(QKeySequence("Ctrl+-")) 
        zoom_out_action.triggered.connect(self.main_window._on_zoom_out)
        appearance_menu.addAction(zoom_out_action)

        view_menu.addMenu(appearance_menu)

        # 创建 "Editor Layout"子菜单
        editor_layout_menu = QMenu("Editor &Layout", self)
    
        # a.单列
        single_column_action = QAction("&Single", self)
        single_column_action.setStatusTip("Use a single editor column layout")
        single_column_action.triggered.connect(lambda: self.main_window._on_set_editor_layout("single"))
        editor_layout_menu.addAction(single_column_action)

        # b.两列
        two_columns_action = QAction("&Two Columns", self)
        two_columns_action.setStatusTip("Use a two editor column layout")
        two_columns_action.triggered.connect(lambda: self.main_window._on_set_editor_layout("two_cols"))
        editor_layout_menu.addAction(two_columns_action)
    
        # 将 "Editor Layout" 子菜单添加到 "View" 菜单
        view_menu.addMenu(editor_layout_menu)
    
        view_menu.addSeparator() 

        # 3. 输出 (Output)
        output_action = QAction("&Output", self)
        output_action.setStatusTip("Toggle Output Panel visibility")
        output_action.setShortcut(QKeySequence("Ctrl+Shift+U"))
        output_action.setCheckable(True) 
        output_action.triggered.connect(self.main_window._on_toggle_output)
        view_menu.addAction(output_action)

        # 4. 自动换行 (Word Wrap)
        word_wrap_action = QAction("Word &Wrap", self)
        word_wrap_action.setStatusTip("Toggle Word Wrap")
        word_wrap_action.setShortcut(QKeySequence("Alt+Z"))
        word_wrap_action.setCheckable(True) 
        word_wrap_action.triggered.connect(self.main_window._on_toggle_word_wrap)
        view_menu.addAction(word_wrap_action)

        go_menu = self.addMenu("&Go")

        # 1. 返回
        go_back_action = QAction("&Back", self)
        go_back_action.setStatusTip("Go back to the previous location")
        go_back_action.setShortcut(QKeySequence("Alt+Left"))
        go_back_action.triggered.connect(self.main_window._on_go_back)
        go_menu.addAction(go_back_action)

        # 2. 前进
        go_forward_action = QAction("&Forward", self)
        go_forward_action.setStatusTip("Go forward to the next location")
        go_forward_action.setShortcut(QKeySequence("Alt+Right"))
        go_forward_action.triggered.connect(self.main_window._on_go_forward)
        go_menu.addAction(go_forward_action)

        go_menu.addSeparator()

        # 3. 转到类型定义
        go_to_type_definition_action = QAction("Go to &Type Definition", self)
        go_to_type_definition_action.setStatusTip("Go to the type definition")
        go_to_type_definition_action.setShortcut(QKeySequence("Ctrl+Shift+F12"))
        go_to_type_definition_action.triggered.connect(self.main_window._on_go_to_type_definition)
        go_menu.addAction(go_to_type_definition_action)

        # 4. 转到括号
        go_to_bracket_action = QAction("Go to &Bracket", self)
        go_to_bracket_action.setStatusTip("Go to the matching bracket")
        go_to_bracket_action.setShortcut(QKeySequence("Ctrl+Shift+\\"))
        go_to_bracket_action.triggered.connect(self.main_window._on_go_to_bracket)
        go_menu.addAction(go_to_bracket_action)

        go_menu.addSeparator()

        # 5. 上一个问题
        prev_problem_action = QAction("&Previous Problem", self)
        prev_problem_action.setStatusTip("Go to the previous problem")
        prev_problem_action.setShortcut(QKeySequence("F8"))
        prev_problem_action.triggered.connect(self.main_window._on_prev_problem)
        go_menu.addAction(prev_problem_action)

        # 6. 下一个问题
        next_problem_action = QAction("&Next Problem", self)
        next_problem_action.setStatusTip("Go to the next problem")
        next_problem_action.setShortcut(QKeySequence("F9"))
        next_problem_action.triggered.connect(self.main_window._on_next_problem)
        go_menu.addAction(next_problem_action)

        go_menu.addSeparator()

        # 7. 上一个更改
        prev_change_action = QAction("Previous &Change", self)
        prev_change_action.setStatusTip("Go to the previous change")
        prev_change_action.setShortcut(QKeySequence("Alt+Shift+F3"))
        prev_change_action.triggered.connect(self.main_window._on_prev_change)
        go_menu.addAction(prev_change_action)

        # 8. 下一个更改
        next_change_action = QAction("Next C&hange", self)
        next_change_action.setStatusTip("Go to the next change")
        next_change_action.setShortcut(QKeySequence("Alt+F3"))
        next_change_action.triggered.connect(self.main_window._on_next_change)
        go_menu.addAction(next_change_action)

        run_menu = self.addMenu("&Run")
        
        # 1. 启动调试
        start_debugging_action = QAction("Start &Debugging", self)
        start_debugging_action.setStatusTip("Start Debugging")
        start_debugging_action.setShortcut(QKeySequence("F5")) 
        start_debugging_action.triggered.connect(self.main_window._on_start_debugging)
        run_menu.addAction(start_debugging_action)

        # 2. 以非调试模式运行
        run_without_debugging_action = QAction("Run Without &Debugging", self)
        run_without_debugging_action.setStatusTip("Run Without Debugging")
        run_without_debugging_action.setShortcut(QKeySequence("Ctrl+F5")) 
        run_without_debugging_action.triggered.connect(self.main_window._on_run_without_debugging)
        run_menu.addAction(run_without_debugging_action)



