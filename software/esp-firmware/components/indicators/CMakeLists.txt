
set(srcs "src/indicators.c")
set(include_dirs "include")
set(requires common app_errors animations)

if (CONFIG_MOCK_INDICATORS)
    set(srcs "mocks/Mockindicators.c")
    list(APPEND include_dirs "mocks")
    # set(requires common app_errors animations cmock)
    # list(APPEND requires cmock)
endif ()

idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS ${include_dirs}
                    REQUIRES common app_errors animations cmock)