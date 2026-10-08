/*
**	Command & Conquer Generals(tm)
**	Copyright 2026 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <gtest/gtest.h>
#include <cstdio>
#include <memory>
#include <string>

#include "Common/Debug.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameLogic/ScriptEngine.h"

namespace
{

// Exposes the protected storage of the script engine to the tests.
class ScriptEngineTestAccess : public ScriptEngine
{
public:
	using ScriptEngine::allocateCounter;
	using ScriptEngine::allocateFlag;
	using ScriptEngine::findAttackInfo;

	TCounter &counter(Int index) { return m_counters[index]; }
	TFlag &flag(Int index) { return m_flags[index]; }
	Int numCounters() const { return m_numCounters; }
	Int numFlags() const { return m_numFlags; }
	Int numAttackInfos() const { return m_numAttackInfo; }
};

std::unique_ptr<ScriptEngineTestAccess> makeEngine()
{
	std::unique_ptr<ScriptEngineTestAccess> engine(new ScriptEngineTestAccess);
	engine->reset();
	return engine;
}

AsciiString makeName(const char *prefix, Int index)
{
	AsciiString name;
	name.format("%s%d", prefix, index);
	return name;
}

Int s_debugCrashCount = 0;

void countDebugCrash(const char *)
{
	++s_debugCrashCount;
}

void failTestOnDebugCrash(const char *message)
{
	ADD_FAILURE() << message;
}

// Counts the debug crashes of an expected overflow instead of failing the test.
class ScopedExpectedDebugCrash
{
public:
	ScopedExpectedDebugCrash() { s_debugCrashCount = 0; DebugSetCrashHandler(countDebugCrash); }
	~ScopedExpectedDebugCrash() { DebugSetCrashHandler(failTestOnDebugCrash); }
};

void expectDebugCrashCount(Int expected)
{
#ifdef DEBUG_CRASHING
	EXPECT_EQ(s_debugCrashCount, expected);
#else
	(void)expected;
	EXPECT_EQ(s_debugCrashCount, 0);
#endif
}

void fillEngine(ScriptEngineTestAccess &engine, Int count)
{
	for (Int i = 1; i <= count; ++i)
	{
		const Int counterIndex = engine.allocateCounter(makeName("counter", i));
		ASSERT_EQ(counterIndex, i);
		engine.counter(counterIndex).value = i * 3;
		engine.counter(counterIndex).isCountdownTimer = (i % 2) == 0;

		const Int flagIndex = engine.allocateFlag(makeName("flag", i));
		ASSERT_EQ(flagIndex, i);
		engine.flag(flagIndex).value = (i % 3) == 0;

		AttackPriorityInfo *info = engine.findAttackInfo(makeName("priority", i), TRUE);
		ASSERT_NE(info, nullptr);
		info->friend_setDefaultPriority(i + 7);
	}
}

void expectFilledEngine(ScriptEngineTestAccess &engine, Int count)
{
	ASSERT_EQ(engine.numCounters(), count + 1);
	ASSERT_EQ(engine.numFlags(), count + 1);
	ASSERT_EQ(engine.numAttackInfos(), count + 1);
	for (Int i = 1; i <= count; ++i)
	{
		EXPECT_EQ(engine.counter(i).name, makeName("counter", i));
		EXPECT_EQ(engine.counter(i).value, i * 3);
		EXPECT_EQ(engine.counter(i).isCountdownTimer, (i % 2) == 0);

		EXPECT_EQ(engine.flag(i).name, makeName("flag", i));
		EXPECT_EQ(engine.flag(i).value, (i % 3) == 0);

		const AttackPriorityInfo *info = engine.findAttackInfo(makeName("priority", i), FALSE);
		ASSERT_NE(info, nullptr);
		EXPECT_EQ(info->getName(), makeName("priority", i));
		EXPECT_EQ(info->getPriority(nullptr), i + 7);
	}
}

void expectXferRoundTrip(Int count)
{
	const std::string fileName = ::testing::TempDir() + "ScriptEngineTest.sav";

	std::unique_ptr<ScriptEngineTestAccess> saved = makeEngine();
	fillEngine(*saved, count);
	{
		XferSave xfer;
		xfer.open(fileName.c_str());
		xfer.xferSnapshot(saved.get());
		xfer.close();
	}

	std::unique_ptr<ScriptEngineTestAccess> loaded = makeEngine();
	{
		// The game state that runs the load post processing does not exist here.
		XferLoad xfer;
		xfer.setOptions(XO_NO_POST_PROCESSING);
		xfer.open(fileName.c_str());
		xfer.xferSnapshot(loaded.get());
		xfer.close();
	}
	std::remove(fileName.c_str());

	expectFilledEngine(*loaded, count);

	// Names allocated after loading continue after the loaded ones.
	EXPECT_EQ(loaded->allocateCounter(makeName("counter", 1)), 1);
	EXPECT_EQ(loaded->allocateFlag(makeName("flag", count)), count);
	EXPECT_EQ(loaded->allocateCounter("counterAfterLoad"), count + 1);
	EXPECT_EQ(loaded->allocateFlag("flagAfterLoad"), count + 1);
}

} // namespace

TEST(ScriptEngine, SameNameReturnsSameIndex)
{
	std::unique_ptr<ScriptEngineTestAccess> engine = makeEngine();
	const Int counterIndex = engine->allocateCounter("counter");
	const Int flagIndex = engine->allocateFlag("flag");
	AttackPriorityInfo *info = engine->findAttackInfo("priority", TRUE);

	EXPECT_EQ(counterIndex, 1);
	EXPECT_EQ(flagIndex, 1);
	EXPECT_EQ(engine->allocateCounter("counter"), counterIndex);
	EXPECT_EQ(engine->allocateFlag("flag"), flagIndex);
	EXPECT_EQ(engine->findAttackInfo("priority", TRUE), info);
	EXPECT_EQ(engine->findAttackInfo("unknown", FALSE), nullptr);
}

#if RETAIL_COMPATIBLE_CRC

TEST(ScriptEngine, RetailCountersStopAtLimit)
{
	std::unique_ptr<ScriptEngineTestAccess> engine = makeEngine();
	for (Int i = 1; i < ScriptEngine::MAX_COUNTERS; ++i)
	{
		ASSERT_EQ(engine->allocateCounter(makeName("counter", i)), i);
	}

	ScopedExpectedDebugCrash expectedCrash;
	EXPECT_EQ(engine->allocateCounter("overflow"), 0);
	expectDebugCrashCount(1);
	EXPECT_EQ(engine->numCounters(), ScriptEngine::MAX_COUNTERS);
}

TEST(ScriptEngine, RetailFlagsStopAtLimit)
{
	std::unique_ptr<ScriptEngineTestAccess> engine = makeEngine();
	for (Int i = 1; i < ScriptEngine::MAX_FLAGS; ++i)
	{
		ASSERT_EQ(engine->allocateFlag(makeName("flag", i)), i);
	}

	ScopedExpectedDebugCrash expectedCrash;
	EXPECT_EQ(engine->allocateFlag("overflow"), 0);
	expectDebugCrashCount(1);
	EXPECT_EQ(engine->numFlags(), ScriptEngine::MAX_FLAGS);
}

TEST(ScriptEngine, RetailAttackInfosStopAtLimit)
{
	std::unique_ptr<ScriptEngineTestAccess> engine = makeEngine();
	for (Int i = 1; i < ScriptEngine::MAX_ATTACK_PRIORITIES; ++i)
	{
		ASSERT_NE(engine->findAttackInfo(makeName("priority", i), TRUE), nullptr);
	}

	EXPECT_EQ(engine->findAttackInfo("overflow", TRUE), nullptr);
	EXPECT_EQ(engine->numAttackInfos(), ScriptEngine::MAX_ATTACK_PRIORITIES);
}

TEST(ScriptEngine, RetailXferRoundTrip)
{
	expectXferRoundTrip(200);
}

#else

TEST(ScriptEngine, CountersFlagsAndAttackInfosHaveNoLimit)
{
	const Int count = 320;
	std::unique_ptr<ScriptEngineTestAccess> engine = makeEngine();
	fillEngine(*engine, count);
	expectFilledEngine(*engine, count);

	EXPECT_EQ(engine->allocateCounter(makeName("counter", count)), count);
	EXPECT_EQ(engine->allocateFlag(makeName("flag", 1)), 1);
}

TEST(ScriptEngine, AttackInfosDoNotMoveWhenAdded)
{
	std::unique_ptr<ScriptEngineTestAccess> engine = makeEngine();
	AttackPriorityInfo *first = engine->findAttackInfo("first", TRUE);
	ASSERT_NE(first, nullptr);
	first->friend_setDefaultPriority(42);

	for (Int i = 1; i <= 400; ++i)
	{
		ASSERT_NE(engine->findAttackInfo(makeName("priority", i), TRUE), nullptr);
	}

	EXPECT_EQ(engine->findAttackInfo("first", FALSE), first);
	EXPECT_EQ(first->getName(), AsciiString("first"));
	EXPECT_EQ(first->getPriority(nullptr), 42);
}

TEST(ScriptEngine, ResetRemovesAllocatedEntries)
{
	std::unique_ptr<ScriptEngineTestAccess> engine = makeEngine();
	fillEngine(*engine, 300);
	engine->reset();

	EXPECT_EQ(engine->numCounters(), 1);
	EXPECT_EQ(engine->numFlags(), 1);
	EXPECT_EQ(engine->numAttackInfos(), 1);
	EXPECT_EQ(engine->findAttackInfo(makeName("priority", 1), FALSE), nullptr);
	EXPECT_EQ(engine->allocateCounter("afterReset"), 1);
	EXPECT_EQ(engine->allocateFlag("afterReset"), 1);
}

TEST(ScriptEngine, XferRoundTrip)
{
	expectXferRoundTrip(320);
}

#endif
