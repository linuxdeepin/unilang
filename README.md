# 概述

　　本版本库维护开发中的代号为 Unilang 的新语言。

## 使用

　　解释器的构建和使用参见以下各节。

### 构建

　　构建环境依赖支持 ISO C++ 14 的 G++ 。

　　更新 git 版本库子模块确保外部依赖项：

```
git submodule update --init
```

　　然后运行脚本构建获得可执行文件：

```
./build.sh
```

### 运行

　　当前使用的外部依赖被静态链接到构建得到的可执行文件，因此不需要另外部署二进制依赖，直接运行即可：

```
./unilang
```

　　进入解释器 REPL 。

　　可选环境变量：

* `ECHO`：启用 REPL 回显。

## 支持的语言特性

　　参照[《Unilang 介绍》](https://gitlabwh.uniontech.com/ut001269/working-docs/-/blob/master/Unilang%20%E4%BB%8B%E7%BB%8D.md)中的例子（尚未完全支持）。

　　详细的语言规范和设计文档待补充。

　　当前支持的特性清单：

* 基本语法和求值算法：
	* 环境和变量解析。
	* 函数应用。
* 函数：
	* `$if`
	* `$sequence`
	* `display`
	* `newline`

## 版本历史

* **V0.0**
	* 依赖 ISO C++17 。
	* 实现解释器 REPL 框架。
* **V0.1**
	* 支持特性：
		* 基本语法和求值算法。
			* 环境和变量解析。
			* 函数应用。
		* 函数：
			* `$if`
			* `$sequence`
			* `display`
			* `newline`
	* 添加外部依赖项 [YSLib](https://gitee.com/FrankHB/YSLib) 。
	* 降低构建环境要求，依赖 ISO C++14 。

