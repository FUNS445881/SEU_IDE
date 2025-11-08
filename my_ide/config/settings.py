# 过滤硬编码
ignored_dirs = {'.git', 'node_modules', '__pycache__', 'venv', '.vscode'}
ignored_exts = {
    '.pyc', '.pyo', '.o', '.so', # 编译文件
    '.dll', '.exe',             # 可执行文件
    '.db', '.sqlite3',          # 数据库文件
    '.img', '.iso', '.bin',     # 镜像文件
    '.jpg', '.jpeg', '.png', '.gif' # 图片
}

