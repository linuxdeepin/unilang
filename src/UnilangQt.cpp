// © 2020 Uniontech Software Technology Co.,Ltd.

#include "UnilangQt.h" // for ReduceReturnUnspecified;
#if __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#	pragma GCC diagnostic ignored "-Wsign-conversion"
#	pragma GCC diagnostic ignored "-Wsign-promo"
#endif
#include <QApplication> // for QApplication;
#include <QWidget> // for QWidget;
#if __GNUC__
#	pragma GCC diagnostic pop
#endif

namespace Unilang
{

namespace
{

} // unnamed namespace;


void
InitializeQt(Interpreter& intp, int& argc, char* argv[])
{
	auto& ctx(intp.Root);
	using namespace Forms;
	using YSLib::unique_ptr;
	using YSLib::make_unique;

	RegisterStrict(ctx, "make-QApplication", [&, argv](TermNode& term){
		RetainN(term, 0);
		term.Value = make_unique<QApplication>(argc, argv);
		return ReductionStatus::Clean;
	});
	RegisterStrict(ctx, "QApplication-exec", [](TermNode& term){
		RetainN(term, 0);
		term.Value = QApplication::exec();
		return ReductionStatus::Clean;
	});
	RegisterStrict(ctx, "make-QWidget", [](TermNode& term){
		RetainN(term, 0);
		term.Value = make_unique<QWidget>();
		return ReductionStatus::Clean;
	});
	RegisterUnary<Strict, const unique_ptr<QWidget>>(ctx, "QWidget-show",
		[](const unique_ptr<QWidget>& p_wgt){
		p_wgt->resize(800, 600);
		p_wgt->show();
		return ValueToken::Unspecified;
	});
}

} // namespace Unilang;

