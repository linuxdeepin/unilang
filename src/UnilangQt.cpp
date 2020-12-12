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
#include <QPushButton> // for QPushButton;
#include <QLabel> // for QString, QLabel;
#include <QBoxLayout> // for QLayout, QVBoxLayout;
#if __GNUC__
#	pragma GCC diagnostic pop
#endif
#include <cassert> // for assert;

namespace Unilang
{

namespace
{

using YSLib::shared_ptr;

QWidget&
ResolveQWidget(TermNode& term)
{
	const auto& p_wgt(Unilang::ResolveRegular<shared_ptr<QWidget>>(term));

	assert(p_wgt.get());
	return *p_wgt;
}

} // unnamed namespace;


void
InitializeQt(Interpreter& intp, int& argc, char* argv[])
{
	auto& ctx(intp.Root);
	using namespace Forms;
	using YSLib::make_shared;

	RegisterStrict(ctx, "make-QApplication", [&, argv](TermNode& term){
		RetainN(term, 0);
		term.Value = make_shared<QApplication>(argc, argv);
		return ReductionStatus::Clean;
	});
	RegisterStrict(ctx, "QApplication-exec", [](TermNode& term){
		RetainN(term, 0);
		term.Value = QApplication::exec();
		return ReductionStatus::Clean;
	});
	RegisterStrict(ctx, "make-QWidget", [](TermNode& term){
		RetainN(term, 0);
		term.Value = make_shared<QWidget>();
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
	RegisterUnary<Strict, const shared_ptr<QWidget>>(ctx, "QWidget-show",
		[](const shared_ptr<QWidget>& p_wgt){
		p_wgt->show();
		return ValueToken::Unspecified;
	});
	RegisterStrict(ctx, "QWidget-setLayout", [](TermNode& term){
		RetainN(term, 2);

		auto i(term.begin());
		auto& wgt(*Unilang::ResolveRegular<shared_ptr<QWidget>>(*++i));
		auto& layout(*Unilang::ResolveRegular<shared_ptr<QLayout>>(*++i));

		wgt.setLayout(&layout);
		return ReduceReturnUnspecified(term);
	});
	RegisterStrict(ctx, "make-QPushButton", [](TermNode& term){
		RetainN(term, 1);

		auto i(term.begin());

		term.Value = shared_ptr<QWidget>(make_shared<QPushButton>(
			Unilang::ResolveRegular<string>(*++i).c_str()));
		return ReductionStatus::Clean;
	});
	ctx.GetRecordRef().Bindings["Qt.AlignCenter"].Value = Qt::AlignCenter;
	RegisterStrict(ctx, "make-QLabel", [](TermNode& term){
		RetainN(term, 2);

		auto i(term.begin());
		const auto& text(Unilang::ResolveRegular<string>(*++i));
		const auto& align(Unilang::ResolveRegular<Qt::AlignmentFlag>(*++i));
		auto p_lbl(make_shared<QLabel>(QString(text.c_str())));

		p_lbl->setAlignment(align);
		term.Value = shared_ptr<QWidget>(std::move(p_lbl));
		return ReductionStatus::Clean;
	});
	RegisterStrict(ctx, "make-QVBoxLayout", [](TermNode& term){
		RetainN(term, 0);
		term.Value = shared_ptr<QLayout>(make_shared<QVBoxLayout>());
		return ReductionStatus::Clean;
	});
	RegisterStrict(ctx, "QLayout-addWidget", [](TermNode& term){
		RetainN(term, 2);

		auto i(term.begin());
		auto& layout(*Unilang::ResolveRegular<shared_ptr<QLayout>>(*++i));
		auto& wgt(*Unilang::ResolveRegular<shared_ptr<QWidget>>(*++i));

		layout.addWidget(&wgt);
		return ReduceReturnUnspecified(term);
	});
}

} // namespace Unilang;

