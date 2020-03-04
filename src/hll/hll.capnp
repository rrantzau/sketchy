@0x91ca1e5998a2e8a0;

# HyperLogLog sketch
# See: https://en.wikipedia.org/wiki/HyperLogLog

struct HLL {
    name    @0: Text;         # = "My little sketch";
    b       @1: UInt8;        # Register bit width
    alpha   @2: Float64;      # Alpha
    alphaMM @3: Float64;      # alpha * m^2
    rowsN   @4: UInt64;       # Running number of values accumulated so far
    mLength @5: UInt64;       # Register size
    m       @6: List(UInt64); # Array of counters of length m
}