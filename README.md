# Unilang

© 2020-2022 UnionTech Software Technology Co., Ltd.

This is the repository of the programming language named **Unilang**, including documents and an interpreter as the reference implementation.

See the sections below to build and use the interpreter.

# About the new language

Unilang is a general purpose programming language project proposed to adapt to more effective and flexible development of desktop environment applications.

## The origin

Currently, there are many options for desktop application development, with their respective advantages and disadvantages:

* The C/C++ native application development solution as [Qt](https://www.qt.io/) is the mainstream solution for many Linux desktop applications.
	* C/C++ have mature language standards and implementations, as well as rich development resources, including multiple language implementations with good vendor neutrality. But at the same time, it is difficult to learn, the project development cycle is often long, and the cost is high. Most of these overall problems are difficult to improve in the short term.
		* C/C++ are the representatives of the most portable industrial languages. However, many widely adopted features are not standardized by the language, and also depend on many details of the underlying system, such as hot loading.
		* There are rich resources of C/C++ libraries, but the problems such as package management, continuous integration and compatibility issues among binary distributions cannot be effectively solved for a long time, and the fragmentation is serious, which is not conducive to rapid deployment.
		* As static languages, C/C++ are problematic in low development efficiency in some scenarios. As statically typed languages, the type systems are not that powerful and they contribute little on the development experience.
		* C/C++ can explicitly manage resources and allow the development of large-scale high-performance GUI applications. However, they are relatively easy to misuse, and it often requires more experienced developers to correctly implement the programs.
	* Qt has good portability and can adapt to many mainstream desktop platforms.
		* However, due to technical problems such as language limitations, Qt also needs to rely on special language extensions (rather than standard C++), and its portability and extensibility at the language level are relatively poor.
		* Compared with other C/C++ programs, the depolyment of Qt programs require more space. Nevertheless, this has relatively minor impacts if Qt is deployed as the system libraries.
* The development solution based on the non-native and dynamic language runtime as [Electron](https://www.electronjs.org/) is another mainstream solution.
	* Using of popular dynamic languages can overcome the problem that some static languages are not flexible, but it is sometimes difficult to ensure the quality.
	* Relying on [GC](https://en.wikipedia.org/wiki/Garbage_collection_%28computer_science%29) does not require explicit memory management, which reduces some development difficulties. However, most developers find it difficult to effectively optimize the runtime mechanism, and it is easy to cause problems such as memory leaking, which greatly affects the quality and development experience of GUI applications.
	* Large runtimes often need to be deployed.
	* Some runtime implementations may have performance issues about cold start.
* The hybrid solutions of native and dynamic languages as [PySide](https://wiki.qt.io/PySide) can solve some problems of the two options above.
	* However, this kind of solutions can not automatically solve the problems brought by native language and dynamic languages themselves.
	* At the same time, it requires developers to understand the basic scheme and does not guarantee that it is easier to use. Otherwise, once used improperly, it is possible to integrate the defects rather than advantages of the two.
* Mobile apps solutions as by [Flutter](https://flutter.dev/) are also migrating to the desktop.
	* The versions ported to the desktop platforms are relatively immature.
	* There are usually problems similar to those in the solutions of other dynamic language runtimes due to the dynamic language being used.

From the perspective of higher-level structure, different types of GUI solutions also have different technical limitations in the sense of architecture, which greatly limits the choices of real general solutions:

* Compared with the traditional *retained mode* GUI, the *immediate mode* GUI as [Dear ImGUI](https://github.com/ocornut/imgui) lacks the abstraction of entities, not reflecting the traditonal approach of [WIMP metaphor](https://en.wikipedia.org/wiki/WIMP_%28computing%29).
	* The so-called [immediate mode](https://en.wikipedia.org/wiki/Immediate_mode_%28computer_graphics%29) was originally focused on graphics rendering and lacked attention to UI interaction. Therefore, even if good display output is achieved, a lot of work is still needed to improve interactivity.
	* Due to the simplification of intermediate states, immediate mode GUI is basically difficult to extend to implement the UI automation interface in nature.
* It is difficult to overcome the specific functional limitations of the underlying implementation by relying on the "native" solution provided with the system.
	* For example, the [Win32 window style `WS_EX_LAYERED` is only supported in the top-level windows but not in the child windows before Windows 8](https://docs.microsoft.com/windows/win32/winmsg/extended-window-styles).
	* Considering the coupling with the system implementation, this actually means that solutions relying on the "native" experience provided by the system (such as [wxWidgets](https://www.wxwidgets.org/)) cannot reliably provide a consistent and portable user experience, even in different versions of the same system.
	* Such inconvenience is even acknowledged within the system manufacturers.
		* The GUI frameworks encapsulating Win32 as [MFC](https://docs.microsoft.com/cpp/mfc/mfc-desktop-applications) and [WinForms](https://docs.microsoft.com/dotnet/desktop/winforms) are increasingly obsolete and are generally replaced by the so-called direct UI that reimplements the rendering logic (not depending on Win32 `HWND`), such as [WPF](https://docs.microsoft.com/dotnet/desktop/wpf).
		* Contrasted with other solutions, [WinUI](https://microsoft.github.io/microsoft-ui-xaml/) directly dropped the support for (less) old versions of the operating system in an early stage (since WinUI 2). The dependence on the old version of the system a reason for that decision.
* The GUI based on Web graphical clients (browsers) has good portability and flexibility, but there are some other special problems:
	* The flexibility of Web-based implementations are mainly achieved by the limited combination of client languages, specifically [JavaScript](https://www.ecma-international.org/publications-and-standards/standards/ecma-262/), [CSS](https://www.w3.org/TR/CSS/#css) and [HTML](https://html.spec.whatwg.org/multipage/). Defects in these languages will long persist.
		* [WebAssembly](https://webassembly.org/) will be a supplement. It cannot replace JavaScript in the foreseeable future, let alone other DSLs other than JavaScript.
		* These technologies do not ensure good support for the development of native applications.
			* Historically, HTML was designed to present static documents (called "pages") in the web rather than interactive dynamic programs.
			* As the role of the client-side patching for converting static pages into dynamic content, JavaScript and CSS were also severely limited in function in the early stage (therefore, [Flash](https://en.wikipedia.org/wiki/Adobe_Flash) and other technologies were once popular).
			* Even though standardized technical specifications such as [DOM](https://dom.spec.whatwg.org/) can simplify a large number of implementation details, incompatibility among different browsers is often problematic. Fortunately, as a dynamic language, JavaScript is easy to reduce problems by [shim](https://en.wikipedia.org/wiki/Shim_%28computing%29) (instead of changing the running environment), but this is at least at the cost of adaptation workload, and it is not always feasible (for example, the lack of support for [PTC (proper tail call)]((https://262.ecma-international.org/6.0/#sec-tail-position-calls)) of ES6 can hardly be solved except modifying the underlying implementation of the language.
	* The actual implementations are extremely complex, obviously more difficult to customize than other solutions.
		* The core parts of the browser (implementing the typesetting engine such as HTML and CSS and the runtime of languages such as JavaScript) are highly encapsulated integrated components, which are mostly implemented in C/C++, but more difficult to modify than almost all other C/C++ programs.
		* Therefore, Web-based GUI solutions can only bundle these native components without significant changes. Even functions not depended on by the application are not easily removed before deployment. Unless distributed by the system, this will seriously bloat the the programs.
	* For some traditional factors (like security concerns), the interaction between the Web programs and the native environment is limited, and the development of desktop applications may require a lot of additional work.
* Hybrid frameworks that rely on other components have path dependency problems correspondingly.
	* If relying on the framework of the native GUI implementation provided by the system, there are problems of the above native solution.
	* Relying on the frameworks having dependencies on the Web (such as Electron, [Cordova](https://cordova.apache.org/), [Tauri](https://tauri.app/)) will bring the problems of the above Web-based solutions.
	* If all are relied on, all similar limitations will eventually be jointly inherited.
	* Nevertheless, if only using these targets as one of the optional output configurations (for instance, using WebAssembly as the generation target), there will be less problems as above.
* Native but not OS-native GUI frameworks as Qt have few global architecture defects comparable to the above problems, but there are still many problems in the implementation architecture and API design, and the development experience is not satisfactory.

So, no existing solution can take into account all kinds of problems and become the preferred solution for desktop development without doubt.

A considerable part of the above problems (performance, deployment difficulty and portability) are directly related to the language. Looking at the language part, we find that existing languages are not able to solve all these major issues well enough, because:

* Most popular standardized languages, such as C, C++ and ECMAScript, have a heavy historical burden and do not have the ability to extend the language itself to meet these needs.
* Dart and other languages specially designed for these solutions have problems in the basic design decisions (like relying on global GC), which make it unable to fully fit some important scenarios.
* Other general purpose languages, such as [Rust](https://www.rust-lang.org/) and [Go](https://golang.org), do not provide GUI solutions together.
	* Although there are some third-party GUI projects, their advantages in design still cannot well meet the needs of desktop application development.
	* The language community has not focused on developing desktop applications.

We are eager to have a new language to solve all the above pain points. However, it is not enough to provide a new language design and implementation. New languages are not magic to automatically solve legacy problems -- especially considering that there is no lack of "new" programming languages in the market, but they still do not meet the needs.

One of the technical reasons for this situation is that many designs are too focused on specific requirements and lack of consideration of the general factors of long-term evolution of the language, and their applicability outside the expected target areas drops sharply, which is not universal enough; or they fail to balance universality and complexity. This makes the application field slightly deviate from the expectation and exposes the limitations of the original design. Even if users knows how to improve a language, they will encounter practical difficulties in development of the language and eventually give up.

If new options are put forward without avoiding this situation, it will only further hinder the solution of the problem. Therefore, on the basis of meeting the needs, we hope that the new language can truly achieve universality in a deeper way - by reducing the built-in *ad-hoc* features specifically for individual problem domains and replacing them with a more general set of primitive features.

> Programming languages should be designed not by piling feature on top of feature, but by removing the weaknesses and restrictions that make additional features appear necessary.

<p align="right">—— <a href="https://schemers.org/Documents/Standards/">R<sup>n</sup>RS</a> & <a href="https://ftp.cs.wpi.edu/pub/techreports/pdf/05-07.pdf">R<sup>-1</sup>RK</a></p>

## Characteristics

Unilang is the language part of the new solution to comprehensively solve the existing problems. The distinguishing features are:

* As a dynamic language, it provides more extensibility at the language level than other languages.
	* Features provided by the language core rules in most other languages are expected to be library modules implemented in Unilang, e.g. statically type checking can be provided by user programs.
		* Customization of the functionality of the language by user-provided components may effectively rule out unexpected dynamic features, and **eventually get advanced development experience as in most static languages without the defects of inconvenience from the core rules of static languages**.
		* It allows the existing language features to be complemented by adding libraries in the environment where the Unilang program has been deployed, without the need to redeploy the implementation of the toolchain.
		* A basic language is provided, and the practical feature set is built by extending this language in the form of libraries. Libraries are expected to be provided by this project and users.
	* Similar to C and C++ but different from [Java](https://docs.oracle.com/javase/specs/jls/se18/html/jls-1.html), it does not explicitly require or assume specific forms of translation and execution. The implementation details such as compilation, interpretation and what image format to load are transparent to the core language rules.
	* There is no preset *phases of translation* as explicitly specified in C and [C++](http://eel.is/c++draft/lex.phases). There is no need of macros expanded in separated phase -- they can be replaced by functions that support first-class environments.
	* It supports [homoiconicity](https://en.wikipedia.org/wiki/Homoiconicity) and allows code as data programming.
	* Functions are [first-class objects](https://en.wikipedia.org/wiki/First-class_citizen).
	* *Environments* have ownership of variable bindings. *First-class environments* are supported.
* It supports C++-like object model and (currently unchecked) unsafe ownership semantics.
	* Unlike C# or rust, it does not provide an ad-hoc `unsafe` keyword to mark "unsafe" code fragments. The most primitive features are "unsafe" by default.
	* Safety is not uniquely defined by language, and users are allowed to implement customized safety of different types and degrees by ways like extending the type system.
* Global GC is not required, and a subset of the language allows the same level of "insecurity" as C++, but ensures deterministic resource allocation.
	* There is no native static check for unsafe operations, but the extensibility of the language allows direct implementation of the type system or automatic proof of stronger memory safety. It may be provided as a library in the future.
	* The language rules still allow the interoperations introducing GC. In particular, multiple non-global GC instances are allowed.
* The language supports [PTC](https://www.researchgate.net/profile/William_Clinger/publication/2728133_Proper_Tail_Recursion_and_Space_Efficiency/links/02e7e53624927461c8000000/Proper-Tail-Recursion-and-Space-Efficiency.pdf) in the formal sense. This makes users have no need to work around about undefined behaviors like stack overflow in the programs.
	* Mainstream languages do not provide such guarantees without aid of GC.
* Implicit [*latent typing*](https://en.wikipedia.org/wiki/Latent_typing) is used instead of explicit [*manifest typing*](https://en.wikipedia.org/wiki/Manifest_typing).
	* This naturally avoids the conflict between the user-provided extended type system and the built-in rules while maintaining scalability.
		* Even without extension, as an implementation detail, the language already allows [*type inference*](https://en.wikipedia.org/wiki/Type_inference) to eliminate some type checks without affecting the semantics of the programs.	
		* User programs are allowed to extend the type system with the syntax and related checks of *type annotations*.
	* Expressions are similar to those in C++ with a few different rules of [*(value category)*](http://eel.is/c++draft/basic.lval). However, unlike C++, it is not the property of statically determined expressions, but the dynamic metadata following the objects implied by the expressions.
	* Similar to the `const` type qualifier in C++, objects referenced by lvalues are allowed to be marked as immutable (read-only), instead of the default convention of *immutable* values as in languages like Rust.
	* Similar to the *expired value (xvalue)* in C++, the object referenced by the lvalue may be tagged unique, allowing the resources in it to be transferred.	
** *Rationale** In the representative decisions above, a common method is to compare the technical feasibility between different directions and adopt the option that is easy to extend to other direction. Otherwise, even if it is feasible, there will be a lot of ineffective work that should have been avoided.
	* Designing a static language, and then adding some rules to disguise it as a dynamic language with sufficient dynamic characteristics, is far more difficult extending rules of a dynamic lanugage to get the feature set a static language would have.
		* Therefore, the basic language is designed as a dynamic language at first.
	* Adding proofs to restore some guarantees (without conflicts to others) where they have been already abandoned, is more difficult to just adding the proofs to make fresh guarantees in the plain contexts where such guarantees are never existed before.
		* For instance, in languages using ad-hoc syntax notations like `unsafe`, ususally the safety guarantees defined by the language rules are dropped altogether, and a part of these guarantees cannot be retained. Even if this problem is ignored, these languages lack mechanisms to allow users to provide stricter guarantees integrating into current ones.
			* Therefore, the basic language is unsafe by default.
		* As another instance, although the default immutable data structures can guarantee the "correctness" like [const correctness](https://isocpp.org/wiki/faq/const-correctness) (an instance of [*type safety*](https://en.wikipedia.org/wiki/Type_safety) that keeps the defined immutable property from being discarded), it ignores the problem that the definition of "immutable" is not sufficiently preciesly described and it cannot be extended by the user program. In many cases, immutability only needs to be an equivalent relationship, not an unmodifiable one.
			* This may cause abuse of specific non-modifiablitity, like the case of requirements on key types of associative containers in the C++ standard library. It actually need no `const` as currently mandated by ISO C++, because the immutablity of the key is defined by the equivalent relationship derived from the comparator. But the type system of C++ cannot distinguish these two case of immutablilty, leading to over-specification in the types of API, which blocks some operations on the key.
				* Using unsafe casts like `const_cast` to cease the type safety guarantee introduced by `const` totally and assuming it not destroyed by other operations is a helpless workaround here (the "more difficult" situation; the type safety cannot be restored, and the effect is worse).
			* Meanwhile, type system having immutability by default, like that in Rust, more fundamentally block ways of extensibility in the sense of type formation.
				* This design implies there is only one kind of immutability, unless then modifying the design of the type system by dropping the original definition of "immutability" and reintroduce qualification like C++'s `const` (the "more difficult" situation).
			* This also limits the extents of *constant propagation* optimization in the existing implementation, because in principle, the "constant" here only cares about whether the substitution of the generated code can maintain the semantic preservation property before and after the transformation, while caring nothing about the equality on concrete values.
				* If the language allows the user to express that "some values with different representations are considered equivalent", the adaptability of the optimization will naturally grow.
			* Therefore, objects in the basic languages are mutable by default.
	* Excluding GC from a language that requires already the global GC, is far more difficult to adding the GC to a language with no mandatory of GC in its rules (especially when the GC is to be customized by users).
		* Therefore, the language first excludes the dependencies on the global GC in its design.
	* It is basically impossible to add extensions to a language implementation without PTC guarantee, unless the logic including the core evaluation rules is reimplemented (for example, by adding a fresh execution engine in the implementation).
		* Therefore, the language first requires PTC to make sure the availablity, instead of encouraging of unreliable indirect implementations to complement the guarantee in future.
		* Notice most fetures other than PTC can still be relatively easily implemented correctly by indirect implementations (e.g. ECMAScript dialects transpiled by [Babel](https://babeljs.io/)). Threfore, most other features are not (and need not) treated specially as PTC in the core language rules.
* The implementation has good interoperability with C++.
	* Currently, the interpreter (runtime) is implemented in C++.
	* With the object model in the language, Unilang objects can be mapped to C++ objects.
	* The language binding mostly focus on the availablity of C/C++ API for well-known ABIs.

To keep universality, Unilang does not provide GUI functionailty as built-in features, but provides related APIs through libraries. In the current plan, Unilang will support Qt-based binding libraries to ease the transition of some existing desktop application projects. The language design of Unilang keeps sufficient abstract ability and extensibility, allowing direct implementation of GUI frameworks in the future.

# About the documents

　　The following documents are maintained in this project:

* `README`: This document. It introduces the overall status, usage and main supported features.
	* It is suitable for all readers interested in this project.
* [Release notes (zh-CN)](ReleaseNotes.zh-CN.md): Release notes of different versions.
	* It is suitable for all readers interested in this project.
* [Language specification (zh-CN)](doc/Language.zh-CN.md)：This is the normative language specification, including the conforming requirements of the implementation, the supported features in the language and some informative descritpions.
	* It is mainly used as a reference to the contributors of this project, as well as developers of the language and its implementations.
	* It is the main source in determining whether the current design of the language and an implementation (both the interpreter and the library code) of the language having defects.
* [Implementation document of the interpreter (zh-CN)](doc/Interpreter.zh-CN.md): This document describe the design of the intpreter, which is not intended or qualified as the publicly available features in the language. The document also contains some descriptions about the project-specific plan of the evolution and decisions, as well as the related rationale.
	* It is sutable for the maintainers of this project (the implementors of the interpreter) and users who want to extend the language implementation.
* [Introduction of the language](doc/Introduction.zh-CN.md)：This document introduces the use of the language and its features.
	* It is hopefully useful for beginners.
	* All users of this project (language, interpreter and library) are recommended to read it.
* [Fetures document](doc/Features.zh-CN.md)：This is a reference to language features as supplement to the introcuction document above. There is a (still non-exhaustive) feature list and related information of how to use them.
	* It is suitable for the end-users of the language (the developers using Unilang).
	* Users having interested in the 
	* Users who need to have an in-depth understanding of the language and need to propose new language features are recommended read it first.

The contributors of this project shall generally be able to determine the relevance of the contents in the above documents and the corresponding implemented modifications (if any).

If there is inconsistency between the contents of the document or it does not match other parts of the project, please contact the maintainers to report the defect.

# Building

This project supports several ways to build.

The supported hosted environments are MSYS2 MinGW32 and Linux.

The following instructions using the root directory of the repository as the current working directory.

## Build dependencies

Some external dependencies are used in the source form, provided as the git submodules.

The building environment relies on the following tools:

* `git`
* `bash`
* GNU coreutils
* G++ supporting ISO C++, and compatible GNU binutils

The following dependencies are optional:

* Clang++ can be a replacement of G++.

The binary form of the following dependencies are also used:

* libffi
* LLVM 7
	* `llvm-config`
* Qt 5
* `pkg-config`

The following commands illustrates how to prepare the build environment by package managers:

```
# Some dependencies may have been preinstalled.
# MSYS2
pacman -S --needed bash coreutils git mingw-w64-x86_64-gcc mingw-w64-x86_64-binutils mingw-w64-x86_64-libffi mingw-w64-x86_64-llvm mingw-w64-x86_64-pkgconf mingw-w64-x86_64-qt5
# Arch Linux
sudo pacman -S --needed bash coreutils git gcc binutils libffi pkgconf qt5-base
yay -S llvm70 # Or some other AUR frontend command.
# Debian (strech/buster)/Ubuntu (bionic-updates/focal)
sudo apt install bash coreutils git g++ libffi-dev llvm-7-dev pkg-config qtbase5-dev
# Deepin
sudo apt install bash coreutils git g++ libffi-dev llvm-dev pkg-config qtbase5-dev
```

### Qt environment requirements and assumptions

* [Itanium C++ ABI](https://itanium-cxx-abi.github.io/cxx-abi/abi.html) is used.
* `QT_NAMESPACE` is not supported.
* Only headers and library files in the filesystem are used. There is no dependencies of other Qt files.
* The compiler option `-I$PREFIX/include/QtWidgets` is used, where `$PREFIX` is the filesystem prefix of the header inclusion directory.
* The linker option `-lQt5Widgets -lQt5Core` is used.

## Updating the build environment

Before the build, run the following command to ensure the external source dependencies are setup properly:

```
git submodule update --init
```

If there exists the submodule update, and the script `install-sbuild.sh` is already executed previously, it is necessary to cleanup the intermediate file to make sure the script work properly again, by the command:

```
rm -f 3rdparty/.patched
```

Alternatively, the follwing `git` command can make the cleanup:

```
git clean -f -X 3rdparty
```

## Using the direct building script

Run the script `build.sh` to build directly and the output executable file will be put into the current working directory:

```
./build.sh
```

This uses `g++` by default. The environment variable `CXX` can override the default, as:

```
env CXX=clang++ ./build.sh
```

The default compiler options are `-std=c++11 -Wall -Wextra -g`. Similarly, use the environment variable `CXXFLAGS` can override the default value.

The script uses shell command lines to call the compiler driver specified by `$CXX` directly, and no parallel builds are supported. It may be slow.

The advantage of this script is the ease to use without further configurations. It may be suitable for one-time testing and deployment.

## Using the script of external build tools

With the [script of external build tools (zh-CN)](https://frankhb.github.io/YSLib-book/Tools/Scripts.zh-CN.html), more configurations are supported. This method is more suitable for the development in this project.

Currently, only x86_64 architecture is supported on Linux.

The following instructions assume the number of maximum parallel build tasks as `$(nproc)`. This can be overriden by a positive integer in the command lines.

### Environment configuration

The configuration of the environment installs the tools and build dependencies, which requires to be run only once. (But it is recommended to configure again after any git submodules update.)

The installed files are built from the source code in `3rdparty/YSLib`.

For Linux targets, first it is required to keep the external dependencies specific for the installation, even they are not used in this project at all:

```
# Arch Linux
sudo pacman -S freetype2 --needed
```

```
# Debian/Ubuntu/Deepin
sudo apt install libfreetype6-dev
```

Then run the script `./install-sbuild.sh` to install the external tools and libraries. The script updates precompiled binary dependencies, then builds and deploys the tools and the libraries. The binaray dependencies are deployed directly into the source tree. Currently the prebuilt dependencies only supports the `x86_64-linux-gnu` target. Any output files built by this project do not need to deploy these binary dependencies.

**NOTE** The binary dependencies installed by the script may change as per the build environment updates. Nevertheless, currently it is guaranteed no binary-incompatible parts are depended on. Therefore, it is optional to update the binary dependencies. However, after the update of the build environment, usually the script required to run again, to ensure up-to-date tools and libraries are installed. If the binary dependencies are no longer existing, the script will automatically fetch them.

The following environment variables controls the behavior of the script:

* `SHBuild_BuildOpt`: The build options, defaulted to  `-xj,$(nproc)`, where `$(nproc)` is the number of parallel builds.
* `SHBuild_SysRoot`: The root directory for the installation, defaulted to `"3rdparty/YSLib/sysroot"`.
* `SHBuild_BuildDir`: The directory for intermediate files during the installation, defaulted to `"3rdparty/YSLib/build"`.
* `SHBuild_Rebuild_S1`: If not empty, specify the rebuild of [stage 1 SHBuild (zh-CN)](https://frankhb.github.io/YSLib-book/Tools/SHBuild.zh-CN.html#%E5%A4%9A%E9%98%B6%E6%AE%B5%E6%9E%84%E5%BB%BA) (it might be slow).
	* **CAUTION** Update of `3rdparty/YSLib/Tools/Scripts` requires the rebuild to prevent imcompatibility.
	* This is not necessary in other cases. It is recommened to not rebuild to improve performance during the installation.

Using of the installed binary tools and dynamic libraries requires the configurations of paths, as:

```
# Configure PATH.
export PATH=$(realpath "$SHBuild_SysRoot/usr/bin"):$PATH
# Configure LD_LIBRARY_PATH (reqiured for Linux with non-default search path).
export LD_LIBRARY_PATH=$(realpath "$SHBuild_SysRoot/usr/lib"):$LD_LIBRARY_PATH
```

The `export` commands can be put into the initialization scripts of the shell (such as `.bash_profile`) so there are no need to repeat.

### Building commands

After the configuration of the build environment, run the script `sbuild.sh` to build this project.

This method support parallel builds and different configurations compared to the direct building script, as:

```
./sbuild.sh release-static -xj,$(nproc)
```

The command above specify the output built files in the directory `build/.release-static`. To avoid the confilicts with intermediate files, the output executable files always having the suffix `.exe`.

Here, `release-static` is the **configuration name**.

Let non-empty configuration name `$CONF`, when `$SHBuild_BuildDir` is not empty, the output directory is `SHBuild_BuildDir/.$CONF`; otherwise, the output directory is `build/.$CONF` .

When `$CONF` has the prefix by `debug`, the debug versions of the libraries (already built from `3rdparty` source in the previously installation steps for the build environment configuration) are used automatically, otherwise libraries of non-debug version are used. When `$CONF` has the suffix `static`, static libraries are used instead of dynamic libraries. The use of dynamic libraries makes the output executable file relying on the files in `$LD_LIBRARY_PATH` set up previously.

Running the direct building script links against static libraries. This is roughly equivalent to the non-debug static library builds here.

# Running

## Running environment

The interpreter executable file using of the dynamic library configuration relies on the correspoinding dynamic library files at runtime. It is necessary to ensure these libraries files can be found by the system (which should be prepared by the steps of the above build environment configuration), as:

```
# MinGW32
export PATH=$(realpath "$SHBuild_SysRoot/usr/bin"):$PATH
```

```
# Linux
export LD_LIBRARY_PATH=$(realpath "$SHBuild_SysRoot/usr/lib"):$LD_LIBRARY_PATH
```

If LLVM is installed to non-default location by means other than the system package manager, it may need also to configure for LLVM, as:

```
# Linux
export LD_LIBRARY_PATH=/opt/llvm70/lib:$LD_LIBRARY_PATH
```

The `LD_LIBRARY_PATH` in Linux configurations above can also be configured by other ways instead, such as [`ldconfig`](https://man7.org/linux/man-pages/man8/ldconfig.8.html).

Use of static library instead can avoid the necessity of the configurations above. However, LLVM may be deployed only with dynamic libraries.

**CAUTION** There is no guarantee to ensure the compatibility among external binary dependencies not configurated by the scripts. They may need other reliable ways of deployment, e.g. by the system package manager. Relying on such libraries cuases the final executable of the interpreter not portable among different system enviornments (like different Linux distrobutions).

## Running the interpreter

Running the executable file of the interpreter enters the REPL, and the interpreter run in the interactive mode. Alternatively, specify a script name in the command line, then the interpreter will be run in the scripting mode, and the script will be loaded and executed. The script name `-` is treated as the standard input.

Running the interpreter with the command line option `-e` accepts string arguments to specify the code being evaluated before entering the interactive or scripting mode. The option `-e` can occur multiple times with one string argument for each instance. These strings are treated as Unilang source code and to be evaluated in order.

The command line of interpreter also supports the POSIX convention of `--`, which indicates all command line arguments after are not interpreted as command line options. This allows to specify script names same to options.

The commond line option `-h` or `--help` shows the help message of the interpreter.

Optionally, the environment variables are handled by the interpreter:

* `ECHO`: If not empty, enable REPL echo. This makes sure the interpreter prints the evaluated result after each interaction session.
* `UNILANG_NO_JIT`: Disable JIT compilation, using pure interpreter instead.
* `UNILANG_NO_SRCINFO`: Disable source information for diagnostic message output. The source names are still used in the diagnostics.
* `UNILANG_PATH`: Specify the library load path. See the descriptions of standard library `load` in the [language specifciation (zh-CN)], as well as the descriptions of standard library operations in the [implementation document of the interpreter (zh-CN)](doc/Interpreter.zh-CN.md).

Except the option `-e`, with the external `echo` command, the interpreter can support non-interactive input, such as:

```
echo 'display "Hello world."; () newline' | ./unilang
```

### Qt Demo

```
./unilang demo/qt.txt
```

See `demo/qt.py` for the Python implementation with the equivalent functionality.

### Quicksort demo

```
./unilang demo/quicksort.txt
```

## Running the test script

The file `test.sh` is the test script. This can be directly with a few test cases. The script will call the interpreter internally.

The test cases are specified in the script code, including call the interpreter to run the test program `test.txt`. In REPL, `load "test.txt"` also call to load the test program.

The script supports following environment variables:

* `UNILANG`: Specify the path of the interpreter, defalted to `./unilang`.
* `PTC`: If not empty, the PTC test case is run. This requires manually termination of the process. During the running, a correct PTC implementation ensures the memory footprint of the intpreter eventually not grow over time.

The built executables built using`sbuild.sh` are not in the current working directory. This can be called by the test script as the following command:

```
UNILANG=build/.debug/unilang.exe ./test.sh
```

# Supported language features

See the examples [introduction of Unilang (zh-CN)](doc/Introduction.zh-CN.md) (not all of them are yet supported) the [feature list document (zh-CN)](doc/Features.zh-CN.md) for features.

See also the [language specification (zh-CN)](doc/Language.zh-CN.md) and the design and the [implementation document of the interpreter (zh-CN)]((Interpreter.zh-CN.md)).

## Known issues

Output of imprecise numbers are compatible to the format of C++ functions in `<cstdio>`, and it is not guaranteed to have expected lexical notations when non-default locales are used:

* The decimal point may be not `.`.
* The output may have characters other than the decimal point, digits, signs and exponent indicators as the delimiters.

Currently, the output number under non-default locales may not be successfully parsed as the numerical literals. 

