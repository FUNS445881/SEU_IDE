/* 由 SeuYacc 生成的 LR(1) 解析器头文件 */

#ifndef MINIC_TAB_H_INCLUDED
# define MINIC_TAB_H_INCLUDED
/* 调试跟踪设置 */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* 令牌类型定义 */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEOF = 0,                     /* "文件结束" */
    MPR = 256,
    UMINUS = 257,
    ELSE = 258,
    INT = 259,
    AND = 260,
    GE = 261,
    LSHIFT = 262,
    RSHIFT = 263,
    NE = 264,
    CONTINUE = 265,
    EQ = 266,
    BREAK = 267,
    DECNUM = 268,
    RETURN = 269,
    WHILE = 270,
    LE = 271,
    IF = 272,
    OR = 273,
    VOID = 274,
    IDENT = 275,
    HEXNUM = 276,
  };
#endif

/* 令牌定义宏 */
#define YYEOF 0
#define MPR 256
#define UMINUS 257
#define ELSE 258
#define INT 259
#define AND 260
#define GE 261
#define LSHIFT 262
#define RSHIFT 263
#define NE 264
#define CONTINUE 265
#define EQ 266
#define BREAK 267
#define DECNUM 268
#define RETURN 269
#define WHILE 270
#define LE 271
#define IF 272
#define OR 273
#define VOID 274
#define IDENT 275
#define HEXNUM 276

/* 值类型定义 */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
    char* str_val;
    int int_val;
    struct ASTNode* node;
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


/* 外部变量声明 */
extern YYSTYPE yylval;


/* 解析函数声明 */
int yyparse(void);


#endif /* !MINIC_TAB_H_INCLUDED */
