// © 2020-2020 Uniontech Software Technology Co.,Ltd.

#include <memory_resource> // for complete std::pmr::polymorphic_allocator;
#include <string> // for std::pmr::string, std::getline;
#include <string_view> // for std::string_view;
#include <iostream>

namespace Unilang
{

using string = std::pmr::string;
using std::string_view;

} // namespace Unilang;

namespace
{

using namespace Unilang;

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
#define APP_VER "0.0.1"
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

