protobuf_generate_cpp (SEASTAR_PROTO_SOURCES SEASTAR_PROTO_HEADERS
  metrics2.proto)

add_custom_target (seastar-protobuf
  SOURCES
    ${SEASTAR_PROTO_SOURCES}
    ${SEASTAR_PROTO_HEADERS})

add_dependencies (seastar seastar-protobuf)
