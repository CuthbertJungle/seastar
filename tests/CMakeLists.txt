add_library (testing
  test-utils.cc
  test_runner.cc)

target_link_libraries (testing PUBLIC
  Boost::unit_test_framework
  seastar)

if (NOT ${SEASTAR_LINK_STATIC_BOOST})
  target_compile_definitions (testing PUBLIC
    BOOST_TEST_DYN_LINK)
endif ()

function (add_seastar_test)
  set (options "")
  set (one_value_args NAME)
  set (multi_value_args SOURCES)
  cmake_parse_arguments (args "${options}" "${one_value_args}" "${multi_value_args}" "${ARGN}")

  add_executable (${args_NAME} "${args_SOURCES}")

  target_link_libraries (${args_NAME} PUBLIC testing)

  add_test (NAME ${args_NAME} COMMAND "${CMAKE_CURRENT_BINARY_DIR}/${args_NAME}")
endfunction ()

add_seastar_test (NAME alloc_test SOURCES alloc_test.cc)
add_seastar_test (NAME allocator_test SOURCES allocator_test.cc)
add_seastar_test (NAME blkdiscard_test SOURCES blkdiscard_test.cc)
add_seastar_test (NAME checked_ptr_test SOURCES checked_ptr_test.cc)
add_seastar_test (NAME chunked_fifo_test SOURCES chunked_fifo_test.cc)
add_seastar_test (NAME circular_buffer_test SOURCES circular_buffer_test.cc)
add_seastar_test (NAME connect_test SOURCES connect_test.cc)
add_seastar_test (NAME defer_test SOURCES defer_test.cc)
add_seastar_test (NAME directory_test SOURCES directory_test.cc)
add_seastar_test (NAME distributed_test SOURCES distributed_test.cc)
add_seastar_test (NAME dns_test SOURCES dns_test.cc)
add_seastar_test (NAME echo_test SOURCES echotest.cc)
add_seastar_test (NAME execution_stage_test SOURCES execution_stage_test.cc)
add_seastar_test (NAME expiring_fifo_test SOURCES expiring_fifo_test.cc)
add_seastar_test (NAME fair_queue-test SOURCES fair_queue_test.cc)
add_seastar_test (NAME file_io_test SOURCES fileiotest.cc)
add_seastar_test (NAME foreign_ptr_test SOURCES foreign_ptr_test.cc)
add_seastar_test (NAME fstream_test SOURCES fstream_test.cc)
add_seastar_test (NAME futures_test SOURCES futures_test.cc)
add_seastar_test (NAME ip_test SOURCES ip_test.cc)
add_seastar_test (NAME json_formatter_test SOURCES json_formatter_test.cc)
add_seastar_test (NAME l3_test SOURCES l3_test.cc)
add_seastar_test (NAME lowres_clock_test SOURCES lowres_clock_test.cc)
add_seastar_test (NAME output_stream_test SOURCES output_stream_test.cc)
add_seastar_test (NAME packet_test SOURCES packet_test.cc)
add_seastar_test (NAME rpc_test SOURCES rpc_test.cc)
add_seastar_test (NAME semaphore_test SOURCES semaphore_test.cc)
add_seastar_test (NAME shared_ptr_test SOURCES shared_ptr_test.cc)
add_seastar_test (NAME slab_test SOURCES slab_test.cc)
add_seastar_test (NAME smp_test SOURCES smp_test.cc)
add_seastar_test (NAME sstring_test SOURCES sstring_test.cc)
add_seastar_test (NAME tcp_test SOURCES tcp_test.cc)
add_seastar_test (NAME thread_test SOURCES thread_test.cc)
add_seastar_test (NAME timer_test SOURCES timertest.cc)
add_seastar_test (NAME tls_test SOURCES tls_test.cc)
add_seastar_test (NAME unwind_test SOURCES unwind_test.cc)
add_seastar_test (NAME weak_ptr_test SOURCES weak_ptr_test.cc)
