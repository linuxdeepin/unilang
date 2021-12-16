// © 2020-2021 Uniontech Software Technology Co.,Ltd.

#if __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
#include "UnilangFFI.h" // for Unilang::allocate_shared, IsTyped;
#include YFM_YSLib_Adaptor_YAdaptor // for YSLib::unique_ptr_from;
#if __GNUC__
#	pragma GCC diagnostic pop
#endif
#if YCL_Win32
#	include YFM_Win32_YCLib_MinGW32 // for ::HMODULE;
#	include YFM_Win32_YCLib_NLS // for platform_ex::UTF8ToWCS;
#else
// XXX: Must use dlfcn. Add check?
#	include <dlfcn.h> // for ::dlerror, RTLD_LAZY, RTLD_GLOBAL, ::dlopen,
//	::dlclose, ::dlsym;
#	include YFM_YCLib_NativeAPI // for YCL_CallGlobal;
#endif
#include "Exception.h" // for UnilangException, TypeError, ListTypeError,
//	InvalidSyntax;
#include <ffi.h> // for ::ffi_type;

namespace Unilang
{

namespace
{

struct LibraryHandleDelete final
{
#if YCL_Win32
	using pointer = ::HMODULE;
#else
	using pointer = void*;
#endif

	void
	operator()(pointer h) const noexcept
	{
#if YCL_Win32
		if(h)
			YCL_TraceCallF_Win32(FreeModule, h);
#else
		if(h && YCL_CallGlobal(dlclose, h) != 0)
		{
			// TODO: Report error.
		}
#endif
	}
};

using UniqueLibraryHandle = YSLib::unique_ptr_from<LibraryHandleDelete>;

class DynamicLibrary final
{
public:
	using FPtr = void(*)();

	string Name;

private:
	UniqueLibraryHandle h_library;

public:
	YB_NONNULL(2)
	DynamicLibrary(const char*);

	YB_ATTR_nodiscard YB_ATTR_returns_nonnull FPtr
	LookupFunctionPtr(const string&) const;
};


DynamicLibrary::DynamicLibrary(const char* filename)
	: Name(filename),
#if YCL_Win32
	h_library(::LoadLibraryW(platform_ex::UTF8ToWCS(filename).c_str()))
{
	if(!h_library)
		throw UnilangException(ystdex::sfmt(
			"Failed loading dynamic library named '%s'.", filename));
}
#else
	h_library(::dlopen((yunused(::dlerror()), filename),
	RTLD_LAZY | RTLD_GLOBAL))
{
	if(const auto p_err = ::dlerror())
		throw UnilangException(ystdex::sfmt("Failed loading dynamic library"
			" named '%s' with error '%s'.", filename, p_err));
}
#endif

DynamicLibrary::FPtr
DynamicLibrary::LookupFunctionPtr(const string& fn) const
{
	if(const auto h = h_library.get())
	{
#if YCL_Win32
		if(const auto p_fn = ::GetProcAddress(h, fn.c_str()))
			return FPtr(p_fn);
		throw UnilangException(ystdex::sfmt("Failed looking up symbol '%s' in the"
			" library '%s'.", fn.c_str(), Name.c_str()));
#else
		yunused(::dlerror());

		const auto p_fn(::dlsym(h, fn.c_str()));
		if(const auto p_err = dlerror())
			throw UnilangException(ystdex::sfmt("Failed looking up symbol '%s'"
				" in the library '%s': %s.", fn.c_str(), Name.c_str(), p_err));
		if(p_fn)
		{
			FPtr v;
			static_assert(sizeof(v) == sizeof(p_fn), "Invalid platform found.");

			std::memcpy(&v, &p_fn, sizeof(void*));
			return v;
		}
		throw UnilangException(ystdex::sfmt("Null value of symbol '%s' found in"
			" the library '%s'.", fn.c_str(), Name.c_str()));
#endif
	}
	throw UnilangException("Invalid library handle found.");
}


struct FFICodec final
{
	::ffi_type libffi_type;

	ReductionStatus(*decode)(TermNode&, const void*);
	void(*encode)(const TermNode&, void*);
};


YB_ATTR_nodiscard YB_NONNULL(2) ReductionStatus
FFI_Decode_string(TermNode& term, const void* buf)
{
	if(const auto s = *static_cast<const char* const*>(buf))
		term.Value = string(s, term.get_allocator());
	term.Value = string(term.get_allocator());
	return ReductionStatus::Clean;
}

YB_NONNULL(2) void
FFI_Encode_string(const TermNode& term, void* buf)
{
	*static_cast<const void**>(buf)
		= &Unilang::ResolveRegular<const string>(term)[0];
}

YB_ATTR_nodiscard YB_NONNULL(2) ReductionStatus
FFI_Decode_void(TermNode& term, const void*)
{
	return ReduceReturnUnspecified(term);
}

YB_NONNULL(2) void
FFI_Encode_void(const TermNode& term, void*)
{
	if(!(Unilang::ResolveRegular<const ValueToken>(term)
		== ValueToken::Unspecified))
		throw TypeError("Only inert can be cast to C void.");
}

template<typename _type>
struct FFI_Codec_Direct final
{
	YB_ATTR_nodiscard YB_NONNULL(2) static ReductionStatus
	Decode(TermNode& term, const void* buf)
	{
		term.Value = *static_cast<const _type*>(buf);
		return ReductionStatus::Clean;
	}

	YB_NONNULL(2) static void
	Encode(const TermNode& term, void* buf)
	{
		*static_cast<_type*>(buf) = Unilang::ResolveRegular<const _type>(term);
	}
};

YB_ATTR_nodiscard YB_NONNULL(2) ReductionStatus
FFI_Decode_pointer(TermNode& term, const void* buf)
{
	if(const auto p = *static_cast<const void* const*>(buf))
		term.Value = p;
	else
		term.Value.Clear();
	return ReductionStatus::Clean;
}

YB_NONNULL(2) void
FFI_Encode_pointer(const TermNode& term, void* buf)
{
	Unilang::ResolveTerm([&](const TermNode& nd, bool has_ref){
		if(IsEmpty(nd))
			*static_cast<const void**>(buf) = {};
		else if(!IsList(nd))
		{
			if(const auto p = nd.Value.AccessPtr<const string>())
				*static_cast<const void**>(buf) = &(*p)[0];
			else
				throw TypeError(ystdex::sfmt("Unsupported type '%s' to encode"
					" pointer found.", nd.Value.type().name()));
		}
		else
			ThrowListTypeErrorForNonlist(nd, has_ref);
	}, term);
}


using codecs_type = map<string, FFICodec>;

codecs_type&
get_codecs_ref(codecs_type::allocator_type a)
{
	static map<string, FFICodec> m{{
		{"string", FFICodec{::ffi_type_pointer, FFI_Decode_string,
			FFI_Encode_string}},
	#define NPL_Impl_FFI_SType_(t) \
		{#t, FFICodec{::ffi_type_##t, FFI_Decode_##t, FFI_Encode_##t}}
	#define NPL_Impl_FFI_DType_(t, _tp) \
		{#t, FFICodec{::ffi_type_##t, FFI_Codec_Direct<_tp>::Decode, \
			FFI_Codec_Direct<_tp>::Encode}}
		NPL_Impl_FFI_SType_(void),
		NPL_Impl_FFI_DType_(sint, int),
		NPL_Impl_FFI_SType_(pointer),
		NPL_Impl_FFI_DType_(uint8, std::uint8_t),
		NPL_Impl_FFI_DType_(sint8, std::int8_t),
		NPL_Impl_FFI_DType_(uint16, std::uint16_t),
		NPL_Impl_FFI_DType_(sint16, std::int16_t),
		NPL_Impl_FFI_DType_(uint32, std::uint32_t),
		NPL_Impl_FFI_DType_(sint32, std::int32_t),
		NPL_Impl_FFI_DType_(uint64, std::uint64_t),
		NPL_Impl_FFI_DType_(float, float),
		NPL_Impl_FFI_DType_(double, double)
	#undef NPL_Impl_FFI_DType_
	#undef NPL_Impl_FFI_SType_
	}, a};

	return m;
}

FFICodec&
get_codec(const string& t)
{
	auto& codecs(get_codecs_ref(t.get_allocator()));
	const auto i(codecs.find(t));

	if(i != codecs.cend())
		return i->second;
	throw UnilangException(ystdex::sfmt("Unsupported FFI type '%s' found.",
		t.c_str()));
}

::ffi_abi
get_abi(string abi)
{
	if(abi == "FFI_DEFAULT_ABI")
		return FFI_DEFAULT_ABI;
	throw UnilangException(
		ystdex::sfmt("Unsupported FFI ABI '%s' found.", abi.c_str()));
}

size_t
align_offset(size_t offset, size_t alignment) noexcept
{
	assert(alignment > 0);
	return offset + (alignment - offset % alignment) % alignment;
}


class CallInterface final
{
private:
	size_t n_params;

public:
	vector<FFICodec> param_codecs;
	vector<::ffi_type*> param_types;
	FFICodec ret_codec;
	size_t buffer_size;
	::ffi_cif cif;

	CallInterface(const string& abi, const string& s_rtype,
		const vector<string>& s_ptypes)
		: n_params(s_ptypes.size()), param_codecs(s_ptypes.get_allocator()),
		param_types(s_ptypes.get_allocator()), ret_codec(get_codec(s_rtype)),
		buffer_size(ret_codec.libffi_type.size)
	{
		if(!ret_codec.decode)
			throw UnilangException(ystdex::sfmt("The type '%s' is not allowed"
				" as a return type.", s_rtype.c_str()));
		param_codecs.reserve(n_params),
		param_types.reserve(n_params);
		for(const auto& s_ptype : s_ptypes)
		{
			auto& codec(get_codec(s_ptype));

			param_codecs.push_back(codec);
			if(!codec.encode)
				throw UnilangException(ystdex::sfmt("The type '%s' is not"
					" allowed in the parameter list.", s_ptype.c_str()));

			auto& t(codec.libffi_type);

			param_types.push_back(&t);
			buffer_size = align_offset(buffer_size, t.alignment) + t.size;
		}
		switch(::ffi_prep_cif(&cif, get_abi(abi), n_params,
			&ret_codec.libffi_type, param_types.data()))
		{
		case FFI_OK:
			break;
		case FFI_BAD_ABI:
			throw UnilangException("FFI_BAD_ABI");
		case FFI_BAD_TYPEDEF:
			throw UnilangException("FFI_BAD_TYPEDEF");
		default:
			throw UnilangException("Unknown error in '::ffi_prep_cif' found.");
		}
	}

	DefGetter(const noexcept, size_t, BufferSize, buffer_size)
	DefGetter(const noexcept, size_t, ParameterCount, n_params)
};

CallInterface&
EnsureValidCIF(const shared_ptr<CallInterface>& p_cif)
{
	if(p_cif)
		return *p_cif;
	// NOTE: This should normall not happen.
	throw std::invalid_argument("Invalid call interface record pointer found.");
}


struct FFIClosureDelete final
{
	void
	operator()(void* h) const noexcept
	{
		::ffi_closure_free(h);
	}
};


class Callback;

struct FFICallback final
{
	::ffi_closure Closure;
	Context* ContextPtr;
	const ContextHandler* HandlerPtr;
	Callback* InfoPtr;
};

static_assert(std::is_standard_layout<FFICallback>(),
	"Invalid FFI closure found.");

void
FFICallbackEntry(::ffi_cif*, void*, void**, void*);

class Callback final
{
private:
	shared_ptr<CallInterface> cif_ptr;
	void* code;
	YSLib::unique_ptr<FFICallback, FFIClosureDelete> cb_ptr;

public:
	Callback(Context& ctx, const ContextHandler& h,
		const shared_ptr<CallInterface>& p_cif)
		: cif_ptr(p_cif), cb_ptr(static_cast<FFICallback*>(
		::ffi_closure_alloc(sizeof(FFICallback), &code)))
	{
		if(cb_ptr)
		{
			auto& cif(EnsureValidCIF(p_cif));
			yunseq(cb_ptr->ContextPtr = &ctx, cb_ptr->HandlerPtr = &h,
				cb_ptr->InfoPtr = this);

			const auto status(::ffi_prep_closure_loc(&cb_ptr->Closure, &cif.cif,
				FFICallbackEntry, cb_ptr.get(), code));

			if(status != ::FFI_OK)
				throw UnilangException(ystdex::sfmt("Unkown error '%zu' in"
					" 'ffi_prep_closure_loc' found.", size_t(status)));
		}
		else
			throw std::bad_alloc();
	}

	YB_ATTR_nodiscard YB_PURE const CallInterface&
	GetCallInterface() const noexcept
	{
		return *cif_ptr;
	}
};


void
FFICallbackEntry(::ffi_cif*, void* ret, void** args, void* user_data)
{
	const auto& cb(*static_cast<FFICallback*>(user_data));
	auto& ctx(*cb.ContextPtr);
	const auto a(ctx.get_allocator());
	auto p_term(Unilang::allocate_shared<TermNode>(a));
	auto& term(*p_term);

	ctx.SetupFront([&, args, ret, p_term](Context& c){
		const auto& h(*cb.HandlerPtr);
		auto& cif(cb.InfoPtr->GetCallInterface());
		const auto n_params(cif.GetParameterCount());

		for(size_t idx(0); idx < n_params; ++idx)
		{
			TermNode tm(a);

			cif.param_codecs[idx].decode(tm, args[idx]);
			term.GetContainerRef().emplace_back(std::move(tm));
		}
		c.SetupFront([&, ret, p_term]{
			cif.ret_codec.encode(term, ret);
			return ReductionStatus::Partial;
		});
		return h(term, c);
	});
	EnvironmentGuard gd(ctx, Unilang::SwitchToFreshEnvironment(ctx));

	ctx.RewriteTerm(term);
}

} // unnamed namespace;


void
InitializeFFI(Interpreter& intp)
{
	auto& ctx(intp.Root);
	using namespace Forms;

	RegisterUnary<>(ctx, "ffi-library?",
		ComposeReferencedTermOp([](const TermNode& term) noexcept{
		return IsTyped<DynamicLibrary>(term);
	}));
	RegisterUnary<Strict, const string>(ctx, "ffi-load-library",
		[](const string& filename){
		return DynamicLibrary(filename.c_str());
	});
	RegisterUnary<>(ctx, "ffi-call-interface?",
		ComposeReferencedTermOp([](const TermNode& term) noexcept{
		return IsTyped<shared_ptr<CallInterface>>(term);
	}));
	RegisterStrict(ctx, "ffi-make-call-interface", [](TermNode& term){
		RetainN(term, 3);

		auto i(std::next(term.begin()));
		const auto& abi(Unilang::ResolveRegular<const string>(*i));
		const auto&
			s_ret_type(Unilang::ResolveRegular<const string>(*++i));
		const auto& s_param_types_term(*++i);

		if(IsList(s_param_types_term))
		{
			vector<string> s_param_types(term.get_allocator());

			s_param_types.reserve(s_param_types_term.size());
			term.Value = std::allocate_shared<CallInterface>(
				term.get_allocator(), abi, s_ret_type, s_param_types);
			return ReductionStatus::Clean;
		}
		throw ListTypeError("Expected a list for the 3rd parameter.");
	});
	RegisterStrict(ctx, "ffi-make-applicative", [](TermNode& term){
		RetainN(term, 3);

		auto i(std::next(term.begin()));
		const auto&
			lib(Unilang::ResolveRegular<const DynamicLibrary>(*i));
		const auto& fn(Unilang::ResolveRegular<const string>(*++i));
		auto& cif(EnsureValidCIF(Unilang::ResolveRegular<const shared_ptr<
			CallInterface>>(*++i)));
		const auto p_fn(lib.LookupFunctionPtr(fn));

		term.Value = ContextHandler(FormContextHandler([&, p_fn](TermNode& tm){
			if(IsBranch(tm))
			{
				const auto n_params(cif.GetParameterCount());

				RetainN(tm, n_params);

				const auto buffer_size(cif.GetBufferSize());
				const auto p_buf(
					ystdex::make_unique_default_init<std::int64_t[]>(
					(buffer_size + sizeof(std::int64_t) - 1)
					/ sizeof(std::int64_t)));
				const auto p_param_ptrs(
					ystdex::make_unique_default_init<void*[]>(n_params));
				void* rptr(
					ystdex::aligned_store_cast<unsigned char*>(p_buf.get()));
				size_t offset(cif.ret_codec.libffi_type.size);

				for(size_t idx(0); idx < n_params; ++idx)
				{
					auto& codec(cif.param_codecs[idx]);
					const auto& t(codec.libffi_type);

					offset = align_offset(offset, t.alignment);
					p_param_ptrs[idx] = ystdex::aligned_store_cast<
						unsigned char*>(p_buf.get()) + offset;
					cif.param_codecs[idx].encode(tm, p_param_ptrs[idx]);
					offset += t.size;
				}
				assert(offset == buffer_size);
				::ffi_call(&cif.cif, p_fn, rptr, p_param_ptrs.get());
				return cif.ret_codec.decode(tm, rptr);
			}
			else
				throw InvalidSyntax("Invalid function application found.");
		}, 1));
		return ReductionStatus::Clean;
	});
	RegisterStrict(ctx, "ffi-make-callback", [](TermNode& term, Context& c){
		auto i(std::next(term.begin()));
		const auto& h(Unilang::ResolveRegular<const ContextHandler>(*i));

		term.Value = YSLib::allocate_shared<Callback>(term.get_allocator(), c,
			h, Unilang::ResolveRegular<const shared_ptr<CallInterface>>(
			*++i));
		return ReductionStatus::Clean;
	});

}

} // namespace Unilang;

