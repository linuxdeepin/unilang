# 前言(Foreword)

　　本文档是 Unilang 语言解释器的设计规格说明。

　　解释器实现 [Unilang 基础语言](Language.zh-CN.md)。

　　本文档遵循的体例和参见上述链接的语言规范文档。

# 整体设计

　　解释器的核心是一个 *REPL(read-eval-print loop)* 。接受用户输入，求值并输出反馈。

　　解释器的核心功能以 `Interpreter` 类提供。主函数中初始化这个类的成员准备基础环境。

**原理** 作为原型实现，解释器比其它的复杂的指令翻译如 *JIT(just-in-time)* 或 *AOT(ahead-of-time)* 编译器的设计都相对简单。相对地，直接实现的性能可能较进一步优化的其它实现明显更低。以解释器原型为功能基线，未来可以演进为其它更复杂的形式，以改进性能并解决其它潜在的可用性问题。

## 实现环境

　　解释器使用 C++ 作为*宿主语言(host language)* 进行实现。Unilang 基础语言是解释器支持实现的语言。在互操作(interoperation) 的意义上，C++ 同时是*本机语言(native language)* 。

**原理** C++ 是较普遍使用的语言。C++ 的实现性能通常较好，实践丰富，适合实现解释器。使用 C++ 实现解释器，在和 C++ 互操作上具有优势，适合本项目的主要需求（实现 GUI 框架）。

　　当前依赖 C++11 。

　　[外部依赖](https://gitee.com/FrankHB/YSLib)引入了较新版本的 C++ 标准库特性的替代。

　　部分类型名使用 `using` 引入，之后不使用命名空间前缀。

　　由于 Unilang 基础语言的特性，对象语言的一部分功能可以由基础语言的程序代码被解释而实现，Unilang 同时是*元编程(metaprogramming)* 意义上的*元语言(meta language)* 和*对象语言(object language)*（这种元编程事实上是一般意义的*反射(reflection)* ）。原型实现中，标准库特性可能使用本机实现或非本机的对象语言*派生(derivation)* 。两者可在性能和实现难度上有不同的优势。相同功能的两种不同实现方式在功能上应等价，以便于相互替换，按需调整。具体功能在此不整体具体限定，而视具体功能的实现需求而定。

## 程序表示

　　程序或程序片段及其中表示的实体对应的数据结构是其对应的表示。

### 外部表示

　　程序以语言规范文档要求提供外部表示(external representation) 。外部表示支持源代码形式的输入，作为*翻译单元(translation unit)* ，经分析求值的输入。

### 内部表示

　　程序的外部表示经分析得到的其它表示都属于内部表示(internal representation) 。

　　本设计中的解释器中的求值部分对 *AST（abstract syntax tree ，抽象语法树）* 直接解释，即 AST 解释器。以 AST 作为 *IR（intermediate representation ，中间表示）* 是所谓的 *HIR（high-level IR，高层中间表示）* 的最直接的形式之一。其它 IR 相对于作为输入的 HIR 来讲是 *LIR（low-level IR，低层中间表示）*。当前设计不指定 LIR 。

**原理** AST 解释器相对引入其它 IR 的解释器（典型地，*字节码(bytecode)* 解释器）的优势是和语言规范中的语义描述直接对应，更适合作为原型。同时，AST 解释器在结构上的可扩展性一般更好，演进为实际可用的编译器比其它形式的解释器更低。现实的一个例子是，[Ruby 语言](https://en.wikipedia.org/wiki/Ruby_%28programming_language%29) 的官方实现 CRuby  1.x 的 [MRI](https://en.wikipedia.org/wiki/Ruby_MRI) 是 AST 解释器，2.x 的 [YARV](https://en.wikipedia.org/wiki/YARV) 是字节码解释器，3.x 的 3x3 实现 JIT 编译器时概念上重新切换到 AST 解释器而非直接基于 YARV 。以性能为目标，字节码的设计在不能被硬件直接利用时，很大程度是多余的。

## 规约

　　本设计中的求值被映射为一个 TRS（term reduction system，项求值系统）模型。作为一类主要的重写系统(rewrite system) ，这可直接用来表达程序的形式语义(foraml semantics) （更具体地，使用[操作语义(operational semantics)](https://en.wikipedia.org/wiki/Operational_semantics) 的方法，表达小步语义(small-step semantics) ）；同时，表达语言的实现时，它高度同构于 AST 解释器。

　　TRS 中的*规约(reduction)* 是输入到输出的有向的变换。一系列规约的步骤(step) 经过一定的组合可替换 IR 的某个*项(term)* 的节点，实现 TRS 重写(rewrting) 。这种重写以解释器中的原生本机代码实现，或调用被解释的对象语言（在此为 Unilang 基础语言）的程序片段实现功能。

### 抽象机

　　因为 C++ 的嵌套函数调用不可靠（任意深度的嵌套函数调用可能耗尽实现资源而引发 C++ 的未定义行为；一般实现中，用尽调用栈引发的错误无法被可移植地处理），在无法证明对象语言的嵌套函数调用深度有限时，使用异步的形式进行调用。这要求解释器保存规约之间的状态，而不能完全地通过 C++ 参数传递。这种状态在*上下文(context)* 数据结构保存。上下文中保存异步规约*动作(action)* ，通过类似顶层跳板(trampoline) 函数循环调用剩余的动作，实现程序逻辑的推进。

　　本设计基于 [CEK 抽象机](https://en.wikipedia.org/wiki/CEK_Machine)实现规约模型。CEK 抽象机的 C(Control) 即为输入和经过规约变换的 AST ；E(Environment) 为环境；K(Continuation) 等价保持异步规约动作。 E 部分也在上下文中保存，同时因为 Unilang 已支持一等环境，可直接通过对象语言程序访问其内容。

**注释** 未来扩展可支持一等续延(first-class continuation) ，允许对象语言编写的用户程序直接操作异步规约动作。非异步规约的实现难以实现这些扩展（需要全程序变换，不适合一般解释器）。

### 求值规约

　　一个对象语言中的表达式被表示为 AST 的一个项被规约，在所有规约步骤结束后，对应位置的项上剩余的重写结果就是这个表达式的求值结果；规约步骤中进行的副作用，即为表达式求值的副作用。

　　以此作为前提，求值算法被表达为已知表示表达式项上的规约。实现求值算法要求的表达式的求值变换的规约是求值规约。其它规约称为管理(administrative) 规约。

**原理** 管理规约可隐藏一些不在对象语言中出现的状态。这也给扩展 AST 解释器为编译实现而不需要引入其它分阶段(phased) IR 提供可能性。

## 诊断

　　语言规范要求的错误都实现为 C++ 异常。

　　为区分不同的来源，设计了不用的异常类。

　　REPL 捕获异常并输出错误消息，以指示不同的错误。

## 类型映射

　　除列表的 Unilang 对象在类型擦除后被统一保存在 `ValueObject` 中。

　　宿主值满足以下类型映射关系（部分没有被语言规范明确要求）：

* `TermNode` ：列表和非列表对象。
	* `TermNode` 中保存 `TermNode` 类型的容器。因为需可靠地支持不完整类型（在 `TermNode` 中保存递归类型），不能使用 ISO C++17 以前的标准库容器。
	* `TermNode` 保存 `ValueObject` 类型的**值数据成员**，作为非列表的对象的表示。
	* 列表使用容器中的子节点表示。非列表使用值数据成员表示。只使用两者之一的对象表示称为**正规表示**。
	* `TermNode` 还包含枚举 `TermTags` 类型的标签，作为指定对象可能作为临时对象、只读或其它状态的元数据。
	* `TermNode` 在表示基础语言中的对象以外，也用于其它中间表示。参见以下章节。
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

　　宿主类型可同时表示非宿主值。特别地，在表示 Unilang 中的列表值之前，`TermNode` 对象即可表示语法分析结果（见以下读取的描述）。这允许直接复用而非另行分配对象，有利于简化实现并提升性能。

## 解释器核心结构

　　本节概述作为 REPL 的解释器核心的顶层结构。

### 读取

　　当前输入的来源仅支持以行为单位的标准输入。

　　输入被作为字符串，经过词法分析器分解为词素序列，然后用简单的语法分析器匹配 `(` 和 `)` ，若不匹配则抛出异常。

　　之后，检查中缀变换，替换 `;` 和 `,` 为函数。

　　为避免宿主实现过深嵌套调用的未定义行为，语法分析使用单独的栈而不是 C++ 调用栈递归解析表达式，中缀变换也使用类似的单独的数据结构遍历表达式。

　　解析后输出 `TermNode` 类型的对象表示 AST 。

### 求值

　　求值部分的核心为 [CEKS 抽象机](https://legacy.cs.indiana.edu/ftp/techreports/TR202.pdf) ，把表达式求值作为*项重写系统(term rewriting system)*的*规约(reduction)*。

　　求值过程中的抽象机中的表示使用本文档的约定。表示保持相对的稳定，以支持和 C++ 互操作。这种互操作允许语言中的特性以 C++ 代码的形式实现，即*本机实现(native implementation)* 。而通过指定基础语言代码进行求值，也可以实现派生特性的非本机实现。

　　以 `TermNode` 对象表示的抽象语法树直接被作为被求值的表达式，按实现求值算法的规约步骤，重写为符合语言规格的仍以 `TermNode` 表示的输出。

　　为避免宿主实现过深嵌套调用的未定义行为，可能递归重入的操作使用异步调用实现。

### 打印

　　默认情况下 P 仅包含 E 的副作用蕴含的输出。

　　为便于调试，环境变量 `ECHO` 用于启用 REPL 回显，相当于以隐含的求值结果为参数，继续求值 `display` 函数。

## 库特性实现

　　对象以本机实现。

　　部分标准库操作使用本机实现。

　　其它操作利用 `Interpreter` 提供的接口，直接以 `Unilang` 源代码字符串作为输入进行求值，得到派生实现。

### 基础环境

　　基础环境以新建环境并逐一初始化其中的标准库绑定的形式实现。

　　初始化的标准库特性中，一部分是本机实现。

# 详细设计

　　本章介绍实现中的 API 的主要功能设计。

　　正式的 API 在主命名空间 `Unilang` 中。不公开的内部实现在其中的未命名(unnamed) 命名空间中。

## 外部实体引用

　　外部依赖包含命名空间 `std` 和 `ystdex` 中的实体，分别由标准库和 YSLib 提供。

　　以下声明被引入到主命名空间中：

```
using ystdex::byte;
using ystdex::size_t;

using ystdex::lref;

using ystdex::any;
using ystdex::bad_any_cast;
using ystdex::function;
using ystdex::make_shared;
using std::pair;
using std::shared_ptr;
using ystdex::string_view;
using std::weak_ptr;

namespace pmr = ystdex::pmr;
```

　　大多数声明和 ISO C++17 的同名声明一致，只有 `lref` 是作为 ISO C++2a 的 `std::reference_wrapper`（相比之前的标准版本，允许不完整类型）的简写。

　　对应于 ISO C++17 的 `std::pmr` 中的实体在需要被实现使用时，以类模板的形式被引入，如：

```
template<typename _tChar, typename _tTraits = std::char_traits<_tChar>,
	class _tAlloc = pmr::polymorphic_allocator<_tChar>>
using basic_string = ystdex::basic_string<_tChar, _tTraits, _tAlloc>;

using string = basic_string<char>;
```

　　此处 `string` 不同于标准库的 `std::string` 。因为历史原因，ISO C++ 的异常类必须使用后者，需要注意区分。

　　类模板 `sfmt` 是类似 `std::sprintf` 的，但返回 `string` 而不是指定参数的格式化字符串例程。

　　类型 `ValueObject` 是类似 ISO C++17 `std::any` 的对象，在此被作为中间表示的基本对象类型，可关联各种的运行时类型的值即目标。目标是 Unilang 中的值关联的 C++ 宿主值。默认初始化的 `ValueObject` 具有空值，不关联目标，目标类型为 `void` 。

## 词法处理

　　词法分析的规则是极简的：主要通过空白符拆分记号。记号直接同词素作为字符串处理，仅当必要时区分类别。

* 函数 `CheckLiteral` ：检查参数并消除提取可能包含的单引号或双引号。
* 函数 `DeliteralizeUnchecked` ：假定字符串参数存在引号，返回去除引号的字符串参数。
* 函数 `Deliteralize` ：返回去除引号的字符串参数，或当不存在引号时为 `char()` 。
* 函数 `IsGraphicalDelimiter` ：判断字符是图形分隔符：括号、逗号或分号。
* 函数 `IsDelimiter` ：判断字符不是图形字符（由 `std::isgraph` 定义）或是图形分隔符。
* 类 `ByteParser` ：分析字节流，提取符合词法要求的词素。
* 枚举 `LexemeCategory` ：词素类别：符号、代码（单引号字面量，当前未使用）、数据（双引号字面量）或扩展（派生实现中未归类字面量，当前未使用）。基础语言中其它字面量在此统一处理为符号。
* 函数 `CategorizeBasicLexeme` ：归类输入的词素，取词素类别。

## 异常

　　实现可使用以下异常类，具有以下继承关系：

* `UnilangException` ：异常基类，派生自 `std::runtime_error` 。
	* `TypeError` : 类型错误。
		* `ListTypeError` : 列表类型错误。
		* `ListReductionFailure` ：列表规约失败，用于合并子调用。
	* `InvalidSyntax` : 语法错误。
		* `ParameterMismatch` ：参数不匹配。
			* `ArityMismatch` : 元数不匹配。
		* `BadIdentifier` ：标识符错误：用于名称解析。

## 项节点

　　项节点(term node) 是语法处理接受的输入和输出类型，也被之后的求值使用，表示被规约项。

* `NoCainterTag` ：标签类型，用于初始化没有子节点的 `TermNode` 。
* `TermNode` ：项节点类型。
	* 包含递归的容器类型 `Container` ，使用兼容 `std::list` 的 `ystdex::list` 实现，以避免 ISO C++17 前 `std` 关联容器使用不完整类型作为模板参数的未定义行为（尽管 libstdc++ 可使用，但未明确支持），并提供较好的兼容性（libstdc++ 直至 GCC 5 没有实现部分分配器相关的接口）。
	* 包含上述容器类型的子节点（也是被归约项的子项）容器对象。
	* 包含 `ValueObject` 类型的 `Value` 对象：值数据成员，主要用于叶节点。
* `TNIter` 和 `TNCIter` ：`TermNode::Container` 的迭代器别名。
* 节点分类谓词：接受 `TermNode` 参数，判断是否符合特定的构造。节点中应不存在环，可表示*真列表(proper list)* 。
	* `IsBranch` 判断节点是枝节点(branch node) 或非叶节点，即具有子节点的节点。
	* `IsBranchedList` 判断节点是分支列表节点(branched list node) 同时是枝节点和列表节点。
	* `IsEmpty` 判断节点是空节点(empty node) 同时是叶节点和列表节点。
	* `IsExtendedList` 判断节点是扩展列表节点(extended list node) 是枝节点或列表节点。
	* `IsLeaf` 判断节点是叶节点(leaf node) ，即不具有子节点的节点，表示空列表或不具有子节点非列表。
	* `IsList` 判断节点是列表节点(list node) 是值数据成员为空的节点。
* 项节点访问函数模板：
	* `Access` 按指定目标类型访问节点的值数据成员。
	* `AccessFirstSubterm` 断言为枝节点并访问第一个子项。
	* `MoveFirstSubterm` 转移第一个子项。
	* `ShareMoveTerm` 转移到 `shared_ptr` 实例中。
	* `RemoveHead` 移除节点的第一个子节点。
* 函数模板 `AsTermNode` 构造没有子节点的以特定类型的值为值数据成员的节点。
* 函数模板 `HasValue` 判断节点的值数据成员是否等于指定的参数值。

## 语法处理

　　因为使用的语法是上下文无关的简单语法，直接使用手工实现的分析器，主要逻辑只包含匹配括号。

* 仿函数 `LexemeTokenizer` 是词素标记器：转换输入为以词素为作为值数据成员的节点。
* 函数模板 `ReduceSyntax` 遍历迭代器序列，递归匹配括号，把输出添加到参数指定的节点。若多余左括号则抛出异常。添加节点使用参数指定的标记器(tokenizer) （默认为 `LexemeTokenizer` ）。需要适应不确定嵌套层数的输入，所以使用单独的栈而不是直接使用 C++ 递归函数实现。

## 项和值的访问接口

　　使用 `TermNode` 作为项时具有一些公共的操作。

* 类型 `TokenValue` ：记号值，和 `string` 不同但可互相转换的类型，作为符号的宿主类型。
* 函数 `TermToNamePtr` ：访问作为记号值的项的值数据成员，失败时结果为空指针。
* 函数 `TermToString` ：转换项为字符串表示，主要用于输出错误。
* 函数 `TermToStringWithReferenceMark` ：同上，但包含区分引用值的标记。
* 函数 `ThrowListTypeErrorForInvalidType` ：抛出类型检查失败时的列表类型错误。
* 函数 `TryAccessLeaf` ：尝试访问特定类型的值数据成员，失败时结果为空指针。
* 函数 `TryAccessTerm` ：同上，但非叶节点视为失败。
* 类型 `AnchorPtr` 作为环境内部保存的引用计数数据指针。
* 类 `EnvironmentReference` 表示环境引用，是 Unilang 环境弱引用的宿主类型。
* 类 `TermReference` 表示项引用，是 Unilang 引用值的宿主类型。
	* `TermReference::get` 取引用的项。项引用保持被引用项来源的环境的引用计数，调试模式下默认可启用安全性检查。若锁定的环境引用为空，则检查失败。
* 函数 `ReferenceTerm` 当参数表示的项的值数据成员是引用值时结果是表示被引用对象的项，否则是参数。
* 用于兼容引用值和非引用值项的访问的实现内部使用的便利接口：
	* `ResolvedTermReferencePtr`
	* `ResolveToTermReferencePtr`
	* `TryAccessReferencedTerm`
	* `ResolveTerm`
* 用于检查项不是列表的内部使用的便利接口：
	* `CheckRegular`
	* `AccessRegular`
	* `ResolveRegular`
* 仿函数 `ReferenceTermOp` ：功能同函数 `ReferenceTerm` 。
* 函数 `ComposeReferencedTermOp` ：复合函数和 `ReferenceTermOp` ，用于在不支持引用值的操作上添加间接访问被引用对象的支持。

## 基本规约接口

　　规约是语义处理的核心，转换项为其它的项，以完成求值。这里提供规约接口依赖的类型。一部分规约例程和上下文无关，在这里一并提供。

* 枚举 `ReductionStatus` ：规约结果。用于规约后对项的处理，如正规化（参见以下描述）。
* 函数 `CheckReducible` ：判断规约结果是否可继续规约。
* 函数 `RegularizeTerm` ：正规化项：清理子项使其成为一个非列表。
* 项提升操作：以 `Lift` 为前缀的函数，用其它参数决定的项替换到第一个参数指定的项，前者通常是后者的* 规约函数：以 `Reduce` 为前缀且以 `TermNode&` 为第一个参数类型的函数，实现小的规约步骤，转换项并返回规约结果。

## 上下文接口

　　上下文在规约中起到关键作用。上下文保存动作作为将被执行的规约步骤。当上下文中动作为空时，规约终止。

* 类 `Environment` ：环境。包含表示变量绑定和父环境的数据成员。类型 `shared_ptr<Environment>` 是 Unilang 环境强引用的宿主类型。
* 类型 `ReducerFunctionType` ：表示动作的函数类型 `ReductionStatus(Context&)` 。
* 类型 `Reducer` ：规约器，兼容 `ReducerFunctionType` 的多态函数调用包装，是动作的一般表示。
* 类型 `ReducerSequence` ：`Reducer` 的序列，表示一系列动作。
* 类 `Conext` ：上下文，包含当前环境、当前动作、下一求值项的引用和规约中需要保存的其它的公共状态，也提供解释器使用规约的内部入口。替换当前环境的操作称为*切换(switch)* 。
	* 父环境通过子对象 `Parent` 引用。这个对象具有 `ValueObject` 类型，可支持不同类型的父环境的表示。父环境最终通过环境强引用访问。
	* 上下文实现环境查找的逻辑。查找父环境取环境强引用时，调试模式默认附加检查。若锁定的环境引用为空，则检查失败。
* 类型 `ContextHandler` ：上下文处理器，兼容签名为 `Reduction(TermNode&, Context&)` 的规约函数的多态函数调用包装。
* 函数模板 `AllocateEnvironment` ：分配环境。
* 函数模板 `SwitchToFreshEnvironment` ：创建新环境并切换当前环境。结果是被切换的先前上下文中的当前环境。
* 函数模板 `EmplaceLeaf` ：在环境的变量绑定中插入对象。
* 函数 `ResolveEnvironment` ：以可能作为环境的宿主类型访问参数指定的值数据成员或项，失败时抛出类型错误异常。
* 类 `EnvironmentSwitcher` ：切换环境的便利接口，作为守卫(guard) 以支持异常安全的对当前环境的切换操作。
* 类型 `EnvironmentGuard` ：包装 `EnvironmentSwitcher` 的守卫。 

## 其它支持接口

* 枚举 `ValueToken` ：单元类型(unit type) 的实现，表示 `#inert` 的宿主值。
* 类 `Continuation` ：续延(continuation) ，保存 `ContextHandler` 对象的异步规约动作，调用时从上下文的下一求值项恢复被规约项作为参数。
* 函数 `ThrowInsufficientTermsError` ：抛出子项不足的异常。
* 类 `SeparatorTransformer` ：转换中缀分隔符。用于把逗号和分号替换为前缀形式的内部表示。
* 类 `FormContextHandler` ：形式上下文处理器。用于构成合并子的宿主类型，即以 `FormContextHandler` 作为目标类型的 `ContextHandler` 。
	* 在 `FormContextHandler` 中存储一个非负整数包装值(wrapping value) 表示应用子求值到底层操作子前需要对实际参数求值的次数。对大多数应用子，这个值等于 1 ；对操作子，这个值等于 0 。
* 注册函数用的便利接口：基于上下文处理器，向指定的对象（上下文或环境）添加函数，用于 Unilang 标准库的实现。
	* `WrappingKind`
	* `RegisterHandler`
	* `RegisterForm`
	* `RegisterStrict`
	* `UnaryExpansion`
	* `RegisterUnary`
* 函数 `BindParameter` ：实现参数绑定。
	* 支持 Unilang 函数调用和 `$def!` 中 <formal> 的递归模式匹配。
	* 绑定匹配后以操作数中的实际参数初始化作为形式参数的对象。
	* 初始化时，根据操作数中的 `TermTags` 等状态进行复制消除(copy elimination) ，避免创建多余的对象副本。

## 求值算法的实现

　　以 `ReduceOnce` 为前缀的函数实现了主规约算法，这是对应 Unilang 求值算法的全局规约实现。具体步骤的其它实现包括 `ReduceCombined` 等。

## 标准库操作的本机实现

　　因为是基本的特性或者，由本机实现比非本机实现更直观容易，一些标准库操作的底层操作逻辑以 C++ API 的方式提供，包括以下规约函数：

* `If`
* `Cons`
* `Eval`
* `MakeEnvironment`
* `GetCurrentEnvironment`
* `Define`
* `VauWithEnvironment`
* `Wrap`
* `Unwrap`
* `Sequence`

## 标准库实现

　　初始化解释器时初始化上下文。其中，初始的当前环境是基础环境。基础环境中初始化绑定，就是标准库提供的绑定：

* 对标准库对象的初始化，直接修改 `Context` 的绑定。
* 对本机实现的应用子，使用 `RegisterStrict` 。
* 对本机实现的操作子，使用 `RegisterForm` 。
* 对非本机实现的库函数，使用解释器的 `Perform` 成员函数接受基础语言代码，其中包括 `$def!` 等可在当前环境添加绑定。

　　之后，切换当前环境到内部，完成对基础环境的隐藏。

