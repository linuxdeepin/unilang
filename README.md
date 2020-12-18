# 概述

　　本版本库维护开发中的代号为 Unilang 的新语言。

　　解释器的构建和使用参见以下各节。

# 构建

　　本项目支持不同方法构建。

　　支持的宿主环境为 MSYS2 MinGW32 和 Linux 。

　　以下使用版本库根目录作为当前工作目录。

## 构建环境依赖

　　除 `git` 外，构建环境依赖和支持 ISO C++ 11 的 G++、`bash` 和 GNU coreutils 。源代码在版本库及 git 子模块中提供，除此之外需要 libffi 的包。

　　构建 Qt demo 需要依赖 Qt5 和 `pkg-config` 。

　　安装构建环境依赖的包管理器命令行举例：

```
# MSYS
pacman -Syu mingw-w64-x86_64-gcc mingw-w64-x86_64-libffi mingw-w64-x86_64-pkgconf mingw-w64-x86_64-qt5 --needed
# Arch Linux
sudo pacman -Syu gcc libffi pkgconf qt5-base --needed
# Debian/Ubuntu/UOS
sudo apt install g++ libffi-dev pkg-config qtbase5-dev # Some may have been preinstalled.
```

## 构建环境更新

　　构建之前，更新 git 版本库子模块确保外部依赖项：

```
git submodule update --init
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

　　运行脚本 `./prepare-libs.sh` 安装其它的[二进制依赖](https://frankhb.github.io/YSLib-book/Archives.zh-CN.html)。

**注意** 因为 [GCC 9.2 修复的一个缺陷](https://stackoverflow.com/a/58376059/2307646)，使用之前版本的 GCC 还需要在构建前修改源码变通以避免构建失败：

```
sed -i 's/p_impl{}/p_impl/' 3rdparty/YSLib/YFramework/include/YCLib/HostedGUI.h
```

**注意** 默认 release 配置启用 LTO(link-time optimization) ，因为一些工具链缺陷可能不稳定。若之后的构建出现和 `lto1` 或 `lto-wrapper` 等 LTO 相关的内部编译器错误(internal compiler error) ，删除构建目录中的子目录 `.shbuild` 和 `.shbuild-dll` ，并使用以下变通全局禁用 LTO ：

```
sed -i 's/-flto=jobserver//g' 3rdparty/YSLib/Tools/Scripts/SHBuild-YSLib-common.txt
sed -i 's/-flto//g' 3rdparty/YSLib/Tools/Scripts/SHBuild-YSLib-common.txt
# Use debug library to work around the LTO information in the release library.
cp 3rdparty/YSLib/YFramework/Linux/lib/libFreeImaged.a 3rdparty/YSLib/YFramework/Linux/lib/libFreeImage.a
```

　　之后，开始构建和部署工具，并配置开发环境。

　　设安装目标根路径 `$INSTALL_ROOT` ，过程如下：
　　
```
# Configure PATH.
export PATH=$(realpath "$INSTALL_ROOT/usr/bin"):$PATH
# Configure LD_LIBRARY_PATH (reqiured for Linux with non-default search path).
export LD_LIBRARY_PATH=$(realpath "$INSTALL_ROOT/usr/lib"):$LD_LIBRARY_PATH
# Build and deploy.
SHBuild_UseDebug=true SHBuild_UseRelease=true SHBuild_NoDev=true SHBuild_SysRoot="$INSTALL_ROOT" 3rdparty/YSLib/Tools/install-sysroot.sh -xj,$(nproc)
```

　　脚本使用变量 `SHBuild_SysRoot` 指定安装位置。若变量未设置或它的值为空，安装位置为 `3rdparty/YSLib/sysroot` 。

　　以上 `export` 命令的逻辑可放到 shell 启动脚本（如 `.bashrc` ）中而不需重复配置。

**说明** 调用脚本 `install-sysroot.sh` 时，非空的环境变量 `SHBuild_NoDev` 指定不需要在生成构建工具（ stage 2 `SHBuild` ）之后构建其它辅助工具（不需要在本项目中使用）。构建这些工具的过程调用的构建工具自身依赖待部署的动态库，所以通常需要在调用脚本**之前**确保设置了正确的 `LD_LIBRARY_PATH` ，以免最终可因为找不到动态库而失败。

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

## 运行环境配置

　　使用上述动态库配置构建的解释器可执行文件在运行时依赖对应的动态库文件。此时，需确保对应的库文件能被系统搜索到（以下运行环境配置已在前述的开发环境配置中包含）：

```
# MinGW32
export PATH=$(realpath "$INSTALL_ROOT/usr/bin"):$PATH
```

```
# Linux
export LD_LIBRARY_PATH=$(realpath "$INSTALL_ROOT/usr/lib"):$LD_LIBRARY_PATH
```

　　使用静态链接构建的版本不需要这样的运行环境配置。

## 运行

　　经过可能需要的配置后，直接运行即可：

```
./unilang
```

　　进入解释器 REPL 。

　　可选环境变量：

* `ECHO`：启用 REPL 回显。

　　配合 `echo` 命令，可支持非交互式输入，如：

```
echo 'display "Hello world."; () newline' | ./unilang
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

