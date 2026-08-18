add_executable(test_common
    test_common.c
    test_crc32.c
    test_ring_buffer.c)
target_link_libraries(test_common PRIVATE devmgr_common)
add_test(NAME unit.common COMMAND test_common)

