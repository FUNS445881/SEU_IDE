from pygments.token import (
    Token, Comment, Keyword, Name, String, Error, Number, Operator,
    Punctuation, Generic, Literal
)

# 过滤硬编码
ignored_dirs = {'.git', 'node_modules', '__pycache__', 'venv', '.vscode'}
ignored_exts = {
    '.pyc', '.pyo', '.o', '.so', # 编译文件
    '.dll', '.exe',             # 可执行文件
    '.db', '.sqlite3',          # 数据库文件
    '.img', '.iso', '.bin',     # 镜像文件
    '.jpg', '.jpeg', '.png', '.gif' # 图片
}

# 这是你自定义的默认亮色主题
DEFAULT_STYLE = {
    Token:              '#000000',
    Comment:            '#008000',
    Comment.Preproc:    '#0000FF',
    Keyword:            '#0000FF bold',
    Keyword.Constant:   '#0000FF',
    Keyword.Declaration:'#0000FF',
    Keyword.Namespace:  '#0000FF',
    Keyword.Pseudo:     '#0000FF',
    Keyword.Reserved:   '#0000FF',
    Keyword.Type:       '#0000FF',
    
    Operator:           '#A52A2A',
    Operator.Word:      '#0000FF bold', # e.g., 'and', 'or', 'not'
    
    Punctuation:        '#000000',

    Name:               '#000000',
    Name.Builtin:       '#0000FF',
    Name.Function:      '#795E26',
    Name.Class:         '#267f99 bold',
    Name.Variable:      '#001080',
    Name.Constant:      '#001080',
    Name.Attribute:     '#001080',
    Name.Tag:           '#800000',
    
    String:             '#A31515',
    String.Doc:         '#A31515',
    String.Interpol:    '#A31515',
    String.Escape:      '#A31515',
    
    Number:             '#098658',
    Literal:            '#098658',
    
    Error:              '#FF0000 bold',
}

# 为你的默认主题定义背景色和默认文本颜色
DEFAULT_BACKGROUND_COLOR = "#FFFFFF"
DEFAULT_TEXT_COLOR = "#000000"