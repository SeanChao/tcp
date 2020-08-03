#include "wrapping_integers.hh"

#include <iostream>

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) { return WrappingInt32{isn + n}; }

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    WrappingInt32 wrapped_checkpoint = wrap(checkpoint, isn);
    cout << wrapped_checkpoint << endl;
    if (n.raw_value() > wrapped_checkpoint.raw_value()) {
        uint32_t diff = n.raw_value() - wrapped_checkpoint.raw_value();
        if (diff > (1u << 16)) {
            uint64_t t = checkpoint + diff - (1ul << 32);
            return t < checkpoint ? t : checkpoint + diff;
        } else {
            uint64_t t = checkpoint + diff;
            return t >= checkpoint ? t : t - (1ul << 32);
        }
    } else {
        uint32_t diff = wrapped_checkpoint.raw_value() - n.raw_value();
        if (diff > (1u << 16)) {
            uint64_t t = checkpoint - diff + (1ul << 32);
            return t > checkpoint ? t : checkpoint - diff;
        } else {
            return checkpoint - diff <= checkpoint ? checkpoint - diff : checkpoint - diff + (1ul << 32);
        }
    }
}
