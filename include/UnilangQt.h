// SPDX-FileCopyrightText: 2020-2021 UnionTech Software Technology Co.,Ltd.

#ifndef INC_Unilang_UnilangQt_h_
#define INC_Unilang_UnilangQt_h_ 1

#include "Interpreter.h" // for Interpreter;

namespace Unilang
{

// NOTE: This function shall be called to use Qt bindings. The 'argc' argument
//	is explicitly kept of 'int' type in the caller.
void
InitializeQt(Interpreter&, int&, char*[]);

} // namespace Unilang;

#endif

