# protobuf_wire fuzz corpus

Seed inputs for `protobuf_wire_fuzzer`. Each file is a raw byte sequence
interpreted as a stream of protobuf wire-format fields (tag + value) by the
decode primitives in
`include/kcenon/monitoring/exporters/internal/protobuf_wire.h`.

The seeds intentionally cover:
- a length-delimited string field (the common Jaeger/Zipkin span field shape),
- a varint field,
- a fixed64 field (Zipkin timestamps),
- truncated / malformed fields that must be rejected without UB.

libFuzzer will mutate these to explore the decoder. New interesting inputs found
during fuzzing should be minimized and committed back here to grow coverage.
