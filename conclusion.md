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
