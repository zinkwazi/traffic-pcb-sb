
set(srcs "src/indicators.c")
set(include_dirs "include")
set(requires common app_errors animations)

if (CONFIG_MOCK_INDICATORS)
    set(srcs "mock/src/mock_indicators.c")
    list(APPEND include_dirs "mock/include")
endif ()

idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS ${include_dirs}
                    REQUIRES ${requires})