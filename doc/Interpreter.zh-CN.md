# 前言(Foreword)

　　本文档是 Unilang 语言解释器的设计规格说明。

　　解释器实现 [Unilang 基础语言](Language.zh-CN.md)。

　　本文档遵循的体例和参见上述链接的文档。

# 整体设计

　　解释器的核心是一个 *REPL(read-eval-print loop)* 。接受用户输入，求值并输出反馈。

　　解释器的核心功能以 `Interpreter` 类提供。主函数中初始化这个类的成员准备基础环境。

## 实现环境

　　解释器以 C++ 实现。当前依赖 C++14 ，可能会变更。

　　[外部依赖](https://gitee.com/FrankHB/YSLib)引入了较新版本的 C++ 标准库特性的替代。

　　部分类型名使用 `using` 引入，之后不使用命名空间前缀。

## 诊断

　　语言规范要求的错误都实现为 C++ 异常。

　　为区分不同的来源，设计了不用的异常类。

　　REPL 捕获异常并输出错误消息，以指示不同的错误。

## 宿主值(host value)

　　宿主值是实现中和基础语言对象对应的 C++ 值。

　　宿主值具有不同的类型，除列表外，类型擦除后被统一保存在 `ValueObject` 中。

　　宿主值满足以下类型映射关系（部分没有被语言规范明确要求）：

* `TermNode` ：列表。
* `ValueToken` ：`#inert` 的宿主类型。
* `TokenValue` ：符号。
* `shared_ptr<Environment>` ：环境强引用。`
* `EnvironmentReference` ：环境弱引用。`
* `TermReference` ：引用值。
* `ContextHandler` ：本机函数类型。
	* 目标为 `FormContextHandler` 类型的 `ContextHandler` ：合并子。
* `string` ：字符串。
* `bool` ：布尔类型。
* `int` ：整数。

## 读取

　　当前输入的来源仅支持以行为单位的标准输入。

　　输入被作为字符串，经过词法分析器分解为词素序列，然后用简单的语法分析器匹配 `(` 和 `)` ，若不匹配则抛出异常。

　　之后，检查中缀变换，替换 `;` 和 `,` 为函数。

　　为避免宿主实现过深嵌套调用的未定义行为，语法分析使用单独的栈而不是 C++ 调用栈递归解析表达式，中缀变换也使用类似的单独的数据结构遍历表达式。

　　解析后输出 `TermNode` 类型的对象表示*抽象语法树(AST, abstract syntax tree)* 。

## 抽象机

　　求值部分的核心为 [CEKS 抽象机](https://legacy.cs.indiana.edu/ftp/techreports/TR202.pdf) ，把表达式求值作为*项重写系统(term rewriting system)*的*规约(reduction)*。

　　以 `TermNode` 对象表示的抽象语法树直接被作为被求值的表达式，按实现求值算法的规约步骤，重写为符合语言规格的仍以 `TermNode` 表示的输出。

　　为避免宿主实现过深嵌套调用的未定义行为，可能递归重入的操作使用异步调用实现。

## 打印

　　默认情况下 P 仅包含 E 的副作用蕴含的输出。

　　为便于调试，环境变量 `ECHO` 用于启用 REPL 回显，相当于以隐含的求值结果为参数，继续求值 `display` 函数。

## 标准库

　　部分标准库函数以 C++ 代码的形式提供*本机(native implementation)* 实现。

　　其它操作利用 `Interpreter` 提供的接口，直接以 `Unilang` 源代码字符串作为输入进行求值，得到*派生(derived)* 实现。

* `$vau`
* `$lambda`
* `list`
* `$set!`
* `$defv!`
* `$defl!`

