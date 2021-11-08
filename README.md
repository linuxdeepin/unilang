# 概述

　　本版本库维护开发中的代号为 Unilang 的新语言。

　　解释器的构建和使用参见以下各节。

# 构建

　　本项目支持不同方法构建。

　　支持的宿主环境为 MSYS2 MinGW32 和 Linux 。

　　以下使用版本库根目录作为当前工作目录。

## 构建环境依赖

　　一些外部依赖项的源代码在版本库及 git 子模块中提供。

　　构建环境依赖以下环境工具：

* `git`
* `bash`
* GNU coreutils
* 支持 ISO C++ 11 的 G++ 和与之兼容的 GNU binutils

　　构建使用外部二进制依赖和相关工具：

* libffi
* LLVM 7
	* `llvm-config`

　　安装构建环境依赖的包管理器命令行举例：

```
# Some dependencies may have been preinstalled.
# MSYS2
pacman -S --needed bash coreutils git mingw-w64-x86_64-gcc mingw-w64-x86_64-binutils mingw-w64-x86_64-libffi mingw-w64-x86_64-llvm
# Arch Linux
sudo pacman -S --needed bash coreutils git gcc binutils libffi
yay -S llvm70 # Or some other AUR frontend command.
# Debian (strech/buster)/Ubuntu (bionic-updates/focal)
sudo apt install bash coreutils git g++ libffi-dev llvm-7-dev
# UOS
sudo apt install bash coreutils git g++ libffi-dev llvm-dev
```

## 构建环境更新

　　构建之前，在版本库根目录运行以下命令确保外部依赖项：

```
git submodule update --init
```

　　若实际发生更新，且之前执行过 `install-sbuild.sh` 脚本，需清理补丁标记文件以确保再次执行这个脚本时能继续正确地处理源代码：

```
rm -f 3rdparty/.patched
```

　　使用以下 `git` 命令也能清理文件：

```
git clean -f -X 3rdparty
```

## 使用直接构建脚本

　　运行脚本 `build.sh` 直接构建，在当前工作目录输出可执行文件：

```
./build.sh
```

　　使用 shell 命令行调用 G++ 编译器驱动，不支持并行构建，可能较慢。优点是不需要进一步配置环境即可使用。适合一次性测试和部署。

## 使用外部工具构建脚本

　　利用[外部工具的脚本](https://frankhb.github.io/YSLib-book/Tools/Scripts.zh-CN.html)，可支持更多的构建配置。这个方式相比直接构建脚本更适合开发。

　　当前 Linux 平台只支持 x86_64 宿主架构。

　　以下设并行构建任务数 `$(nproc)` 。可在命令中单独指定其它正整数的值代替。

### 配置环境

　　配置环境完成工具和依赖项（包括动态库）的安装，仅需一次。（但更新子模块后一般建议重新配置。）

　　安装的文件由 `3rdparty/YSLib` 中的源代码构建。

　　对 Linux 平台构建目标，首先需确保构建过程使用的外部依赖被安装：

```
# Arch Linux
sudo pacman -S freetype2 --needed
```

```
# Debian/Ubuntu/UOS
sudo apt install libfreetype6-dev
```

　　运行脚本 `./install-sbuild.sh` 安装外部工具和库。脚本更新预编译的二进制依赖之后，构建和部署工具和库。其中，二进制依赖直接被部署到源码树中。当前二进制依赖只支持 `x86_64-linux-gnu` 。本项目构建输出的文件分发时不需要依赖其中的二进制文件。

**注释** 脚本安装的二进制依赖可能会随构建环境更新改变，但当前本项目保证不依赖其中可能存在的二进制不兼容的部分。因此，二进制依赖的更新是可选的。但是，在构建环境更新后，一般仍需再次运行脚本配置环境，以确保覆盖安装外部工具和（非二进制依赖形式分发的）库的最新版本。其中，若二进制依赖文件不再在脚本预期的部署位置中存在，脚本会重新获取最新版本的二进制依赖。

　　以下环境变量控制脚本的行为：

* `SHBuild_BuildOpt` ：构建选项。默认值为 `-xj,$(nproc)` ，其中 `$(nproc)` 是并行构建任务数。可调整 `$(nproc)` 为其它正整数。
* `SHBuild_SysRoot` ：安装根目录。默认指定值指定目录 `"3rdparty/YSLib/sysroot"` 。
* `SHBuild_BuildDir` ：中间文件安装的目录。默认值指定目录 `"3rdparty/YSLib/build"` 。
* `SHBuild_Rebuild_S1` ：非空值指定重新构建 [stage 1 SHBuild](https://frankhb.github.io/YSLib-book/Tools/SHBuild.zh-CN.html#%E5%A4%9A%E9%98%B6%E6%AE%B5%E6%9E%84%E5%BB%BA)（较慢）。
	* **注意** 构建环境更新 `3rdparty/YSLib/Tools/Scripts` 的文件后，需指定此环境变量为非空值，以避免可能和更新后的文件不兼容的问题。
	* 其它情形不必要，建议忽略，以提升构建性能。

　　使用安装的二进制工具和动态库需配置路径，如下：

```
# Configure PATH.
export PATH=$(realpath "$SHBuild_SysRoot/usr/bin"):$PATH
# Configure LD_LIBRARY_PATH (reqiured for Linux with non-default search path).
export LD_LIBRARY_PATH=$(realpath "$SHBuild_SysRoot/usr/lib"):$LD_LIBRARY_PATH
```

　　以上 `export` 命令的逻辑可放到 shell 启动脚本（如 `.bash_profile` ）中而不需重复配置。

### 构建命令

　　配置环境后，运行脚本 `sbuild.sh` 构建。

　　和直接构建脚本相比，支持并行构建，且支持不同的配置，如：

```
./sbuild.sh release-static -xj,$(nproc)
```

　　则默认在 `build/.release-static` 目录下输出构建的文件。为避免和中间目录冲突，输出的可执行文件后缀名统一为 `.exe` 。

　　此处 `release-static` 是**配置名称**。

　　设非空的配置名称为 `$CONF` 。当 `$SHBuild_BuildDir` 非空时输出文件目录是 `SHBuild_BuildDir/.$CONF` ；否则，输出文件目录是 `build/.$CONF` 。

　　当 `$CONF` 前缀为 `debug` 时，使用调试版本的库，否则使用非调试版本的库。当 `$CONF` 后缀为 `static` 时，使用静态库，否则使用动态库。使用动态库的可执行文件依赖先前设置的 `LD_LIBRARY_PATH` 路径下的动态库文件。

　　运行直接构建脚本使用静态链接，相当于此处使用非 debug 静态库构建。

# 运行

## 运行环境

　　使用上述动态库配置构建的解释器可执行文件在运行时依赖对应的动态库文件。此时，需确保对应的库文件能被系统搜索到（以下运行环境配置已在前述的开发环境配置中包含），如：

```
# MinGW32
export PATH=$(realpath "$SHBuild_SysRoot/usr/bin"):$PATH
```

```
# Linux
export LD_LIBRARY_PATH=$(realpath "$SHBuild_SysRoot/usr/lib"):$LD_LIBRARY_PATH
```

　　若使用系统包管理器以外的方式安装 LLVM 运行时库到非默认位置，类似添加 LLVM 的路径，如：

```
# Linux
export LD_LIBRARY_PATH=/opt/llvm70/lib:$LD_LIBRARY_PATH
```

　　以上 Linux 配置的 `LD_LIBRARY_PATH` 也可通过 [`ldconfig`](https://man7.org/linux/man-pages/man8/ldconfig.8.html) 等其它方式代替。

　　使用静态链接构建的版本不需要这样的运行环境配置；不过 LLVM 通常使用动态库。

**注意** 非脚本配置的外部二进制依赖项可能不兼容，需要通过系统包管理器等方式部署，依赖这些库导致解释器最终的二进制文件不保证跨系统环境（如不同 Linux 发行版）之间可移植。

## 运行解释器

　　运行解释器可执行文件直接进入 REPL ；或使用命令行选项 `-e` ，支持直接求值字符串参数。

　　可选环境变量：

* `ECHO`：启用 REPL 回显。
* `UNILANG_NO_JIT`：停用代码执行时 JIT 编译，使用纯解释器。

　　除使用选项 `-e` ，配合 `echo` 命令，也可支持非交互式输入，如：

```
echo 'display "Hello world."; () newline' | ./unilang
```

## 运行测试脚本

　　文件 `test.sh` 是测试脚本。可以直接运行测试用例，其中调用解释器。

　　测试用例直接在脚本中指定，包括调用解释器运行测试程序 `test.txt`。在 REPL 中 `load "test.txt"` 也可以调用测试程序。

　　脚本以下支持环境变量：

* `UNILANG` ：指定解释器可执行文件路径，默认为 `./unilang` 。
* `PTC` ：非空时，运行 PTC 测试用例。手动终止进程后结束用例。在此期间，正确的 PTC 实现可确保最终内存占用不随时间增长。

　　使用 `sbuild.sh` 构建的可执行文件不在当前目录。可使用类似以下的 `bash` 命令调用：

```
UNILANG=build/.debug/unilang.exe ./test.sh
```

# 支持的语言特性

　　参照[《Unilang 介绍》](https://gitlabwh.uniontech.com/ut001269/working-docs/-/blob/master/Unilang%20%E4%BB%8B%E7%BB%8D.md)中的例子（尚未完全支持）。

　　详细的语言规范和设计文档待补充。

　　当前支持的特性清单：

* 基本语法和求值算法：
	* 环境和变量解析。
	* 函数应用。
* 函数：详见[语言规范文档](doc/Language.zh-CN.md)。

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
		* 修复解释器中可能由用户程序中构造的深度过大的嵌套列表引起的未定义行为，包括列表对象被销毁时和形式参数树被检查时。
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

