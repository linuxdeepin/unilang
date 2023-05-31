// SPDX-FileCopyrightText: 2020-2023 UnionTech Software Technology Co.,Ltd.

#include "UnilangQt.h" // for YSLib::shared_ptr, YSLib::unique_ptr,
//	YSLib::make_unique, function, vector, Environment, Unilang::make_shared,
//	ReduceReturnUnspecified, std::bind, std::ref;
#include "TermAccess.h" // for Unilang::ResolveTerm, Unilang::CheckRegular,
//	Access, Unilang::ResolveRegular;
#include <cassert> // for assert;
#include YFM_YSLib_Core_YCoreUtilities // for YSLib::CheckUpperBound;
#include <iostream> // for std::cerr, std::endl, std::clog;
#include "Exception.h" // for ThrowInsufficientTermsError, ArityMismatch;
#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#	pragma GCC diagnostic ignored "-Wsign-conversion"
#	pragma GCC diagnostic ignored "-Wsign-promo"
#	ifndef __clang__
#		pragma GCC diagnostic ignored "-Wsuggest-attribute=pure"
#	endif
#endif
#include <QtGlobal> // for QT_VERSION, QT_VERSION_CHECK;
#include <QHash> // for QHash;
#include <QByteArray> // for QByteArray;
#include <QString> // for QString;
#include YFM_YSLib_Adaptor_YAdaptor // for YSLib::to_std_string;
#include <QtConfig> // for QT_VERSION_STR;
#include <Qt> // for Qt::AA_EnableHighDpiScaling, Qt::AlignCenter,
//	Qt::ApplicationAttribute, Qt::InputMethodQuery, Qt::AlignmentFlag,
//	Qt::WindowFlags;
#include <QCoreApplication> // for QCoreApplication;
#include <QGuiApplication> // for QGuiApplication;
#include <QApplication> // for QApplication;
#include <QCursor> // for QCursor;
#include <QAction> // for QAction;
#include <QWidget> // for QWidget;
#include <QPoint> // for QPoint;
#include <QSize> // for QSize;
#include <QPushButton> // for QPushButton;
#include <QLabel> // for QLabel;
#include <QBoxLayout> // for QLayout, QVBoxLayout;
#include <QMainWindow> // for QMainWindow;
#include <QQmlApplicationEngine> // for QQmlApplicationEngine;
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

YB_PURE QWidget&
ResolveQWidget(TermNode& term)
{
	const auto& p_wgt(Unilang::ResolveRegular<const shared_ptr<QWidget>>(term));

	assert(p_wgt.get());
	return *p_wgt;
}

YB_PURE const QWidget&
ResolveConstQWidget(TermNode& term)
{
	const auto& p_wgt(Unilang::ResolveTerm(
		[&](TermNode& nd, bool has_ref) -> shared_ptr<const QWidget>{
		Unilang::CheckRegular<shared_ptr<QWidget>>(nd, has_ref);
		if(const auto p = TryAccessLeafAtom<const shared_ptr<QWidget>>(term))
			return *p;
		return Access<const shared_ptr<const QWidget>>(nd);
	}, term));

	assert(p_wgt.get());
	return *p_wgt;
}

using DynamicSlot = function<void(QObject*, void**)>;

struct Sink
{
	QHash<QByteArray, int> SlotIndices;
	vector<DynamicSlot> SlotList;

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
				slot_id = YSLib::CheckUpperBound<int>(SlotList.size());
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
	GetSinkRef() noexcept
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
	auto& rctx(intp.Main);
	auto& renv(rctx.GetRecordRef());
	auto& m(renv.GetMapRef());

	Environment::DefineChecked(m, "QT_VERSION_STR",
		string(QT_VERSION_STR, m.get_allocator()));
	RegisterStrict(m, "make-DynamicQObject", [](TermNode& term){
		RetainN(term, 0);
		term.Value = Unilang::make_shared<DynamicQObject>();
	});
	RegisterStrict(m, "QObject-connect", [&](TermNode& term, Context& ctx){
		RetainN(term, 4);

		auto i(term.begin());
		// TODO: Extend to real QObject.
		auto& sender(ResolveQWidget(*++i));
		const auto& signal(Unilang::ResolveRegular<const string>(*++i));
		auto& receiver(
			*Unilang::ResolveRegular<shared_ptr<DynamicQObject>>(*++i));
		auto& sink(receiver.GetSinkRef());
		auto& tm_func(*++i);

		// NOTE: Typecheck.
		yunused(Unilang::ResolveRegular<const ContextHandler>(tm_func));
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
	m["Qt.AA_EnableHighDpiScaling"].Value = Qt::AA_EnableHighDpiScaling;
	m["Qt.AlignCenter"].Value = Qt::AlignCenter;
	RegisterStrict(m, "QCoreApplication-applicationName", [](TermNode& term){
		RetainN(term, 0);
		term.SetValue(MakeString(term, QCoreApplication::applicationName()));
	});
	RegisterStrict(m, "QCoreApplication-organizationName",
		[](TermNode& term){
		RetainN(term, 0);
		term.SetValue(MakeString(term, QCoreApplication::organizationName()));
	});
	RegisterStrict(m, "QCoreApplication-instance",
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
	RegisterStrict(m, "QCoreApplication-setAttribute", [](TermNode& term){
		const auto n(FetchArgumentN(term));

		if(n == 1 || n == 2)
		{
			auto i(term.begin());
			const auto attribute(Unilang::ResolveRegular<
				const Qt::ApplicationAttribute>(*++i));

			QCoreApplication::setAttribute(attribute,
				n == 1 || Unilang::ResolveRegular<const bool>(*++i));
			return ReduceReturnUnspecified(term);
		}
		if(n < 1)
			ThrowInsufficientTermsError(term, {}, 1);
		throw ArityMismatch(2, n);
	});
	RegisterUnary<Strict, const string>(rctx,
		"QCoreApplication-setOrganizationName", [](const string& str){
		QCoreApplication::setOrganizationName(MakeQString(str));
		return ValueToken::Unspecified;
	});
	RegisterStrict(m, "make-QGuiApplication", [&, argv](TermNode& term){
		RetainN(term, 0);
		term.Value = Unilang::make_shared<QGuiApplication>(argc, argv);
	});
	RegisterStrict(m, "QGuiApplication-exec", [](TermNode& term){
		RetainN(term, 0);
		term.Value = QGuiApplication::exec();
	});
	RegisterStrict(m, "QGuiApplication-restoreOverrideCursor",
		[](TermNode& term){
		RetainN(term, 0);
		QGuiApplication::restoreOverrideCursor();
		return ReduceReturnUnspecified(term);
	});
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0) \
	&& QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	RegisterUnary<Strict, const bool>(rctx,
		"QGuiApplication-setFallbackSessionManagementEnabled", [](bool enabled){
		QGuiApplication::setFallbackSessionManagementEnabled(enabled);
		return ValueToken::Unspecified;
	});
#endif
	RegisterUnary<Strict, const QCursor>(rctx,
		"QGuiApplication-setOverrideCursor", [](const QCursor& cursor){
		QGuiApplication::setOverrideCursor(cursor);
		return ValueToken::Unspecified;
	});
	RegisterStrict(m, "make-QApplication", [&, argv](TermNode& term){
		RetainN(term, 0);
		term.Value = Unilang::make_shared<QApplication>(argc, argv);
	});
	RegisterStrict(m, "QApplication-aboutQt", [](TermNode& term){
		RetainN(term, 0);
		QApplication::aboutQt();
		return ReduceReturnUnspecified(term);
	});
	RegisterStrict(m, "QApplication-exec", [](TermNode& term){
		RetainN(term, 0);
		term.Value = QApplication::exec();
	});
	RegisterStrict(m, "make-QWidget", [](TermNode& term){
		RetainN(term, 0);
		term.Value = Unilang::make_shared<QWidget>();
	});
	RegisterStrict(m, "QWidget-addAction", [](TermNode& term){
		RetainN(term, 2);

		auto i(term.begin());
		auto& wgt(ResolveQWidget(*++i));

		wgt.addAction(Unilang::ResolveRegular<QAction* const>(*++i));
		return ReduceReturnUnspecified(term);
	});
	RegisterStrict(m, "QWidget-close", [](TermNode& term){
		RetainN(term);

		auto i(term.begin());

		term.Value = ResolveQWidget(*++i).close();
	});
	RegisterStrict(m, "QWidget-hasHeightForWidth", [](TermNode& term){
		RetainN(term);

		auto i(term.begin());

		term.Value = ResolveConstQWidget(*++i).hasHeightForWidth();
	});
	RegisterStrict(m, "QWidget-heightForWidth", [](TermNode& term){
		RetainN(term, 2);

		auto i(term.begin());
		auto& wgt(ResolveConstQWidget(*++i));

		term.Value
			= wgt.heightForWidth(Unilang::ResolveRegular<const int>(*++i));
	});
	RegisterStrict(m, "QWidget-inputMethodQuery", [](TermNode& term){
		RetainN(term, 2);

		auto i(term.begin());
		auto& wgt(ResolveConstQWidget(*++i));

		term.Value = wgt.inputMethodQuery(
			Unilang::ResolveRegular<const Qt::InputMethodQuery>(*++i));
	});
	RegisterStrict(m, "QWidget-minimumSizeHint", [](TermNode& term){
		RetainN(term);

		auto i(term.begin());

		term.Value = ResolveConstQWidget(*++i).minimumSizeHint();
	});
	RegisterStrict(m, "QWidget-minimumSizeHint", [](TermNode& term){
		RetainN(term);

		auto i(term.begin());

		term.Value = ResolveConstQWidget(*++i).minimumSizeHint();
	});
	RegisterStrict(m, "QWidget-move", [](TermNode& term){
		const auto n(FetchArgumentN(term));

		if(n == 2 || n == 3)
		{
			auto i(term.begin());
			auto& wgt(ResolveQWidget(*++i));
			if(n == 2)
				wgt.move(Unilang::ResolveRegular<const QPoint>(*++i));
			else
			{
				const int& x(Unilang::ResolveRegular<const int>(*++i));

				wgt.move(x, Unilang::ResolveRegular<const int>(*++i));
			}
			return ReduceReturnUnspecified(term);
		}
		if(n < 2)
			ThrowInsufficientTermsError(term, {}, 2);
		throw ArityMismatch(3, n);
	});
	RegisterStrict(m, "QWidget-paintEngine", [](TermNode& term){
		RetainN(term);

		auto i(term.begin());

		term.Value = ResolveConstQWidget(*++i).paintEngine();
	});
	RegisterStrict(m, "QWidget-resize", [](TermNode& term){
		const auto n(FetchArgumentN(term));

		if(n == 2 || n == 3)
		{
			auto i(term.begin());
			auto& wgt(ResolveQWidget(*++i));
			if(n == 2)
				wgt.resize(Unilang::ResolveRegular<const QSize>(*++i));
			else
			{
				const int& w(Unilang::ResolveRegular<const int>(*++i));

				wgt.resize(w, Unilang::ResolveRegular<const int>(*++i));
			}
			return ReduceReturnUnspecified(term);
		}
		if(n < 2)
			ThrowInsufficientTermsError(term, {}, 2);
		throw ArityMismatch(3, n);
	});
	RegisterBinary<Strict, const shared_ptr<QWidget>, const string>(rctx,
		"QWidget-setWindowFilePath",
		[](const shared_ptr<QWidget>& p_wgt, const string& str){
		p_wgt->setWindowFilePath(MakeQString(str));
		return ValueToken::Unspecified;
	});
	RegisterUnary<Strict, const shared_ptr<QWidget>>(rctx, "QWidget-show",
		[](const shared_ptr<QWidget>& p_wgt){
		p_wgt->show();
		return ValueToken::Unspecified;
	});
	RegisterStrict(m, "QWidget-restoreGeometry", [](TermNode& term){
		RetainN(term, 2);

		auto i(term.begin());
		auto& wgt(ResolveQWidget(*++i));

		term.Value = wgt.restoreGeometry(
			Unilang::ResolveRegular<const QByteArray>(*++i));
	});
	RegisterStrict(m, "QWidget-saveGeometry", [](TermNode& term){
		RetainN(term);

		auto i(term.begin());

		term.Value = ResolveConstQWidget(*++i).saveGeometry();
	});
	RegisterStrict(m, "QWidget-setLayout", [](TermNode& term){
		RetainN(term, 2);

		auto i(term.begin());
		auto& wgt(ResolveQWidget(*++i));
		auto& layout(*Unilang::ResolveRegular<const shared_ptr<QLayout>>(*++i));

		wgt.setLayout(&layout);
		return ReduceReturnUnspecified(term);
	});
	RegisterStrict(m, "QWidget-setVisible", [](TermNode& term){
		RetainN(term, 2);

		auto i(term.begin());
		auto& wgt(ResolveQWidget(*++i));

		wgt.setVisible(Unilang::ResolveRegular<const bool>(*++i));
		return ReduceReturnUnspecified(term);
	});
	RegisterStrict(m, "QWidget-sizeHint", [](TermNode& term){
		RetainN(term);

		auto i(term.begin());

		term.Value = ResolveConstQWidget(*++i).paintEngine();
	});
	RegisterStrict(m, "make-QPushButton", [](TermNode& term){
		RetainN(term, 1);

		auto i(term.begin());

		term.Value = shared_ptr<QWidget>(Unilang::make_shared<QPushButton>(
			Unilang::ResolveRegular<const string>(*++i).c_str()));
	});
	RegisterStrict(m, "make-QLabel", [](TermNode& term){
		RetainN(term, 2);

		auto i(term.begin());
		const auto& text(Unilang::ResolveRegular<const string>(*++i));
		const auto& align(
			Unilang::ResolveRegular<const Qt::AlignmentFlag>(*++i));
		auto p_lbl(Unilang::make_shared<QLabel>(QString(text.c_str())));

		p_lbl->setAlignment(align);
		term.Value = shared_ptr<QWidget>(std::move(p_lbl));
	});
	RegisterStrict(m, "QLabel-setText", [](TermNode& term){
		RetainN(term, 2);

		auto i(term.begin());
		auto& wgt(ResolveQWidget(*++i));
		const auto& text(Unilang::ResolveRegular<const string>(*++i));

		dynamic_cast<QLabel&>(wgt).setText(MakeQString(text));
		return ReduceReturnUnspecified(term);
	});
	RegisterStrict(m, "make-QVBoxLayout", [](TermNode& term){
		RetainN(term, 0);
		term.Value = shared_ptr<QLayout>(Unilang::make_shared<QVBoxLayout>());
	});
	RegisterStrict(m, "make-QMainWindow", [](TermNode& term){
		const auto n(FetchArgumentN(term));

		if(n < 2)
		{
			auto i(term.begin());
			const auto p_wgt(n == 0 ? nullptr
				: Unilang::ResolveRegular<QWidget* const>(*++i));

			term.Value = shared_ptr<QWidget>(Unilang::make_shared<QMainWindow>(
				p_wgt, n == 2 ? Unilang::ResolveRegular<const Qt::WindowFlags>(
				*++i) : Qt::WindowFlags()));
		}
		throw ArityMismatch(2, n);
	});
	RegisterStrict(m, "QMainWindow-addToolBar", [](TermNode& term){
		RetainN(term, 2);

		auto i(term.begin());
		auto& wgt(ResolveQWidget(*++i));

		term.Value = dynamic_cast<QMainWindow&>(wgt).addToolBar(
			MakeQString(Unilang::ResolveRegular<const string>(*++i)));
	});
	RegisterStrict(m, "QMainWindow-createPopupMenu", [](TermNode& term){
		RetainN(term);

		auto i(term.begin());

		term.Value = dynamic_cast<QMainWindow&>(
			ResolveQWidget(*++i)).createPopupMenu();
	});
	RegisterStrict(m, "QMainWindow-menuBar", [](TermNode& term){
		RetainN(term);

		auto i(term.begin());

		term.Value = dynamic_cast<const QMainWindow&>(
			ResolveConstQWidget(*++i)).menuBar();
	});
	RegisterStrict(m, "QMainWindow-setCentralWidget", [](TermNode& term){
		RetainN(term, 2);

		auto i(term.begin());
		auto& wgt(ResolveQWidget(*++i));

		dynamic_cast<QMainWindow&>(wgt).setCentralWidget(&ResolveQWidget(*++i));
		return ReduceReturnUnspecified(term);
	});
	RegisterBinary<Strict, const shared_ptr<QWidget>, const bool>(rctx,
		"QMainWindow-setUnifiedTitleAndToolBarOnMac",
		[](const shared_ptr<QWidget>& p_wgt, bool set){
		dynamic_cast<QMainWindow&>(*p_wgt).setUnifiedTitleAndToolBarOnMac(set);
		return ValueToken::Unspecified;
	});
	RegisterStrict(m, "QMainWindow-statusBar", [](TermNode& term){
		RetainN(term);

		auto i(term.begin());

		term.Value = dynamic_cast<const QMainWindow&>(
			ResolveConstQWidget(*++i)).statusBar();
	});
	RegisterStrict(m, "QLayout-addWidget", [](TermNode& term){
		RetainN(term, 2);

		auto i(term.begin());
		auto& layout(*Unilang::ResolveRegular<const shared_ptr<QLayout>>(*++i));
		auto& wgt(ResolveQWidget(*++i));

		layout.addWidget(&wgt);
		return ReduceReturnUnspecified(term);
	});
	RegisterStrict(m, "make-QQmlApplicationEngine", [](TermNode& term){
		RetainN(term);

		auto i(term.begin());

		term.Value = Unilang::make_shared<QQmlApplicationEngine>(
			Unilang::ResolveRegular<QObject* const>(*++i));
	});	
	RegisterStrict(m, "QQmlApplicationEngine-load", [](TermNode& term){
		RetainN(term, 2);

		auto i(term.begin());
		auto& engine(*Unilang::ResolveRegular<
			const shared_ptr<QQmlApplicationEngine>>(*++i));

		Unilang::ResolveTerm([&](const TermNode& nd, bool has_ref){
			Unilang::CheckRegular<QUrl>(nd, has_ref);
			if(const auto p = TryAccessLeafAtom<const QUrl>(term))
				engine.load(*p);
			else
				engine.load(MakeQString(Access<const string>(nd)));
		}, *++i);
		return ReduceReturnUnspecified(term);
	});
	RegisterStrict(m, "QQmlApplicationEngine-rootObjects",
		[](TermNode& term){
		RetainN(term);

		auto i(term.begin());

		// XXX: Support 'const QQmlApplicationEngine'?
		term.Value = Unilang::ResolveRegular<
			const shared_ptr<QQmlApplicationEngine>>(*++i)->rootObjects();
	});	
	RegisterStrict(m, "make-QQuickView", [](TermNode& term){
		RetainN(term, 0);
		term.Value = Unilang::make_shared<QQuickView>();
	});
	RegisterStrict(m, "QQuickView-show", [](TermNode& term){
		RetainN(term, 1);

		auto i(term.begin());

		Unilang::ResolveRegular<const shared_ptr<QQuickView>>(*++i)->show();
		return ReduceReturnUnspecified(term);
	});
	RegisterStrict(m, "QQuickView-showFullScreen", [](TermNode& term){
		RetainN(term, 1);

		auto i(term.begin());

		Unilang::ResolveRegular<
			const shared_ptr<QQuickView>>(*++i)->showFullScreen();
		return ReduceReturnUnspecified(term);
	});
	RegisterStrict(m, "QQuickView_set-source", [](TermNode& term){
		RetainN(term, 2);

		auto i(term.begin());
		auto& window(
			*Unilang::ResolveRegular<const shared_ptr<QQuickView>>(*++i));

		window.setSource(
			QUrl(Unilang::ResolveRegular<const string>(*++i).c_str()));
		return ReduceReturnUnspecified(term);
	});
	RegisterStrict(m, "QQuickView_set-transparent", [](TermNode& term){
		RetainN(term, 1);

		auto i(term.begin());
		auto& window(
			*Unilang::ResolveRegular<const shared_ptr<QQuickView>>(*++i));
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

	rctx.GetRecordRef().GetMapRef()["UnilangQt.native__"].Value
		= GetModuleFor(rctx, std::bind(InitializeQtNative, std::ref(intp),
		std::ref(argc), argv));
	intp.Main.ShareCurrentSource("<lib:UnilangQt>");
	intp.Perform(R"Unilang(
$def! UnilangQt make-environment ($let ()
(
	$def! impl__ $provide!
	(
		QWidget
	)
	(
		$import! std.classes make-class;
		$import! UnilangQt.native__ make-QWidget make-DynamicQObject
			QWidget-setLayout QWidget-setLayout QWidget-resize QWidget-show;

		$def! QWidget make-class () ($lambda (self)
		(
			$defv! $inh! (sym) d
				eval (list $def! sym (unwrap eval%) sym d) self;
			$set! self _widget () make-QWidget;
			$set! self _dynamic () make-DynamicQObject;
			$inh! QWidget-setLayout;
			$set! self setLayout
				$lambda/e self (layout)
					QWidget-setLayout _widget layout;
			$inh! QWidget-resize;
			$set! self resize
				$lambda/e self (w h) QWidget-resize _widget w h;
			$inh! QWidget-show;
			$set! self show $lambda/e self () QWidget-show _widget;
		));
	);
	() lock-current-environment
)) UnilangQt.native__;
	)Unilang");
}

} // namespace Unilang;

