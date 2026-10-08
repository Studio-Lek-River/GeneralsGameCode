# z_googletest compiles these sources together with the Core tests.
set(ZI_GOOGLETEST_SRC
    GameEngine/GameLogic/ScriptEngineTest.cpp
)

add_library(zi_googletest INTERFACE)

target_sources(zi_googletest INTERFACE ${ZI_GOOGLETEST_SRC})
