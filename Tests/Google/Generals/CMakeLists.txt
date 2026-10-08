# g_googletest compiles these sources together with the Core tests.
set(GI_GOOGLETEST_SRC
    GameEngine/GameLogic/ScriptEngineTest.cpp
)

add_library(gi_googletest INTERFACE)

target_sources(gi_googletest INTERFACE ${GI_GOOGLETEST_SRC})
