# 项目总体结构与特点说明

## 0. 目录结构（文件树形式说明）

IDE 根目录
|-- [README.md]：项目简介、环境准备与启动方式说明
|-- [requirements.txt]：Python 依赖列表，用于 `pip install -r requirements.txt`
|-- [__init__.py]：使根目录成为 Python 包，结构兼容用
|-- [test]：测试与示例文件目录，用于在 IDE 中验证编辑、运行、搜索等功能
| |-- hello.py / test.c / test.txt 等：提供多种语言与类型的示例文件
|-- [my_ide]：IDE 主要逻辑实现区域
| |-- [main.py]：程序入口，创建 QApplication 并启动 MainWindow
| |-- [windows]：窗口相关模块
| | |-- [main_window.py]：主窗口类 MainWindow，负责整体界面布局与各组件的组织和调度
| |
| |-- [controllers]：控制器层，封装编辑器逻辑
| | |-- [editor_controller.py]：EditorController，负责撤销/重做、查找/替换、缩放、自动换行等编辑行为控制
| |
| |-- [components]：组件文件夹，存放各种 UI 组件
| | |-- [code_editor.py]：CodeEditor，扩展自 QPlainTextEdit，支持自动缩进等功能
| | |-- [file_tree.py]：FileTreeWidget，文件树视图，支持新建/删除文件夹和文件
| | |-- [activity_bar.py]：ActivityBar，左侧活动栏，用按钮切换资源管理器/搜索面板等视图
| | |-- [menu_bar.py]：MenuBar，自定义菜单栏，定义 File/Edit/View/Run 等菜单并发出统一动作事件
| | |-- [search_panel.py]：SearchPanel，多文件搜索面板，结合 SearchWorker 在线程中遍历文件并展示高亮结果
| | |-- [find_panel.py]：FindPanel，悬浮查找/替换面板，提供查找、上一条/下一条与替换全部等操作
| | |-- [output_bar.py]：OutputBar，底部终端/问题/输出综合面板
| | |-- [syntax_highlighter_customer.py]：CustomHighlighter，自定义语法高亮实现，结合 Pygments 样式
| |
| |-- [config]：配置与常量
| | |-- [settings.py]：忽略目录/文件后缀配置、默认语法高亮主题 DEFAULT_STYLE、默认前景/背景色等
| |
| |-- [core]：核心/编译相关数据
| | |-- [error_missing_brace.json]：示例错误 JSON，用于问题面板展示并支持跳转到对应代码行
| |
| |-- [resources]：静态资源
| | |-- [activity_bar]：活动栏与文件树工具按钮所用的图标资源

---

## 一、项目总体架构概览

本项目实现了一个简易集成开发环境（IDE），整体采用典型的「入口模块 + 主窗口 + 功能组件 + 控制器 + 配置/核心」分层结构。

- 入口层：

  - [main.py](main.py)
    - 提供程序入口 `main()`，初始化 `QApplication`，创建并显示主窗口 `MainWindow`，随后进入事件循环，是整个 IDE 的启动点。

- 主窗口与应用骨架：

  - [main_window.py](main_window.py)
    - 定义 `MainWindow(QMainWindow)`，负责整体 UI 布局与各个功能组件的挂载，包括：代码编辑器、菜单栏、侧边栏（文件树 + 搜索）、底部输出面板等。
    - 通过 `_init_editor()`、`_init_menu_bar()`、`_init_sidebar()`、`_init_find_panel()`、`_init_output_bar()`、`_init_controller()` 等方法，将不同子组件进行组合，是整个界面的「中枢神经」。

- 控制层（逻辑与编辑行为控制）：

  - [editor_controller.py](editor_controller.py)
    - 定义 `EditorController`，对文本编辑器的行为进行集中控制，如撤销/重做、剪切/复制/粘贴、缩放、自动换行、查找/替换等；
    - 将这些操作从 `MainWindow` 中抽离出来，降低主窗口的复杂度，是 IDE 编辑行为的「逻辑控制中心」。

- 组件层（UI 功能组件）：

  - 编辑器组件：
    - [code_editor.py](code_editor.py)
      - 定义 `CodeEditor(QPlainTextEdit)`，在原生编辑器基础上增加「自动缩进」等增强功能，是代码编辑体验的核心部件。
  - 文件树与侧边栏：
    - [file_tree.py](file_tree.py)
      - 定义 `FileTreeWidget`，使用 `QFileSystemModel + QTreeView` 实现文件/文件夹浏览、新建/删除等操作，是 IDE 的资源管理器。
    - [activity_bar.py](activity_bar.py)
      - 定义 `ActivityBar`，用图标按钮在「资源管理器」与「搜索面板」之间切换，类似 VS Code 左侧竖直侧边栏。
  - 菜单栏：
    - [menu_bar.py](menu_bar.py)
      - 定义 `MenuBar(QMenuBar)`，提供 File / Edit / Select / View / Run 等菜单，并将用户的点击转换为统一的字符串事件（如 `file_open`、`edit_find` 等）发给主窗口处理。
  - 搜索与查找替换：
    - [search_panel.py](search_panel.py)
      - 定义 `SearchPanel`，包含 `SearchWorker` 后台线程，实现对工作目录的多文件搜索（支持正则 / 严格匹配 / 大小写可选），并以树形结构展示结果。
    - [find_panel.py](find_panel.py)
      - 定义 `FindPanel`，在当前编辑器内进行查找与替换（带上一条/下一条、大小写/整词匹配等），与 `EditorController` 配合实现文本搜索高亮与替换。
  - 输出与终端：
    - [output_bar.py](output_bar.py)
      - 定义 `OutputBar`，包含三个标签页：终端（结合 `termqt`）、问题列表（诊断信息）、输出文本区；支持点击问题行跳转对应代码位置。
  - 语法高亮：
    - [syntax_highlighter_customer.py](syntax_highlighter_customer.py)
      - 定义 `CustomHighlighter(QSyntaxHighlighter)`，结合 Pygments 词法分析和主题风格，实现对代码的语法着色。

- 配置与核心数据：

  - [settings.py](settings.py)
    - 存放搜索忽略目录/后缀、默认语法高亮主题 `DEFAULT_STYLE`、默认背景/文本颜色等，是 IDE 的全局配置中心。
  - [error_missing_brace.json](error_missing_brace.json)
    - 用于模拟/承载编译错误信息，供主窗口的自动「编译检测」定时任务读取，在问题面板中展示。

- 测试与样例：
  - [test](test) 目录
    - 包含若干简单脚本与示例文件，用于在 IDE 中打开、编辑、运行时进行功能测试。

## 二、模块间协同工作方式

1. 应用启动流程

- 用户运行 `python -m my_ide.main`：
  - [main.py](main.py) 中的 `main()` 函数创建 `QApplication`，并实例化 `MainWindow`；
  - `MainWindow` 构造函数中依次调用 `init_ui()`、`_init_find_panel()`、`_init_output_bar()`、`_init_controller()` 等方法，搭建好 UI 框架与控制逻辑。

2. 主窗口与子组件交互

- 主窗口持有各组件实例：编辑器（`CodeEditor`）、菜单栏（`MenuBar`）、活动栏（`ActivityBar`）、文件树（`FileTreeWidget`）、搜索面板（`SearchPanel`）、查找面板（`FindPanel`）、底部输出栏（`OutputBar`）等；
- `MenuBar.action_triggered` 信号发出字符串标识，`MainWindow` 在 `_init_menu_bar()` 中连接到 `_handle_menu_action()`（在 [main_window.py](main_window.py) 内部），再通过 `self.action_handlers` 字典分发给具体的槽函数（文件打开、保存、运行、切换主题等）。

3. 编辑器逻辑与查找替换

- 文本编辑组件为 `CodeEditor`，其具体行为（撤销/重做/查找/缩放/自动换行等）由 `EditorController` 统一封装：
  - `MainWindow` 在 `_init_controller()` 中创建 `EditorController(self.editor)`；
  - 查找面板 `FindPanel` 发出的信号（`find_triggered`、`find_next_triggered`、`replace_all_triggered` 等）由 `MainWindow` 转而调用 `EditorController.edit_find()`、`find_next()`、`replace_all()` 等方法；
  - `EditorController` 利用 `QTextDocument` 的搜索能力和 `ExtraSelection` 实现多结果高亮和当前结果高亮。

4. 文件树与搜索面板

- 文件树：
  - `FileTreeWidget` 中的按钮点击信号 `new_file_clicked`、`new_folder_clicked`、`delete_button_clicked` 由主窗口在 `_init_sidebar()` 中连接到 `_on_new_file()`、`_on_new_folder()`、`_on_file_delete()` 等槽函数，真正执行文件系统操作；
  - 双击树节点时，主窗口在 `_on_file_double_clicked()` 中打开对应文件，并将内容加载到编辑器显示。
- 搜索面板：
  - `SearchPanel` 通过 `SearchWorker` 在线程中遍历目录文件（忽略特定目录/后缀，读取配置自 [settings.py](settings.py)），将匹配结果以 `match_found` 信号返回；
  - UI 端使用 `QTreeWidget` 显示结果，双击结果行通过 `result_clicked` 信号通知主窗口；
  - 主窗口在 `_on_search_result_clicked()` 中打开对应文件，并跳转到匹配位置。

5. 终端与问题面板

- 终端：
  - `OutputBar` 内部使用 `termqt.Terminal` 与 `TerminalWinptyIO` 创建一个嵌入式命令行终端，可直接在 IDE 中输入命令；
  - 主窗口中与“Run With Terminal / Run Without Terminal”菜单项关联的槽函数，会调用 `OutputBar` 的方法以及内部的后台线程 `ProcessWorker` 来执行外部命令，并通过信号实时输出到「终端」页签或「输出」页签。
- 问题面板：
  - `MainWindow` 中的 `compiler_timer` 每 10 秒调用 `_run_compiler_cycle()`，从 [error_missing_brace.json](error_missing_brace.json) 读取错误信息，并调用 `OutputBar.add_problem()` 更新「问题」页；
  - 用户点击某一问题行时，`OutputBar.problem_clicked` 信号触发 `MainWindow._jump_to_problem_location()`，在编辑器中跳转到相应行。

6. 语法高亮与主题切换

- `MenuBar` 的 Syntax Style 子菜单通过信号 `style_changed`（参数为风格名字，如 `default`、`monokai` 等），通知主窗口用户选择的新样式；
- 主窗口响应 `_on_style_changed()` 时：
  - 使用 Pygments 的 `get_lexer_for_filename` / `lexers` 获取合适的 lexer；
  - 构造 `CustomHighlighter` 实例，并根据 [settings.py](settings.py) 中的 `DEFAULT_STYLE` 或者 Pygments 内置样式，动态调整编辑器背景色和字体颜色，实现「类 VS Code」的多主题语法高亮效果。

## 三、本项目的特色与可截图重点

下面列出几个较有特色、比较适合在报告中截图展示的功能点，并指出相应的文件、函数或代码片段位置，方便你在 IDE 中打开后截图。

### 1. 自动缩进的增强代码编辑器

- 相关文件：
  - [code_editor.py](code_editor.py)
- 关键类/函数：
  - 类 `CodeEditor(QPlainTextEdit)`
  - 方法 `keyPressEvent(self, event)`
- 特色说明：
  - 在用户按下回车键时，根据上一行的前导空白和结尾字符（如 `{`、`(`、`:`）自动计算新行缩进，模拟主流 IDE 的智能缩进行为；
  - 使用正则 `re.match(r'^(\s*)', prev_line_text)` 提取缩进，再根据语句结构追加缩进级别。
- 建议截图内容：
  - 类定义与 `keyPressEvent` 方法核心逻辑代码（上半屏显示类与构造，下半屏显示自动缩进逻辑）。

### 2. 多文件异步搜索与高亮结果展示

- 相关文件：
  - [search_panel.py](search_panel.py)
- 关键类/函数：
  - 后台线程类 `SearchWorker`（方法 `run_search`）
  - UI 类 `SearchPanel`（方法 `start_search`、`add_match`）
- 特色说明：
  - 使用 `QThread + QObject` 的组合让 `SearchWorker` 在后台遍历文件系统，不阻塞主界面；
  - 利用配置中 `ignored_dirs`、`ignored_exts` 进行过滤（配置定义见 [settings.py](settings.py)）；
  - 搜索结果在 `QTreeWidget` 中以「文件 → 多行命中」树形结构展示，每一行结果用富文本 `<span style="background-color: yellow;">` 高亮匹配片段。
- 建议截图内容：
  1. `SearchWorker.run_search` 中的目录遍历与正则匹配逻辑代码；
  2. `add_match` 函数构造高亮 HTML 的部分，以及结果树结构（父子项）创建代码。

### 3. 查找/替换面板与多结果高亮

- 相关文件：
  - [find_panel.py](find_panel.py)
  - [editor_controller.py](editor_controller.py)
- 关键类/函数：
  - `FindPanel` 中的 `_init_ui`、`_connect_signals`、`update_results_label` 等；
  - `EditorController` 中的 `edit_find`、`find_next`、`find_previous`、`replace_all`、`_highlight_all_matches`。
- 特色说明：
  - UI 层将查找、下一条、上一条、替换全部封装在一个悬浮小面板中，与主编辑器松耦合；
  - 逻辑层在 `EditorController` 中集中处理：
    - 使用 `QTextDocument.find` 收集所有匹配结果；
    - 使用 `QTextEdit.ExtraSelection` 列表实现「所有匹配高亮 + 当前匹配高亮」的效果；
    - `replace_all` 通过「从后往前遍历匹配结果」以及 `beginEditBlock()/endEditBlock()`，保证一次性撤销与位置不偏移。
- 建议截图内容：
  1. `FindPanel` 的 UI 布局代码和信号连接部分（展示自定义查找面板的设计）；
  2. `EditorController._highlight_all_matches` 与 `replace_all` 的实现（显示对多结果高亮和批量替换的处理方式）。

### 4. 集成终端 + 问题列表 + 输出面板

- 相关文件：
  - [output_bar.py](output_bar.py)
  - [main_window.py](main_window.py)
  - [error_missing_brace.json](error_missing_brace.json)
- 关键类/函数：
  - `OutputBar.init_ui` 中关于 `Terminal`、`QTableWidget`（问题）和 `QPlainTextEdit`（输出）的三标签页初始化；
  - `OutputBar.add_problem`、`clear_problems` 和 `_on_problem_row_clicked`；
  - `MainWindow._run_compiler_cycle` 与 `_parse_problems_and_update_ui`。
- 特色说明：
  - 使用第三方库 `termqt` 在 Qt 界面中嵌入一个可交互的命令行终端；
  - 周期性检查错误 JSON 文件，将错误列表显示到「问题」面板，并实现点击行后编辑器自动跳转的功能；
  - 形成「运行 → 输出/问题 → 点击问题回跳代码」的闭环体验。
- 建议截图内容：
  1. `OutputBar` 中终端 + 问题 + 输出三标签页的布局代码；
  2. `_on_problem_row_clicked` 与 `MainWindow._jump_to_problem_location` 的配合逻辑（从表格行到编辑器光标的跳转）。

### 5. 自定义/多主题语法高亮

- 相关文件：
  - [syntax_highlighter_customer.py](syntax_highlighter_customer.py)
  - [settings.py](settings.py)
  - [menu_bar.py](menu_bar.py)
- 关键类/函数：
  - `CustomHighlighter` 构造函数以及 `_create_formats_from_dict`、`_create_formats_from_pygments_style`、`highlightBlock`；
  - 配置中的 `DEFAULT_STYLE`、`DEFAULT_BACKGROUND_COLOR`、`DEFAULT_TEXT_COLOR`；
  - 菜单栏中 Syntax Style 子菜单的构建和 `style_changed` 信号发射处。
- 特色说明：
  - 支持两种来源的高亮方案：
    1. 自定义字典 `DEFAULT_STYLE`（仿 VS 风格）；
    2. Pygments 内置样式（如 `monokai`，`solarized-dark` 等）；
  - 动态根据 Token 类型（`Keyword`、`String`、`Comment` 等）设置前景色、背景色和粗体/斜体等属性，实现可切换主题的语法高亮系统。
- 建议截图内容：
  1. `DEFAULT_STYLE` 字典中对不同 Token 的颜色配置；
  2. `CustomHighlighter.highlightBlock` 中对 Pygments token 的处理逻辑。

### 6. 菜单驱动的统一动作分发机制

- 相关文件：
  - [menu_bar.py](menu_bar.py)
  - [main_window.py](main_window.py)
- 关键类/函数：
  - `MenuBar._init_menu_content` 和 `_add_action`；
  - `MainWindow._init_menu_bar` 和 `self.action_handlers` 字典；
- 特色说明：
  - 所有菜单项在 `_add_action` 中统一创建，将动作名（如 `file_open`、`zoom_in`、`toggle_dark_theme`）以字符串形式通过 `action_triggered` 信号抛出；
  - 主窗口维护一个 `action_handlers` 字典，将字符串映射到实际槽函数，实现「动作名 → 逻辑处理」的集中管理，便于扩展与维护。
- 建议截图内容：
  1. `MenuBar._add_action` 的实现代码；
  2. `MainWindow._init_controller` 中对 `action_handlers` 的构造与各种动作的映射表。

---

以上内容可以直接作为课程设计报告中「系统总体设计」「模块划分与协同」「系统特色功能展示」部分的文字基础。你可以根据需要：

- 在文本描述旁配合上述定位好的代码截图；
- 另外再补充几张实际运行界面（主窗口、文件树、搜索结果、终端和问题面板等）的运行截图，即可形成一份比较完整的 IDE 项目说明。
