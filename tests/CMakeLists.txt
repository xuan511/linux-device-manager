add_executable(test_common
    test_common.c
    test_crc32.c
    test_ring_buffer.c)
target_link_libraries(test_common PRIVATE devmgr_common)
add_test(NAME unit.common COMMAND test_common)

add_executable(test_protocol test_protocol.c)
target_link_libraries(test_protocol PRIVATE devmgr_protocol)
add_test(NAME unit.protocol COMMAND test_protocol)

add_executable(test_session test_session.c)
target_link_libraries(test_session PRIVATE devmgr_device)
add_test(NAME unit.session COMMAND test_session)

if(UNIX AND NOT APPLE)
    add_executable(test_transport test_transport.c)
    target_compile_definitions(test_transport PRIVATE _POSIX_C_SOURCE=200809L)
    target_link_libraries(test_transport PRIVATE devmgr_transport)
    add_test(NAME unit.transport COMMAND test_transport)
endif()
