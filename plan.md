### Part 1: 项目文件结构
```
my_ide/
│
├── my_ide/               # 核心源代码包 (Python Package)
│   │
│   ├── __init__.py         # 标记这是一个 Python 包
│   │
│   ├── main.py             # 应用程序入口，创建和显示主窗口
│   │
│   ├── components/         # 可重用的 UI 组件 (Widgets)
│   │   ├── __init__.py
│   │   ├── file_tree.py      # 文件树组件
│   │   └── output_bar.py     # 底部控制台组件
│   │
│   ├── controllers/        # 一些组件的控制逻辑
│   │   ├── __init__.py
│   │   └── editor_controller.py   # 对QPlainTextEdit的控制，如查找，全选，回退等操作的支持
│   │
│   ├── resources/            # 资源文件 (如图标、UI 文件)
│   │   ├── icons/            # .png, .svg 图标(暂命名文件夹)
│   │   │   ├── run.png
│   │   │   └── save.png
│   │   
│   │
│   └── windows/              # 应用程序的主窗口和对话框
│       ├── __init__.py
│       └── main_window.py    # 主窗口的类，负责组装所有组件和处理信号
│
├── tests/                # 单元测试和集成测试
│   ├── __init__.py
│   ├── hello.py          # 测试终端指令
|   └── test.c            # 高光测试文件
│
├── scripts/              # 辅助脚本
│   └── build.py          # (可选) 打包应用的脚本
│
├── requirements.txt      # 项目依赖列表 (pip install -r requirements.txt)
│
└── README.md             # 项目说明文件
```

#### 各文件夹职责详解：

*   **`my_ide/` (根目录):** 项目的根。
*   **`my_ide/my_ide/` (源代码包):** 这是所有 Python 源代码存放的地方。
*   **`main.py`:** 程序的唯一入口。它的职责非常简单：创建 QApplication 实例，创建主窗口 `MainWindow` 的实例，然后显示窗口并启动事件循环。
*   **`components/`:** 存放**自包含的、可重用的 UI 模块**。
*   **`controllers/`:** 存放模块的控制代码
*   **`resources/`:** 存放所有非代码资源。
    *   `icons/`: 图标。
*   **`windows/`:** 存放构成应用主要界面的“容器”类。`main_window.py` 就是“总装车间”，它会创建`FileTree` 等组件的实例，并将它们放置在正确的位置，连接它们的信号和槽。
*   **`tests/`:** 测试代码。
*   **`requirements.txt`:** 项目依赖