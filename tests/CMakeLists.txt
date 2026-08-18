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

add_executable(test_ipc test_ipc.c)
target_link_libraries(test_ipc PRIVATE devmgr_ipc)
add_test(NAME unit.ipc COMMAND test_ipc)

add_executable(test_upgrade test_upgrade.c)
target_link_libraries(test_upgrade PRIVATE devmgr_upgrade)
add_test(NAME unit.upgrade COMMAND test_upgrade)

add_executable(test_parser_stress test_parser_stress.c)
target_link_libraries(test_parser_stress PRIVATE devmgr_protocol)
add_test(NAME stress.parser COMMAND test_parser_stress)
set_tests_properties(stress.parser PROPERTIES LABELS "stress;fuzz-smoke" TIMEOUT 30)


if(UNIX AND NOT APPLE)
    add_executable(test_transport test_transport.c)
    target_compile_definitions(test_transport PRIVATE _POSIX_C_SOURCE=200809L)
    target_link_libraries(test_transport PRIVATE devmgr_transport)
    add_test(NAME unit.transport COMMAND test_transport)

    add_executable(test_worker test_worker.c)
    target_link_libraries(test_worker PRIVATE devmgr_worker)
    add_test(NAME unit.worker COMMAND test_worker)

    add_test(NAME integration.demo
             COMMAND ${CMAKE_COMMAND} -E env DEVMGR_BINARY_DIR=$<TARGET_FILE_DIR:devmgrd>
                     bash ${PROJECT_SOURCE_DIR}/scripts/run-demo.sh)
    set_tests_properties(integration.demo PROPERTIES LABELS integration TIMEOUT 45)

    add_test(NAME integration.fault-recovery
             COMMAND ${CMAKE_COMMAND} -E env DEVMGR_BINARY_DIR=$<TARGET_FILE_DIR:devmgrd>
                     bash ${PROJECT_SOURCE_DIR}/scripts/demo-fault-recovery.sh)
    set_tests_properties(integration.fault-recovery PROPERTIES LABELS integration TIMEOUT 60)
endif()
