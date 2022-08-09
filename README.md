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
	* Nevertheless, if only using these targets as one of the optional output configurations (for instance, using WebAssembly as the generation target), there will be less problems as above。
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

If new options are put forward without avoiding this situation, it will only further hinder the solution of the problem. Therefore, on the basis of meeting the needs, we hope that the new language can truly achieve universality in a deeper way - by reducing the built-in *ad-hoc* features specifically for individual problem areas and replacing them with a more general set of primitive features.

> Programming languages should be designed not by piling feature on top of feature, but by removing the weaknesses and restrictions that make additional features appear necessary.

<p align="right">—— <a href="https://schemers.org/Documents/Standards/">R<sup>n</sup>RS</a> & <a href="https://ftp.cs.wpi.edu/pub/techreports/pdf/05-07.pdf">R<sup>-1</sup>RK</a></p>

