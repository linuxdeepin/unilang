// © 2020 Uniontech Software Technology Co.,Ltd.

#include "UnilangQt.h" // for ReduceReturnUnspecified;
#if __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#	pragma GCC diagnostic ignored "-Wsign-conversion"
#	pragma GCC diagnostic ignored "-Wsign-promo"
#endif
#include <QWidget> // for QWidget;
#include <QApplication> // for QApplication;
#if __GNUC__
#	pragma GCC diagnostic pop
#endif
#include <cassert> // for assert;

namespace Unilang
{

namespace
{

using YSLib::unique_ptr;

QWidget&
ResolveQWidget(TermNode& term)
{
	const auto& p_wgt(Unilang::ResolveRegular<unique_ptr<QWidget>>(term));

	assert(p_wgt.get());
	return *p_wgt;
}

} // unnamed namespace;


void
InitializeQt(Interpreter& intp, int& argc, char* argv[])
{
	auto& ctx(intp.Root);
	using namespace Forms;
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
	RegisterStrict(ctx, "QWidget-resize", [](TermNode& term){
		RetainN(term, 3);

		auto i(term.begin());
		auto& wgt(ResolveQWidget(*++i));
		const auto& w(Unilang::ResolveRegular<int>(*++i));
		const auto& h(Unilang::ResolveRegular<int>(*++i));

		wgt.resize(w, h);
		return ReduceReturnUnspecified(term);
	});
	RegisterUnary<Strict, const unique_ptr<QWidget>>(ctx, "QWidget-show",
		[](const unique_ptr<QWidget>& p_wgt){
		p_wgt->show();
		return ValueToken::Unspecified;
	});
}

} // namespace Unilang;

