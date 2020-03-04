@0xf462993712015378;

# Count-min sketch
# See: https://en.wikipedia.org/wiki/Count%E2%80%93min_sketch

struct CMS {
    name                     @0: Text;          # = "My little sketch";

    usageType                @1: UsageType;     # See definition below.  What type of computation is this sketch used for?

    delta                    @2: Float64;       # f_i is the true frequency with which i occurs in the stream.
    epsilon                  @3: Float64;       # Probability is:  1 - delta  that  f_i_estimated <= f_i + epsilon * N,  where  N = sum{i=0,n}(f_i)
    d                        @4: UInt64;        # Derived from delta:   d = int(np.ceil(np.log(1 / delta)))
    w                        @5: UInt64;        # Derived from epsilon: w = int(np.ceil(np.exp(1) / epsilon))
    pointV                   @6: Int64;         # usageType == point:  Value to compute frequency for
    topkK                    @7: UInt64;        # usageType == topk:   Compute the top K most frequent values
    heavyF                   @8: Float64;       # usageType == heavy:  Heavy hitters mininum frequency

    prime                    @9: UInt64;        # Hashing parameter, a prime number, for example 2^31-1

    topkN                   @10: UInt64;        # Running number of how many top-k values we have stored in the arrays so far (topkN <= topkK)
    rowsN                   @11: UInt64;        # Running number of values accumulated so far

    ##
    ## CMS data structures:
    ##
    a                       @12: List(UInt64);  # Array of length D of hash values used by function hash()
    b                       @13: List(UInt64);  # (same as array a)
    estimators              @14: List(UInt64);  # 2-dimensional D*W array, we will to the arithmetic

    ##
    ## Data structures used only by top-k and heavy-hitters queries:
    ##
    topkValues              @15: List(TopkValue);
    topkFrequencies         @16: List(TopkFrequency);

    topkValuesPrevious      @17: List(TopkValue);
    topkFrequenciesPrevious @18: List(TopkFrequency);
}

enum UsageType {
    point @0; # Point query
    topk  @1; # Top-k computation
    heavy @2; # Heavy-hitters
}

struct TopkValue {
    rank      @0 : Int16; # rank = i if this value is the top-i highest value.  If values are {7, 3, 10, 4}
    value     @1 : Int64;
    frequency @2 : Int64;
    payload   @3 : Text;
}

struct TopkFrequency {
    rank      @0 : Int16;
    value     @1 : Int64;
    frequency @2 : Int64;
    payload   @3 : Text;
}

struct Scratchpad {
    topkFrequencies     @0: List(TopkFrequency);
    topkValues          @1: List(TopkValue);
    topkValuesSortIndex @2: List(UInt64);
}
