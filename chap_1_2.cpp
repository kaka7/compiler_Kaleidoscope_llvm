#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

/*
//#
如果你对语言和编译器设计有兴趣的话，研究LLVM是十分必要的
我们将从一个简单的框架开始，让你可以扩展到其它语言。你也可以用教程中的代码测试llvm中的各种特性。大量涉及到语言设计和LLVM使用技巧
教程是关于编译器和LLVM的具体实现,，*不包含*教授现代化和理论化的软件工程原理。这意味着在实践中我们使用了一些并不科学但是简单的方法来介绍我们的做法。
比如，内存泄露，大量使用全局变量，不好的设计模式如访客模式等……但是，它非常简单

*/

/*
//===---------------------------------------------------------------------===//
// chapter1 Lexer
从文本文件中读取程序代码，理解自己应该去做什么(从一连串的字符中理解我们定义的程序)
词法分析:扫描器,将输入分解为多个token
//===----------------------------------------------------------------------===//
*/

// The lexer returns tokens [0-255] if it is an unknown character, otherwise one
// of these for known things.

/*
//#
c语言
数据类型是一个64位浮点型（float）
*/

/*
//#
question
万花筒调用标准库函数（LLVM的JIT做到这个简直轻而易举）。这意味着您可以使用“extern”关键字，在第6章还有一个更有趣的例子，我们编写一个显示Mandelbrot集放大程序。你使用之前定义的函数（这在相互递归的函数中很有用）extern sin(arg);

*/

//token归纳起来分三类,定义开头,内容(标示符或数字),结尾
enum Token
{
  tok_eof = -1, //结尾

  // commands
  tok_def = -2,
  tok_extern = -3, //开头

  // primary
  tok_identifier = -4,
  tok_number = -5 //内容
};

static std::string IdentifierStr; // Filled in if tok_identifier 全局标示符
static double NumVal;             // Filled in if tok_number     全局数字 以及CurTok也是全局的

/// gettok - Return the next token from standard input.
static int gettok()
{
  static int LastChar = ' ';

  // Skip any whitespace.忽略token之间的空白符 而不是tab
  while (isspace(LastChar))
    LastChar = getchar();

  //标示符字母开头后面可以有数字
  if (isalpha(LastChar))
  { // identifier: [a-zA-Z][a-zA-Z0-9]*
    IdentifierStr = LastChar;
    while (isalnum((LastChar = getchar())))
      IdentifierStr += LastChar;

    if (IdentifierStr == "def")
      return tok_def;
    if (IdentifierStr == "extern")
      return tok_extern;
    return tok_identifier;
  }
  //数字
  if (isdigit(LastChar) || LastChar == '.')
  { // Number: [0-9.]+
    std::string NumStr;
    do
    {
      NumStr += LastChar;
      LastChar = getchar();
    } while (isdigit(LastChar) || LastChar == '.');

    NumVal = strtod(NumStr.c_str(), 0); //C中的strtod函数转化为数字
    return tok_number;
  }
  //注释
  if (LastChar == '#')
  {
    // Comment until end of line.
    do
      LastChar = getchar();
    while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');
    if (LastChar != EOF)
      return gettok();
  }
    // 文件结尾
  // Check for end of file.  Don't eat the EOF.
  if (LastChar == EOF)
    return tok_eof;

  // Otherwise, just return the character as its ascii value.
  int ThisChar = LastChar;
  LastChar = getchar();
  return ThisChar;
}
/*
//#
//===----------------------------------------------------------------------===//
//chapter2  Abstract Syntax Tree (aka Parse Tree)
内容:
AST
语法分析(递归下降法和运算符优先级分析):ast定义了解析中的各种类型(表达式，原型，函数),有解析器合并成有意义的树
    基础解析:标示符,数字,(),
    二元表达式:难,没看完
    def,extern的解析
    (词法分析器返回的每一个token可能是一个枚举值enum token或者是一个字符比如“+”，实际上这返回的是字符的ASCII码)

驱动程序:将词法分析器和语法分析器结合在一起,MainLoop,解析时调用相应的解析函数
文中的AST结构记录的信息是与语法是无关的，没有涉及到运算符的优先级和词法结构。
//===----------------------------------------------------------------------===//
*/

//===----------------------------------------------------------------------===//
// 1) Abstract Syntax Tree (aka Parse Tree)
//===----------------------------------------------------------------------===//

namespace
{
  // Kaleidoscope有表达式，原型，函数对象
/// ExprAST - Base class for all expression nodes.
class ExprAST
{
public:
  virtual ~ExprAST() {}
};

/// NumberExprAST - Expression class for numeric literals like "1.0".
class NumberExprAST : public ExprAST
{
public:
  NumberExprAST(double val) {}//文本转化为数字并存储起来
};


//三类;变量表达式,操作表达式,函数调用表达式
/// VariableExprAST - Expression class for referencing a variable, like "a".
class VariableExprAST : public ExprAST
{
  std::string Name;

public:
  VariableExprAST(const std::string &name) : Name(name) {}
};

/// BinaryExprAST - Expression class for a binary operator.
class BinaryExprAST : public ExprAST
{
public:
  BinaryExprAST(char op, ExprAST *lhs, ExprAST *rhs) {}//#表达式左右都是 ExprAST*
};

/// CallExprAST - Expression class for function calls.
class CallExprAST : public ExprAST
{
  std::string Callee;
  std::vector<ExprAST *> Args;//#Args也是ExprAST pointer

public:
  CallExprAST(const std::string &callee, std::vector<ExprAST *> &args)
      : Callee(callee), Args(args) {}
};

//函数原型ast,记录函数定义的name和args
/// PrototypeAST - This class represents the "prototype" for a function,
/// which captures its name, and its argument names (thus implicitly the number
/// of arguments the function takes).
class PrototypeAST
{
  std::string Name;
  std::vector<std::string> Args;

public:
  PrototypeAST(const std::string &name, const std::vector<std::string> &args)
      : Name(name), Args(args) {}
};
// 函数AST包含函数原型以及函数内容body
/// FunctionAST - This class represents a function definition itself.
class FunctionAST
{
public:
  FunctionAST(PrototypeAST *proto, ExprAST *body) {}
};
} // end anonymous namespace

/*
//===----------------------------------------------------------------------===//
// 2) Parser
递归下降解析器:ParseParenExpr,找成对出现的,如()
/// primary 基础表达式
///   ::= identifierexpr
///   ::= numberexpr
///   ::= parenexpr 括号,将表达式组合起来引导引导解析器正确地处理它们。当建立好了抽象语法树后，它们便可以被抛弃了

ExprAST *X = new VariableExprAST("x");
ExprAST *Y = new VariableExprAST("y");
ExprAST *Result = new BinaryExprAST('+', X, Y);
//===----------------------------------------------------------------------===//
*/

/// CurTok/getNextToken - Provide a simple token buffer.  CurTok is the current
/// token the parser is looking at.  getNextToken reads another token from the
/// lexer and updates CurTok with its results.
static int CurTok;
static int getNextToken()
{
  return CurTok = gettok();
}


static std::map<char, int> BinopPrecedence;//操作符的优先级

/// GetTokPrecedence - Get the precedence of the pending binary operator token.
static int GetTokPrecedence()
{
  if (!isascii(CurTok))
    return -1;

  // Make sure it's a declared binop.
  int TokPrec = BinopPrecedence[CurTok];
  if (TokPrec <= 0)
    return -1;
  return TokPrec;
}
//错误的辅助函数
/// Error* - These are little helper functions for error handling.
ExprAST *Error(const char *Str)
{
  fprintf(stderr, "Error: %s\n", Str);
  return 0;
}
PrototypeAST *ErrorP(const char *Str)
{
  Error(Str);
  return 0;
}

/// expression
///   ::= primary binoprhs
static ExprAST *ParseExpression()
{
  ExprAST *LHS = ParsePrimary();
  if (!LHS)
    return 0;

  return ParseBinOpRHS(0, LHS);
}

/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
static ExprAST *ParseIdentifierExpr()
{
  std::string IdName = IdentifierStr;

  getNextToken(); // eat identifier.

  if (CurTok != '(') // Simple variable ref. 是简单变量,决定当前的identifier是一个函数调用，还是一个变量。判断的方法是读取下一个token，若下一个token**不是**(，则这是函数调用这时候返回VariableExprAST，否则是使用变量，返回CallExprAST。
    return new VariableExprAST(IdName);

  // Call. 是函数调用
  getNextToken(); // eat (
  std::vector<ExprAST *> Args;
  if (CurTok != ')')//#这里面都是参数
  {
    while (1)
    {
      ExprAST *Arg = ParseExpression();
      if (!Arg)
        return 0;
      Args.push_back(Arg);

      if (CurTok == ')')
        break;

      if (CurTok != ',')
        return Error("Expected ')' or ',' in argument list");
      getNextToken();
    }
  }

  // Eat the ')'.
  getNextToken();

  return new CallExprAST(IdName, Args);
}

/// numberexpr ::= number 如果当前的token是数字(NumVal已更新),这调用
static ExprAST *ParseNumberExpr()
{
  ExprAST *Result = new NumberExprAST(NumVal);//全局变量
  getNextToken(); // consume the number
  return Result;//*Result
}

/// parenexpr ::= '(' expression ')'
//#括号并不会成为抽象语法树的组成部分，它的作用是将表达式组合起来引导引导解析器正确地处理它们。当建立好了抽象语法树后，它们便可以被抛弃了
static ExprAST *ParseParenExpr()//括号表达式
{ //默认当前的token是(,有可能末尾的token就不是)
  getNextToken(); // eat (.
  ExprAST *V = ParseExpression();//递归调用,处理嵌套,简洁 //todo ParsePrimary
  if (!V)
    return 0;

  if (CurTok != ')')
    return Error("expected ')'");
  getNextToken(); // eat ).
  return V;
}

/// primary
///   ::= identifierexpr
///   ::= numberexpr
///   ::= parenexpr
static ExprAST *ParsePrimary() //前置调用判断
{
  switch (CurTok)
  {
  default:
    return Error("unknown token when expecting an expression");
  case tok_identifier://#enum变量就是对应的值-1,-2,-3
    return ParseIdentifierExpr();
  case tok_number:
    return ParseNumberExpr();
  case '(':
    return ParseParenExpr();
  }
}

/// binoprhs 二元表达式解析
// 二义性 优先级
// 比如表达式a+b+(c+d)*e*f+g。解析器将这个字符串看做一串由二元运算符分隔的基本表达式。因此，它将先解析第一个基本表达式a，接着将解析到成对出现的[+, b] 
// [+, (c+d)] 
// [, e] [, f]和 [+, g]。因为括号也是基础表达式，不用担心解析器会对``(c+d)``出现困惑。
// 开始解析第一步，表达式是由第一个基础表达式和之后的一连串[运算符, 基础表达式]组成
///   ::= ('+' primary)* 
// 优先级数值被传入ParseBinOpRHS，凡是比这个优先级值低的运算符都不能被使用。比如如果当前的解析的是[+, x]，
// 且目前传入的优先级值为40，那么函数就不会消耗任何token（因为”+”优先级值仅20）是为我们解析*运算符-表达式*对的函数。它记录优先级和已解析部分的指针。
// 若当前的token已经不是运算符时，我们会获得一个无效的优先级值-1，它比任何一个运算符的优先级都小
static ExprAST *ParseBinOpRHS(int ExprPrec, ExprAST *LHS)
{
  // If this is a binop, find its precedence.
  while (1)
  {
    int TokPrec = GetTokPrecedence();

    // If this is a binop that binds at least as tightly as the current binop,
    // consume it, otherwise we are done.
    if (TokPrec < ExprPrec)
      return LHS;

    // Okay, we know this is a binop.
    int BinOp = CurTok;
    getNextToken(); // eat binop

    // Parse the primary expression after the binary operator.
    ExprAST *RHS = ParsePrimary();
    if (!RHS)
      return 0;

    // If BinOp binds less tightly with RHS than the operator after RHS, let
    // the pending operator take RHS as its LHS.
    int NextPrec = GetTokPrecedence();
    if (TokPrec < NextPrec)
    {
      RHS = ParseBinOpRHS(TokPrec + 1, RHS);
      if (RHS == 0)
        return 0;
    }

    // Merge LHS/RHS.
    LHS = new BinaryExprAST(BinOp, LHS, RHS);
  }
}

// 一是用”extern”声明外部函数，二是直接声明函数体
/// prototype
///   ::= id '(' id* ')'
static PrototypeAST *ParsePrototype()
{
  if (CurTok != tok_identifier)
    return ErrorP("Expected function name in prototype");

  std::string FnName = IdentifierStr;
  getNextToken();

  if (CurTok != '(')
    return ErrorP("Expected '(' in prototype");

  std::vector<std::string> ArgNames;
  while (getNextToken() == tok_identifier)
    ArgNames.push_back(IdentifierStr);
  if (CurTok != ')')
    return ErrorP("Expected ')' in prototype");

  // success.
  getNextToken(); // eat ')'.

  return new PrototypeAST(FnName, ArgNames);
}

/// definition ::= 'def' prototype expression
static FunctionAST *ParseDefinition()
{
  getNextToken(); // eat def.
  PrototypeAST *Proto = ParsePrototype();
  if (Proto == 0)
    return 0;

  if (ExprAST *E = ParseExpression())
    return new FunctionAST(Proto, E);
  return 0;
}



/// external ::= 'extern' prototype
static PrototypeAST *ParseExtern()
{
  getNextToken(); // eat extern.
  return ParsePrototype();
}

//===----------------------------------------------------------------------===//
// Top-Level parsing 将让用户输入任意的外层表达式（top-level expressions），在运行的同时会计算出表达式结果
//===----------------------------------------------------------------------===//
/// toplevelexpr ::= expression
static FunctionAST *ParseTopLevelExpr()
{
  if (ExprAST *E = ParseExpression())
  {
    // Make an anonymous proto.
    PrototypeAST *Proto = new PrototypeAST("", std::vector<std::string>());
    return new FunctionAST(Proto, E);
  }
  return 0;
}

static void HandleDefinition()
{
  if (ParseDefinition())
  {
    fprintf(stderr, "Parsed a function definition.\n");
  }
  else
  {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleExtern()
{
  if (ParseExtern())
  {
    fprintf(stderr, "Parsed an extern\n");
  }
  else
  {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleTopLevelExpression()
{
  // Evaluate a top-level expression into an anonymous function.
  if (ParseTopLevelExpr())
  {
    fprintf(stderr, "Parsed a top-level expr\n");
  }
  else
  {
    // Skip token for error recovery.
    getNextToken();
  }
}

/// top ::= definition | external | expression | ';'
static void MainLoop()
{
  while (1)
  {
    fprintf(stderr, "ready> ");
    switch (CurTok)
    {
    case tok_eof:
      return;
    case ';':
      getNextToken();
      break; // ignore top-level semicolons. 是用来辅助判断输入是否结束
    case tok_def:
      HandleDefinition();
      break;
    case tok_extern:
      HandleExtern();
      break;
    default:
      HandleTopLevelExpression();
      break;
    }
  }
}

//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//

int main()
{
  // Install standard binary operators.
  // 1 is lowest precedence.
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40; // highest.

  // Prime the first token.
  fprintf(stderr, "ready> ");
  getNextToken();

  // Run the main "interpreter loop" now.
  MainLoop();

  return 0;
}
//g++ -g -O3 toy.cpp

// $ ./a.out
// ready> def foo(x y) x+foo(y, 4.0);
// Parsed a function definition.
// ready> def foo(x y) x+y y;
// Parsed a function definition.
// Parsed a top-level expr
// ready> def foo(x y) x+y );
// Parsed a function definition.
// Error: unknown token when expecting an expression
// ready> extern sin(a);
// ready> Parsed an extern
// ready> ^D
// $
