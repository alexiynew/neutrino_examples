set(SOURCES
    src/main.cpp
    src/view.hpp
    src/view.cpp
    src/event_handler.hpp
    src/event_handler.cpp
    src/data_context.hpp
    src/data_context.cpp
)

configure_file(${EXAMPLE_SOURCE_DIR}/data/UbuntuMono-Regular.ttf ${EXAMPLE_BINARY_DIR}/data/UbuntuMono-Regular.ttf COPYONLY)
