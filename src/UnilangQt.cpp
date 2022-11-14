// SPDX-FileCopyrightText: 2020-2022 UnionTech Software Technology Co.,Ltd.

#include "UnilangQt.h" // for ReduceReturnUnspecified, YSLib::shared_ptr,
//	YSLib::unique_ptr, YSLib::make_unique, function, YSLib::vector,
//	YSLib::make_shared, std::bind, std::ref;
#include "Exception.h" // for ThrowInsufficientTermsError, ArityMismatch;
#include <cassert> // for assert;
#include <iostream> // for std::cerr, std::endl, std::clog;
#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#	pragma GCC diagnostic ignored "-Wsign-conversion"
#	pragma GCC diagnostic ignored "-Wsign-promo"
#	ifndef __clang__
#		pragma GCC diagnostic ignored "-Wsuggest-attribute=pure"
#	endif
#endif
#include <QHash> // for QHash;
#include <QString> // for QString;
#include YFM_YSLib_Adaptor_YAdaptor // for YSLib::to_std_string;
#include <Qt> // for Qt::AA_EnableHighDpiScaling, Qt::AlignCenter,
//	Qt::ApplicationAttribute, Qt::AlignmentFlag;
#include <QCoreApplication> // for QCoreApplication;
#include <QApplication> // for QApplication;
#include <QWidget> // for QWidget;
#include <QPushButton> // for QPushButton;
#include <QLabel> // for QLabel;
#include <QBoxLayout> // for QLayout, QVBoxLayout;
#include <QQuickView> // for QQuickView;
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif
#include "Forms.h" // for Forms;
#include <ystdex/bind.hpp> // for ystdex::bind1, std::placeholders;
#include YFM_YSLib_Core_YException // for YSLib::FilterExceptions;
#include "Evaluation.h" // for ReduceCombinedBranch;

namespace Unilang
{

namespace
{

using YSLib::shared_ptr;
using YSLib::unique_ptr;
using YSLib::make_unique;

template<typename _fCallable>
shared_ptr<Environment>
GetModuleFor(Context& ctx, _fCallable&& f)
{
	ystdex::guard<EnvironmentSwitcher>
		gd(ctx, Unilang::SwitchToFreshEnvironment(ctx,
		ValueObject(ctx.WeakenRecord())));

	ystdex::invoke(f);
	// TODO: Freeze the environment?
	return ctx.ShareRecord();
}

YB_PURE QWidget&
ResolveQWidget(TermNode& term)
{
	const auto& p_wgt(Unilang::ResolveRegular<shared_ptr<QWidget>>(term));

	assert(p_wgt.get());
	return *p_wgt;
}

using DynamicSlot = function<void(QObject*, void**)>;

struct Sink
{
	QHash<QByteArray, int> SlotIndices;
	YSLib::vector<DynamicSlot> SlotList;

	[[gnu::nonnull(3, 5)]] bool
	ConnectDynamicSlot(const QObject&, const char*,
		const QObject&, const char*, function<DynamicSlot(const char*)>);
};

bool
Sink::ConnectDynamicSlot(const QObject& sender, const char* signal,
	const QObject& receiver, const char* slot,
	function<DynamicSlot(const char*)> create)
{
	const auto the_signal(QMetaObject::normalizedSignature(signal));
	const auto the_slot(QMetaObject::normalizedSignature(slot));

	if(QMetaObject::checkConnectArgs(the_signal, the_slot))
	{
		const int signal_id(sender.metaObject()->indexOfSignal(the_signal));

		if(signal_id >= 0)
		{
			int slot_id(SlotIndices.value(the_slot, -1));

			if(slot_id < 0)
			{
				slot_id = SlotList.size();
				SlotList.push_back(create(the_slot.data()));
				SlotIndices[the_slot] = slot_id;
			}
			return QMetaObject::connect(&sender, signal_id, &receiver,
				slot_id + receiver.metaObject()->methodCount());
		}
		else
			std::cerr << "ERROR: No signal found in the sender for '"
				<< signal << "'." << std::endl;
	}
	return {};
}


class DynamicQObject: public QObject
{
private:
	Sink sink;

public:
	DynamicQObject(QObject* parent = {})
		: QObject(parent)
	{}

	int
	qt_metacall(QMetaObject::Call, int, void**) override;

	Sink&
	GetSinkRef()
	{
		return sink;
	}
};

int
DynamicQObject::qt_metacall(QMetaObject::Call c, int id, void** arguments)
{
	id = QObject::qt_metacall(c, id, arguments);
	if(id < 0 || c != QMetaObject::InvokeMetaMethod)
		return id;
	assert(size_t(id) < sink.SlotList.size());
	sink.SlotList[size_t(id)](sender(), arguments);
	return -1;
}

YB_ATTR_nodiscard YB_PURE QString
MakeQString(const string& s)
{
	return QString::fromStdString(YSLib::to_std_string(s));
}

YB_ATTR_nodiscard YB_PURE string
MakeString(string::allocator_type a, const QString& qs)
{
	const auto& s(qs.toStdString());

	return string({s.cbegin(), s.cend()}, a);
}
YB_ATTR_nodiscard YB_PURE inline string
MakeString(const TermNode& term, const QString& qs)
{
	return MakeString(term.get_allocator(), qs);
}


void
InitializeQtNative(Interpreter& intp, int& argc, char* argv[])
{
	using namespace Forms;
	using YSLib::make_shared;
	auto& rctx(intp.Main);
	auto& renv(rctx.GetRecordRef());

	RegisterStrict(rctx, "make-DynamicQObject", [](TermNode& term){
		RetainN(term, 0);
		term.Value = make_shared<DynamicQObject>();
		return ReductionStatus::Clean;
	});
	RegisterStrict(rctx, "QObject-connect", [&](TermNode& term, Context& ctx){
		RetainN(term, 4);

		auto i(term.begin());
		auto& sender(ResolveQWidget(*++i));
		const auto& signal(Unilang::ResolveRegular<string>(*++i));
		auto& receiver(
			*Unilang::ResolveRegular<shared_ptr<DynamicQObject>>(*++i));
		auto& sink(receiver.GetSinkRef());
		auto& tm_func(*++i);

		// NOTE: Typecheck.
		yunused(ResolveRegular<ContextHandler>(tm_func));
		sink.ConnectDynamicSlot(sender, signal.c_str(), receiver, "slot()",
			[&](const char* slot){
			using namespace std;
			using namespace std::placeholders;

#ifndef NDEBUG
			clog << "DEBUG: Created slot: " << slot << '.' << endl;
#else
			yunused(slot);
#endif
			return DynamicSlot(
				ystdex::bind1([&](QObject*, void**, TermNode& tm) noexcept{
				auto& orig_next(ctx.GetNextTermRef());

				// XXX: Throwing exceptions from an event handler is not
				//	supported by Qt.
				YSLib::FilterExceptions([&]{
#ifndef NDEBUG
					clog << "DEBUG: Slot called." << endl;
#endif
					TermNode tm_v(tm.get_allocator());

					tm_v.Add(Unilang::AsTermNode(
						TermReference(tm, ctx.GetRecordPtr())));

#if true
					Context nctx(ctx.Global);
					// XXX: Always use DefaultReduceOnce for now.

					nctx.SwitchEnvironmentUnchecked(ctx.ShareRecord());
#else
					// XXX: For exposition only. Context::ReductionGuard is not
					//	compatible because it would invalidate the iterator
					//	saved by the context (Interpreter::PrepareExecution).
					// TODO: Better change the 'Rewrite' call after continuation
					//	barrier landed. Also avoided recreation of the context
					//	object by some cache mechanism if it is impossible to
					//	reuse simply.
					Context::ReductionGuard gd(ctx);
					auto& nctx(ctx);

#endif
					nctx.SetNextTermRef(tm_v);
					// NOTE: Trampolined. This cannot be asynchronous which
					//	interleaves with Qt's event loop, as ctx is being
					//	blocked by QApplication::exec on this loop.
					nctx.Rewrite([&](Context& c1){
						intp.PrepareExecution(nctx);
						return ReduceCombinedBranch(tm_v, c1);
					});
				}, "slot event handler");
				ctx.SetNextTermRef(orig_next);
			}, _2, std::move(tm_func)));
		});
		return ReduceReturnUnspecified(term);
	});
	renv.Bindings["Qt.AA_EnableHighDpiScaling"].Value
		= Qt::AA_EnableHighDpiScaling;
	renv.Bindings["Qt.AlignCenter"].Value = Qt::AlignCenter;
	RegisterStrict(rctx, "QCoreApplication-applicationName", [](TermNode& term){
		RetainN(term, 0);
		term.SetValue(MakeString(term, QCoreApplication::applicationName()));
	});
	RegisterStrict(rctx, "QCoreApplication-organizationName",
		[](TermNode& term){
		RetainN(term, 0);
		term.SetValue(MakeString(term, QCoreApplication::organizationName()));
	});
	RegisterStrict(rctx, "QCoreApplication-instance",
		[](TermNode& term){
		RetainN(term, 0);
		term.SetValue(QCoreApplication::instance());
	});
	RegisterUnary<Strict, const string>(rctx,
		"QCoreApplication-setApplicationName", [](const string& str){
		QCoreApplication::setApplicationName(MakeQString(str));
		return ValueToken::Unspecified;
	});
	RegisterUnary<Strict, const string>(rctx,
		"QCoreApplication-setApplicationVersion", [](const string& str){
		QCoreApplication::setApplicationVersion(MakeQString(str));
		return ValueToken::Unspecified;
	});
	RegisterStrict(rctx, "QCoreApplication-setAttribute", [](TermNode& term){
		const auto n(FetchArgumentN(term));

		if(n == 1 || n == 2)
		{
			auto i(term.begin());
			const auto attribute(Unilang::ResolveRegular<
				Qt::ApplicationAttribute>(*++i));

			QCoreApplication::setAttribute(attribute,
				n == 1 || ResolveRegular<bool>(*++i));
			return ReduceReturnUnspecified(term);
		}
		if(n < 1)
			ThrowInsufficientTermsError(term, {}, 1);
		throw ArityMismatch(2, n);
	});
	RegisterStrict(rctx, "make-QApplication", [&, argv](TermNode& term){
		RetainN(term, 0);
		term.Value = make_shared<QApplication>(argc, argv);
		return ReductionStatus::Clean;
	});
	RegisterStrict(rctx, "QApplication-exec", [](TermNode& term){
		RetainN(term, 0);
		term.Value = QApplication::exec();
		return ReductionStatus::Clean;
	});
	RegisterStrict(rctx, "make-QWidget", [](TermNode& term){
		RetainN(term, 0);
		term.Value = make_shared<QWidget>();
		return ReductionStatus::Clean;
	});
	RegisterStrict(rctx, "QWidget-resize", [](TermNode& term){
		RetainN(term, 3);

		auto i(term.begin());
		auto& wgt(ResolveQWidget(*++i));
		const int& w(Unilang::ResolveRegular<int>(*++i));
		const int& h(Unilang::ResolveRegular<int>(*++i));

		wgt.resize(w, h);
		return ReduceReturnUnspecified(term);
	});
	RegisterUnary<Strict, const shared_ptr<QWidget>>(rctx, "QWidget-show",
		[](const shared_ptr<QWidget>& p_wgt){
		p_wgt->show();
		return ValueToken::Unspecified;
	});
	RegisterStrict(rctx, "QWidget-setLayout", [](TermNode& term){
		RetainN(term, 2);

		auto i(term.begin());
		auto& wgt(ResolveQWidget(*++i));
		auto& layout(*Unilang::ResolveRegular<shared_ptr<QLayout>>(*++i));

		wgt.setLayout(&layout);
		return ReduceReturnUnspecified(term);
	});
	RegisterStrict(rctx, "make-QPushButton", [](TermNode& term){
		RetainN(term, 1);

		auto i(term.begin());

		term.Value = shared_ptr<QWidget>(make_shared<QPushButton>(
			Unilang::ResolveRegular<string>(*++i).c_str()));
		return ReductionStatus::Clean;
	});
	RegisterStrict(rctx, "make-QLabel", [](TermNode& term){
		RetainN(term, 2);

		auto i(term.begin());
		const auto& text(Unilang::ResolveRegular<string>(*++i));
		const auto& align(Unilang::ResolveRegular<Qt::AlignmentFlag>(*++i));
		auto p_lbl(make_shared<QLabel>(QString(text.c_str())));

		p_lbl->setAlignment(align);
		term.Value = shared_ptr<QWidget>(std::move(p_lbl));
		return ReductionStatus::Clean;
	});
	RegisterStrict(rctx, "QLabel-setText", [](TermNode& term){
		RetainN(term, 2);

		auto i(term.begin());
		auto& wgt(ResolveQWidget(*++i));
		const auto& text(Unilang::ResolveRegular<string>(*++i));

		dynamic_cast<QLabel&>(wgt).setText(MakeQString(text));
		return ReduceReturnUnspecified(term);
	});
	RegisterStrict(rctx, "make-QVBoxLayout", [](TermNode& term){
		RetainN(term, 0);
		term.Value = shared_ptr<QLayout>(make_shared<QVBoxLayout>());
		return ReductionStatus::Clean;
	});
	RegisterStrict(rctx, "QLayout-addWidget", [](TermNode& term){
		RetainN(term, 2);

		auto i(term.begin());
		auto& layout(*Unilang::ResolveRegular<shared_ptr<QLayout>>(*++i));
		auto& wgt(ResolveQWidget(*++i));

		layout.addWidget(&wgt);
		return ReduceReturnUnspecified(term);
	});
	RegisterStrict(rctx, "make-QQuickView", [](TermNode& term){
		RetainN(term, 0);
		term.Value = make_shared<QQuickView>();
		return ReductionStatus::Clean;
	});
	RegisterStrict(rctx, "QQuickView-show", [](TermNode& term){
		RetainN(term, 1);

		auto i(term.begin());

		Unilang::ResolveRegular<shared_ptr<QQuickView>>(*++i)->show();
		return ReduceReturnUnspecified(term);
	});
	RegisterStrict(rctx, "QQuickView-showFullScreen", [](TermNode& term){
		RetainN(term, 1);

		auto i(term.begin());

		Unilang::ResolveRegular<shared_ptr<QQuickView>>(*++i)->showFullScreen();
		return ReduceReturnUnspecified(term);
	});
	RegisterStrict(rctx, "QQuickView_set-source", [](TermNode& term){
		RetainN(term, 2);

		auto i(term.begin());
		auto& window(*Unilang::ResolveRegular<shared_ptr<QQuickView>>(*++i));

		window.setSource(QUrl(Unilang::ResolveRegular<string>(*++i).c_str()));
		return ReduceReturnUnspecified(term);
	});
	RegisterStrict(rctx, "QQuickView_set-transparent", [](TermNode& term){
		RetainN(term, 1);

		auto i(term.begin());
		auto& window(*Unilang::ResolveRegular<shared_ptr<QQuickView>>(*++i));
		auto sf(window.format());

		sf.setAlphaBufferSize(8);
		window.setFormat(sf);
		window.setColor(Qt::transparent);
		return ReduceReturnUnspecified(term);
	});
}

} // unnamed namespace;


void
InitializeQt(Interpreter& intp, int& argc, char* argv[])
{
	auto& rctx(intp.Main);

	rctx.GetRecordRef().Bindings["UnilangQt.native__"].Value
		= GetModuleFor(rctx, std::bind(InitializeQtNative, std::ref(intp),
		std::ref(argc), argv));
	intp.Perform(R"Unilang(
		$def! UnilangQt $let ()
		(
			$import! UnilangQt.native__ QObject-connect
				Qt.AA_EnableHighDpiScaling
				QCoreApplication-setAttribute
				make-QApplication QApplication-exec
				make-QWidget QWidget-resize QWidget-show QWidget-setLayout
				make-QPushButton
				Qt.AlignCenter
				make-QLabel QLabel-setText
				make-QVBoxLayout QLayout-addWidget
				make-QQuickView QQuickView-show QQuickView-showFullScreen
				QQuickView_set-source QQuickView_set-transparent;
			$def! impl__ $provide!
			(
				QWidget
			)
			(
				$import! std.classes make-class;
				$import! UnilangQt.native__ make-DynamicQObject;
				$def! QWidget make-class () ($lambda (self)
				(
					$set! self _widget () make-QWidget;
					$set! self _dynamic () make-DynamicQObject;
					$set! self QWidget-setLayout QWidget-setLayout;
					$set! self setLayout
						$lambda/e self (layout)
							QWidget-setLayout _widget layout;
					$set! self QWidget-resize QWidget-resize;
					$set! self resize
						$lambda/e self (w h) QWidget-resize _widget w h;
					$set! self QWidget-show QWidget-show;
					$set! self show $lambda/e self () QWidget-show _widget;
				));
			);
			() lock-current-environment
		);
	)Unilang");
}

} // namespace Unilang;

