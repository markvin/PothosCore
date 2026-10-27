#a directory of the parent without its own project() that finds the core
find_package(Pothos CONFIG REQUIRED)
if(NOT TARGET Pothos::Pothos)
    message(FATAL_ERROR "plain_b: no target Pothos::Pothos")
endif()
