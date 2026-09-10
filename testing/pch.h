// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

// The unit tests reuse the emulator precompiled header verbatim.
// The emulator sources are pulled into this project as links (see pureikyubu_test.vcxproj),
// so they must see exactly the same environment as in the main emulator project.

#include "../src/pch.h"

#include "CppUnitTest.h"

// All test translation units use the Microsoft CppUnitTest assertions.
using namespace Microsoft::VisualStudio::CppUnitTestFramework;

#endif //PCH_H
