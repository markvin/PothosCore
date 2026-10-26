########################################################################
# Ratchet check: Poco may only be used by files listed in an allowlist
# (cmake -P script). Poco is being removed in phases; every phase shrinks
# the list, and a listed file that no longer uses Poco is an error too,
# so the list can only get shorter.
#
# Required: -DSRC=<source root> -DALLOWLIST=<allowlist file>
#
# A file "uses Poco" when it includes <Poco/...> or mentions Poco::,
# POCO_ or poco_. Public headers under include/ may never use Poco.
########################################################################
foreach(var SRC ALLOWLIST)
    if (NOT DEFINED ${var})
        message(FATAL_ERROR "PothosCheckPoco: ${var} is not set")
    endif ()
endforeach()

#paths are reported relative to an absolute source root
get_filename_component(SRC "${SRC}" ABSOLUTE)

#read the allowlist: one path per line, # comments and blank lines ignored
file(STRINGS "${ALLOWLIST}" allow_lines)
set(allowed)
foreach(line ${allow_lines})
    string(STRIP "${line}" line)
    if (line STREQUAL "" OR line MATCHES "^#")
        continue()
    endif ()
    list(APPEND allowed "${line}")
endforeach()

#scan the sources
set(using)
foreach(dir include lib apps)
    file(GLOB_RECURSE files RELATIVE "${SRC}" "${SRC}/${dir}/*.cpp" "${SRC}/${dir}/*.hpp")
    foreach(f ${files})
        file(READ "${SRC}/${f}" text)
        if (text MATCHES "#include[ \t]*[<\"]Poco/|Poco::|POCO_|poco_")
            list(APPEND using "${f}")
        endif ()
    endforeach()
endforeach()

set(errors)
foreach(f ${using})
    if (f MATCHES "^include/")
        list(APPEND errors "public header uses Poco: ${f}")
    elseif (NOT f IN_LIST allowed)
        list(APPEND errors "uses Poco but is not in the allowlist: ${f}")
    endif ()
endforeach()
foreach(f ${allowed})
    if (NOT f IN_LIST using)
        list(APPEND errors "in the allowlist but does not use Poco (remove it): ${f}")
    endif ()
endforeach()

if (errors)
    list(LENGTH errors n)
    list(JOIN errors "\n  " text)
    message(FATAL_ERROR "Poco allowlist check failed (${n}):\n  ${text}")
endif ()
list(LENGTH using n)
message(STATUS "Poco allowlist check: ${n} files use Poco, all allowed")
