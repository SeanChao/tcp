find_library (LIBPCAP pcap)
find_library (LIBPTHREAD pthread)
find_library (LIBSPDLOG spdlog)

macro (add_sponge_exec exec_name)
    add_executable ("${exec_name}" "${exec_name}.cc")
    target_link_libraries ("${exec_name}" ${ARGN} sponge ${LIBPTHREAD})
endmacro (add_sponge_exec)

macro (link_log exec_name)
    target_link_libraries ("${exec_name}" ${LIBSPDLOG})
endmacro (link_log)
