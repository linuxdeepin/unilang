// © 2020 Uniontech Software Technology Co.,Ltd.

#include "UnilangQt.h" // for ReduceReturnUnspecified, YSLib::shared_ptr,
//	YSLib::unique_ptr, YSLib::make_unique, function, YSLib::vector,
//	YSLib::make_shared;
#include <cassert> // for assert;
#include <iostream> // for std::cerr, std::endl, std::clog;
#if __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#	pragma GCC diagnostic ignored "-Wsign-conversion"
#	pragma GCC diagnostic ignored "-Wsign-promo"
#endif
#include <QHash> // for QHash;
#include <QApplication> // for QApplication;
#include <QWidget> // for QWidget;
#include <QPushButton> // for QPushButton;
#include <QLabel> // for QString, QLabel;
#include <QBoxLayout> // for QLayout, QVBoxLayout;
#if __GNUC__
#	pragma GCC diagnostic pop
#endif
#include YFM_YSLib_Core_YException // for YSLib::FilterExceptions;
#include "Evaluation.h" // for ReduceCombined;

namespace Unilang
{

namespace
{

using YSLib::shared_ptr;
using YSLib::unique_ptr;
using YSLib::make_unique;

QWidget&
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

} // unnamed namespace;


void
InitializeQt(Interpreter& intp, int& argc, char* argv[])
{
	auto& ctx(intp.Root);
	using namespace Forms;
	using YSLib::make_shared;

	RegisterStrict(ctx, "make-DynamicQObject", [](TermNode& term){
		RetainN(term, 0);
		term.Value = make_shared<DynamicQObject>();
		return ReductionStatus::Clean;
	});
	RegisterStrict(ctx, "QObject-connect", [](TermNode& term, Context& c){
		RetainN(term, 4);

		auto i(term.begin());
		auto& sender(ResolveQWidget(*++i));
		const auto& signal(Unilang::ResolveRegular<string>(*++i));
		auto& receiver(
			*Unilang::ResolveRegular<shared_ptr<DynamicQObject>>(*++i));
		auto& sink(receiver.GetSinkRef());

		// NOTE: Typecheck.
		yunused(ResolveRegular<ContextHandler>(*++i));
		sink.ConnectDynamicSlot(sender, signal.c_str(), receiver,
			"slot()", [&](const char* slot){
			using namespace std;
			using namespace placeholders;

#ifndef NDEBUG
			clog << "DEBUG: Created slot: " << slot << '.' << endl;
#endif
			LiftOther(term, *term.rbegin());
			return DynamicSlot(
				std::bind([&](QObject*, void**, TermNode& tm) noexcept{
				// XXX: Throwing exceptions from an event handler is not
				//	supported by Qt.
				YSLib::FilterExceptions([&]{
#ifndef NDEBUG
					clog << "DEBUG: Slot called." << endl;
#endif
					TermNode tm_v(tm.get_allocator());

					tm_v.Add(Unilang::AsTermNode(
						TermReference(tm, c.GetRecordPtr())));

					Context::ReductionGuard gd(c);
					auto& orig_next(c.GetNextTermRef());

					c.SetNextTermRef(tm_v);
					// NOTE: Trampolined. This cannot be asynchnous which
					//	interleaves with Qt's event loop.
					c.Rewrite([&](Context& c1){
						return ReduceCombinedBranch(tm_v, c1);
					});
					c.SetNextTermRef(orig_next);
				}, "slot event handler");
			}, _1, _2, std::move(term)));
		});
		return ReduceReturnUnspecified(term);
	});
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
		auto& wgt(ResolveQWidget(*++i));
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
	RegisterStrict(ctx, "QLabel-setText", [](TermNode& term){
		RetainN(term, 2);

		auto i(term.begin());
		auto& wgt(ResolveQWidget(*++i));
		const auto& text(Unilang::ResolveRegular<string>(*++i));

		dynamic_cast<QLabel&>(wgt).setText(
			QString::fromStdString(YSLib::to_std_string(text)));
		return ReduceReturnUnspecified(term);
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
		auto& wgt(ResolveQWidget(*++i));

		layout.addWidget(&wgt);
		return ReduceReturnUnspecified(term);
	});
	intp.Perform(R"Unilang(
		$defv! $remote-eval (&o &e) d eval o (eval e d);
		$def! std.classes $let ()
		(
			$def! impl__ $provide!
			(
				make-class
				make-object
			)
			(
				$def! (encapsulate-class class? decapsulate-class)
					() make-encapsulation-type;
				$defl! make-class (base ctor)
					encapsulate-class (list base ctor);
				$defl! ctor-of (c) first (rest (decapsulate-class c));
				$defl! base-of (c) first (decapsulate-class c);
				$defl! apply-ctor (c self args)
					apply (ctor-of c) (list* self args);
				$defl! make-object (c .args)
				(
					$def! self () make-environment;
					$def! base base-of c;
					$if (null? base) () (apply-ctor base self ());
					apply-ctor c self args;
					self
				)
			);
			() lock-current-environment
		);

		$def! UnilangQt $let ()
		(
			$def! UnilangQt.impl__ $provide!
			(
				QWidget
			)
			(
				$import! std.classes make-class;
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
		);	)Unilang");
}

} // namespace Unilang;

