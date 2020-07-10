// © 2020-2020 Uniontech Software Technology Co.,Ltd.

#include <memory_resource> // for complete std::pmr::polymorphic_allocator;
#include <list> // for std::pmr::list;
#include <string> // for std::pmr::string, std::getline;
#include <string_view> // for std::string_view;
#include <any> // for std::any;
#include <iostream>

namespace Unilang
{

using std::pmr::list;
using std::pmr::string;
using std::string_view;

} // namespace Unilang;

namespace
{

using namespace Unilang;

using ValueObject = std::any;


class TermNode final
{
public:
	using Container = list<TermNode>;
	using allocator_type = Container::allocator_type;
	using iterator = Container::iterator;
	using const_iterator = Container::const_iterator;
	using reverse_iterator = Container::reverse_iterator;
	using const_reverse_iterator = Container::const_reverse_iterator;

	Container Subterms{};
	ValueObject Value{};

	TermNode() = default;
	explicit
	TermNode(allocator_type a)
		: Subterms(a)
	{}
	TermNode(const Container& con)
		: Subterms(con)
	{}
	TermNode(Container&& con)
		: Subterms(std::move(con))
	{}
	TermNode(const TermNode&) = default;
	TermNode(const TermNode& tm, allocator_type a)
		: Subterms(tm.Subterms, a), Value(tm.Value)
	{}
	TermNode(TermNode&&) = default;
	TermNode(TermNode&& tm, allocator_type a)
		: Subterms(std::move(tm.Subterms), a), Value(std::move(tm.Value))
	{}
	
	TermNode&
	operator=(TermNode&&) = default;

	void
	Add(const TermNode& term)
	{
		Subterms.push_back(term);
	}
	void
	Add(TermNode&& term)
	{
		Subterms.push_back(std::move(term));
	}

	[[nodiscard, gnu::pure]]
	allocator_type
	get_allocator() const noexcept
	{
		return Subterms.get_allocator();
	}
};


template<typename... _tParams>
[[nodiscard, gnu::pure]] inline TermNode
AsTermNode(_tParams&&... args)
{
	TermNode term;

	term.Value = ValueObject(std::forward<_tParams>(args)...);
	return term;
}


class Interpreter final
{
private:
	string line{};

public:
	Interpreter();
	Interpreter(const Interpreter&) = delete;

	bool
	Process();

	void
	Run();

	std::istream&
	WaitForLine();
};

Interpreter::Interpreter()
{}

bool
Interpreter::Process()
{
	if(line.empty())
		return true;
	if(line == "exit")
		return {};
	return true;
}

void
Interpreter::Run()
{
	while(WaitForLine() && Process())
		;
}

std::istream&
Interpreter::WaitForLine()
{
	using namespace std;

	cout << "> ";
	return getline(cin, line);
}


#define APP_NAME "Unilang demo"
#define APP_VER "0.0.2"
#define APP_PLATFORM "[C++17]"
constexpr auto
	title(APP_NAME " " APP_VER " @ (" __DATE__ ", " __TIME__ ") " APP_PLATFORM);

} // unnamed namespace;

int
main()
{
	using namespace std;
	Interpreter intp{};

	cout << title << endl << "Initializing...";
	cout << "Initialization finished." << endl;
	cout << "Type \"exit\" to exit, \"cls\" to clear screen." << endl << endl;
	intp.Run();
}

