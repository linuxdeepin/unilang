# 版本历史

* **V0.0**
	* 依赖 ISO C++17 。
	* 实现解释器 REPL 框架。
* **V0.1**
	* 支持特性：
		* 基本语法和求值算法：
			* 环境和变量解析。
			* 函数应用。
		* 函数：
			* `$if`
			* `$sequence`
			* `display`
			* `newline`
	* 添加外部依赖项 [YSLib](https://gitee.com/FrankHB/YSLib) 。
	* 降低构建环境要求，依赖 ISO C++14 。
* **V0.2**
	* 新增支持特性：
		* 基本语法和求值算法：
			* 环境引用。
		* 各种不同的异常类。
		* 变量绑定。
		* 函数：
			* `$def!`
			* `list`
			* `null?`
			* `cons`
			* `eval`
			* `get-current-environment`
			* `make-environment`
			* `$vau/e`
			* `$vau`
			* `wrap`
			* `$lambda`
			* `unwrap`
			* `$set!`
			* `$defv!`
			* `$defl!`
		* 对象 `ignore`
	* 输出格式调整。
	* 使用外部依赖简化部分实现。
* **V0.3**
	* 新增支持特性：
		* 函数：
			* `raise-invalid-syntax-error`
			* `apply`
			* `list*`
			* `$defw!`
			* `first`
			* `rest`
			* `accr`
			* `foldr1`
			* `map1`
			* `$let`
	* 添加文档：
		* [语言规范文档](doc/Language.zh-CN.md)。
		* [解释器实现文档](doc/Interpreter.zh-CN.md)。
	* 降低构建环境要求，依赖 ISO C++11 。
* **V0.4**
	* 新增支持特性：
		* 函数：
			* `list-concat`
			* `append`
			* `$let/d`
			* `lock-environment`
			* `lock-current-environment`
			* `make-standard-environment`
			* `$let*`
			* `$letrec`
			* `$bindings/p->environment`
			* `$bindings->environment`
			* `$provide!`
			* `$provide/d!`
			* `$import!`
			* `$cond`
			* `$when`
			* `$unless`
	* 修复求值中非布尔值不被作为 `#t` 处理。
	* 解释器文档添加详细设计。
* **V0.5**
	* 修改内部对象模型。
	* 新增支持特性：
		* 函数：
			* `eq?`
			* `eqv?`
			* `$and?`
			* `$or?`
			* `not?`
			* `make-encapsulation-type`
			* `load`
* **V0.6**
	* 新增支持特性：
		* **实验性** FFI 支持。
		* 支持绑定的变量（包括函数参数）以引用传递。
		* 默认在调试构建启用安全检查：
			* 父环境引用。
			* 对象悬空引用。
		* 函数：
			* 辅助库函数：
				* `random.choice`
				* `sys.exit`
			* `$lambda/e`
			* `bound-lvalue?`
			* `$resolve-identifier`
			* 保留引用值的不安全操作：
				* `list%`
				* `$vau/e%`
				* `eval%`
				* `$vau%`
				* `$lambda%`
				* `$defv%!`
			* `move!`
			* `forward!`
			* 比较 `int` 操作数的关系操作：
				* `<?`
				* `<=?`
				* `>=?`
				* `>?`
			* `filter`
	* 优化实现：
		* 移除部分标准库函数内部实现中参数传递的不必要的复制。
		* 函数 `make-environment` 转移实际参数中的本机环境引用。
	* 修复缺陷：
		* 缺少列表参数检查。
		* 错误的常规项（列表或非列表值的内部表示）检查。
	* 更新外部依赖项，增强构建脚本支持。
* **V0.7**
	* 添加关于模块的规则和约定。
	* 新增支持特性：
		* 新增标准库函数：
			* `derive-current-environment`
			* `$as-environment`
			* `$provide/d!`
			* `$provide/let!`
			* `$defl/e!`
			* 模块 `std.strings` 及其中的函数：
				* `++`
				* `string-empty?`
				* `string<-`
				* `string=?`
				* `string-contains?`
				* `string-contains-ci?`
				* `string->symbol`
				* `symbol->string`
				* `string->regex`
				* `regex->match?`
			* 整数数值操作：
				* `+`
				* `add2`
				* `-`
				* `*`
				* `multiply2`
				* `/`
				* `div`
				* `mod`
			* `cons%`
			* `id`
			* `as-const`
			* `expire`
			* `$move-resolved!`
			* `$lvalue-identifier?`
			* `forward-first%`
			* `first%`
			* `rest%`
			* `$while`
			* `$until`
			* `uncollapsed?`
			* `$defv/e%!`
		* 构造合并子等标准库函数支持环境列表作为父环境。
		* 引用值支持增强：
			* 函数 `cons` 复制左值操作数。
			* 以下函数支持保留引用值和转发参数：
				* `apply`
				* `$cond`
				* `$when`
				* `$unless`
				* `first`
				* `accr`
				* `foldr1`
				* `map1`
				* `list-concat`
		* 形式参数树支持引用标记字符 `@` 绑定列表左值子对象的引用值。
		* 支持通过 `eval` 求值和合并子调用的上下文的 PTC(proper tail call) 保证。
	* 修复实现问题：
		* 修复以下函数中的非预期对象复制：
			* `foldr1`
			* `map1`
			* `$let`
			* `$let*`
			* `$letrec`
		* 修复解释器中可能由用户程序中构造的深度过大的嵌套列表引起的未定义行为，包括列表对象被销毁时和形式参数树被检查时（自从 V0.1 前）。
	* 确保符号求值为左值。
	* 优化实现：
		* 支持合并子右值调用转移而不是复制内部资源。
		* 省略合并子调用时对形式参数的冗余检查。
* **V0.8**
	* 调整和优化解释器实现：
		* 启用内存池。
		* 命令行添加 `-e` 选项支持。
		* 增强异常处理。
		* 新增可选的 LLVM 代码生成。
	* 修复实现问题（除非另行指定，自从 V0.7 ）：
		* 修复用户定义的函数右值调用转移资源未生效的问题。
		* 修复标准库函数：
			* 修复 `list%` 中的非预期对象复制：
			* 修复 `$provide/let!` 和 `$provide!` 最后的符号以 `.` 起始时错误地被忽略。
			* 修复 `wrap` 和 `unwrap` 操作对操作数的表示的检查。
		* 修复 Unilang 解释器数值减法操作（自从 V0.6 ）。
		* 修复返回值转换的实现中缺失对象转移支持。
	* 新增支持特性：
		* 新增标准库函数：
			* `desigil`
			* `symbols->imports`
			* 标准库模块 `std.strings` 中：
				* `string-split`
				* `regex-replace`
			* `raise-error`
			* `$remove-eval`
			* `$remove-eval%`
			* 标准库模块 `std.system` 中：
				* `env-get`
			* 标准库模块 `std.modules`
			* `check-list-reference`
			* `first&`
			* `rest&`
			* `$lambda/e%`
			* `$defl/e%!`
			* `$let%`
			* `$let*%`
			* `assign@!`
			* `idv`
			* `collapse`
			* `assign%!`
			* `set-first%!`
			* `check-environment`
			* 标准库模块 `std.promises`
			* `wrap%`
		* 调整标准库函数：
			* `$provide/let!` 、`$provide!` 和 `$import!` 支持指定引用标记字符。
			* 函数 `load` 移至标准库模块 `std.io` 。
			* 涉及形式参数树的检查中支持递归检查和符号类型错误。
			* 函数 `wrap` 和 `unwrap` 避免冗余复制，并添加合并子的子对象引用支持。
	* 分离启动脚本 `init.txt` 。
		* 保持以下标准库核心函数在初始环境可用：
			* `display`
			* `newline`
			* `load`
			* `display`
			* `puts`
			* `++`
		* 新增库函数 `putss` 和测试框架。
	* 添加测试脚本 `test.txt`，添加测试用例。
	* 修复语言规范中一些等价谓词的比较规则不够充分及相关的解释器实现中关于合并子的子对象引用的相等操作。
* **V0.9**
	* 修复实现问题（除非另行指定，自从 V0.8 ）：
		* 修复库函数：
			* `make-encapsulation-type` 构造的封装类型相等性（以 `eqv?` 比较）。
			* `set-first%!` 调用失败
			* `collapse` 的调用结果没有保留右值引用
			* `first` 和 `first&` 的调用结果没有保留消亡值或左值列表的临时对象元素的右值引用
			* 标准库模块 `std.promises` 中：
				* `force` 对嵌套的 promise 对象调用失败及对右值不正确共享状态转移
	* 新增支持特性：
		* 新增标准库函数：
			* `itos`
			* `stoi`
			* `assign!`
			* 标准库模块 `std.io` 中：
				* `write`
			* `weaken-environment`
			* `$wvau`
			* `$wvau%`
			* `$wvau/e`
			* `$wvau/e%`
			* `first@`
			* 补充数值操作函数：
				* `number?`
				* `real?`
				* `rational?`
				* `integer?`
				* `exact-integer?`
				* `exact?`
				* `inexact?`
				* `finite?`
				* `infinite?`
				* `nan?`
				* `zero?`
				* `=?`
				* `zero?`
				* `positive?`
				* `negative?`
				* `odd?`
				* `even?`
				* `max`
				* `min`
				* `add1`
				* `sub1`
				* `/`
				* `abs`
		* 启动脚本默认库函数：
			* `stoi-exact`
			* `rmatch?`
			* 依赖控制 API
				* `version?`
				* `string->version`
				* `version->string`
				* `version<?`
				* `dependency?`
				* `make-dependency`
				* `name-of`
				* `version-of`
				* `check-of`
				* `validate`
				* `strings->dependency-contract`
				* `dependency-set?`
				* `make-dependency-set`
				* `has-dependency?`
			* 测试 API ：
				* `moved?`
				* `unit`
				* `$expect-moved`
		* 新增支持基本转义字符序列。
		* 支持使用 `'` 分隔的字面量构成符号。
		* 新增非符整数以外的数值字面量及对应的数值类型。
	* 调整支持特性：
		* 调整标准库函数：
			* 标准库模块 `std.io` 中：
				* `display` 不输出字符串字面量的引号（保持原行为使用 `write` ）。
			* 修改算术操作为数值操作，支持不同的数值类型：
				* 算术操作（包括算术计算和比较，下同）统一支持两个操作数，不再支持多个操作数。
				* 算术操作支持不同的数值类型的内部表示。
				* 移除函数：
					* `add2`
					* `multiply2`
		* `#ignore` 是具有单独的类型的字面量，不再是符号。
	* 测试脚本 `test.txt` 添加测试用例。
	* 调整和优化解释器实现：
		* 简化部分标准库实现。
		* 增强命令行选项支持。
			* 选项 `-e` 支持重复多次。
			* 新增脚本模式，支持从命令行指定文件名或标准输入 `-` 。
			* 新增显示命令行帮助的选项。
* **V0.10**
	* 新增上层语言特性：
		* 语法：扩展中缀变换，包括中缀的赋值和算术操作。
		* 语法： `()` 以外的括号的解析和匹配。
		* 启动脚本实现上层语言函数：
			* `if`
			* `while`
			* `!=`
			* 标准库函数的若干其它别名
		* 测试库 API 作为上层语言特性。
	* 添加新的示例程序，包括使用基础语言和上层语言版本。
* **V0.11**
	* 修复实现问题：
		* 转移值和访问列表元素的一些操作可能错误地得到临时值（自从 V0.6 ）。
		* 修复标准库函数实现对临时对象的处理（自从 V0.7 ）：
			* `first`
			* `first&`
		* 修复 ignore 的值及其引起的通过 $lambda 等方式构造的函数递归调用不满足 PTC（自从 V0.9 ）。
	* 新增支持特性：
		* 启动脚本默认库函数：
			* 新增类型库函数：
				* `type?`
				* `type->string`
				* `has-type?`
				* `typed-ptree->ptree`
			* 标准库函数别名转为类型标注操作函数：
				* `lambda`
				* `def`
				* `defn`
				* `let`
			* 新增类型标注操作函数：
				* `let*`
				* `letrec`
		* 新增标准库函数：
			* `reference?`
			* `unique?`
			* `list?`
			* 标准库模块 `std.strings` 中：
				* `string?`
			* `forward`
			* `and`
			* `or`
	* 调整支持特性：
		* 调整标准库函数：
			* `$and?` 重命名为 `$and` 。
			* `$or?` 重命名为 `$or` 。
			* `list-extract-first` 和 `list-extract-rest%` 支持转发参数。`
		* 移除启动脚本默认库函数别名 `and` 和 `or`（这些函数在标准库中提供，不再短路求值）。
	* 测试脚本 `test.txt` 添加关于类型库等新增特性的测试用例。
	* 整理特性文档的示例，在 `demo` 中提供。
	* 构建脚本 `build.sh` 支持使用定制的编译器命令和选项。
* **V0.12**
	* 修复实现问题：
		* 修复标准库函数实现：
			* 修复保留引用值值类别（自从 V0.7 ）：
				* `first&`
				* `collapse`
			* 避免部分本机实现的函数右值调用在结果中遗留临时对象标签（自从 V0.7 ）：
				* `$def!`
				* `eval`
				* `eval%`
				* `$vau`
				* `$vau%`
				* `$vau/e`
				* `$vau/e%`
			* 修复 `$resolve-identifier` 结果可能遗留临时对象标签（自从 V0.6 ）。
			* 修复 `$move-resolved!` 结果可能遗留临时对象标签（自从 V0.8 ）。
		* 修复递归复制 Unilang 对象可能导致解释器未定义行为的问题（自从 V0.1 前）。
		* 修复内部表示为嵌套单元素列表的非列表值可能被错误地作为列表求值（自从 V0.8 ）。
		* 修复 PTC 实现资源释放顺序（自从 V0.7 ）。
	* 新增支持特性：
		* 新增标准库函数：
			* 新增标准库模块 `std.continuations`
			* `$quote`
			* 新增标准库模块 `std.classes`
			* 标准库模块 `std.io` 中：
				* `read-line`
		* 新增库函数：
			* 异常库
			* `$check`
		* 新增和源文件名以及源代码内容关联的源代码信息用于诊断。
		* 新增环境变量 `UNILANG_NO_SRCINFO` 指定停用用于诊断的源代码信息（不含源文件名）。
		* 启动脚本默认库函数：
			* 新增类型库函数：
			* 标准库函数别名转为类型标注操作函数：
			* 新增类型标注操作函数：
		* 新增 Qt 绑定支持。
			* 新增 Qt 示例。
	* 调整支持特性：
		* 调整标准库函数：
			* `$and` 和 `$or` 支持转发引用参数。
	* 其它实现增强：
		* 分隔符变换使用分配器。
		* 优化 `#ignore` 处理。
		* 改进构建脚本对 Clang++ 的支持，移除不支持的编译选项。
		* `init.txt` ：
			* 移除冗余的 `let` 定义。
		* 加入断言检查不应在一等对象的表示中出现的临时对象标签。
		* 改进项节点内部 API 。
		* 简化标签值。
		* 优化 PTC 的内部数据结构和尾调用实现。
	* 修复（上层语言）排序示例（自从 V0.11 ）。

