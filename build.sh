g++ -std=c++11 -Wall -Wextra -g -ounilang src/*.cpp \
	-I3rdparty/YSLib/YBase/include \
	3rdparty/YSLib/YBase/source/ystdex/any.cpp \
	3rdparty/YSLib/YBase/source/ystdex/cstdio.cpp \
	3rdparty/YSLib/YBase/source/ystdex/cassert.cpp \
	3rdparty/YSLib/YBase/source/ystdex/exception.cpp \
	3rdparty/YSLib/YBase/source/ystdex/memory_resource.cpp \
	3rdparty/YSLib/YBase/source/ystdex/node_base.cpp \
	3rdparty/YSLib/YBase/source/ystdex/tree.cpp

