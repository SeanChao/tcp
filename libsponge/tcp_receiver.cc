#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

bool TCPReceiver::segment_received(const TCPSegment &seg) {
    // Set the Initial Sequence Number if necessary.
    // The sequence number of the first arriving segment that has the SYN flag set is the initial sequence number.
    // You’ll want to keep track of that in order to keep converting between 32-bit wrapped seqnos/acknos and their
    // absolute equivalents. (Note that the SYN flag is just one flag in the header. The same segment could also carry
    // data and could even have the FIN flag set.)
    bool syn = seg.header().syn;
    WrappingInt32 seqno = seg.header().seqno;
    if (syn) {
        // duplicate SYN
        if (this->__isn) {
            return false;
        }
        this->__isn = seqno;
        _lower = __isn.value();
    }
    if (!this->__isn) {
        return false;
    }
    if(seg.header().fin && this->_fin) {
        return false;
    }
    // Determine if any part of the segment falls inside the window. This method
    // should return true if any part of the segment fell inside the window, and false otherwise.2 Here’s what we mean
    // by that: A segment occupies a range of sequence numbers—a range starting with its sequence number, and with
    // length equal to its length in sequence space() (which reflects the fact that SYN and FIN each count for one
    // sequence number, as well as each byte of the payload). A segment is acceptable (and the method should return
    // true) if any of the sequence numbers it occupies falls inside the receiver’s window.
    optional<WrappingInt32> lower = ackno();
    uint64_t left = unwrap(lower.value(), this->__isn.value(), this->checkpoint);
    uint64_t right = left + window_size() - 1;
    uint64_t seg_left = unwrap(seqno, this->__isn.value(), this->checkpoint);
    uint64_t seg_right = seg_left + seg.length_in_sequence_space() - 1;
    if (seg_right >= left && seg_left <= right) {
        // Push any data, or end-of-stream marker, to the StreamReassembler.
        // If the FIN flag is set in a TCPSegment’s header, that means that the stream ends with the last
        // byte of the payload—it’s equivalent to an EOF.
        size_t index = unwrap(seqno, __isn.value(), checkpoint);
        index = index == 0 ? 0 : index - 1;  // handle underflow
        std::string data = seg.payload().copy();
        this->_fin |= seg.header().fin;
        _reassembler.push_substring(data, index, seg.header().fin);
        // _lower = _lower + static_cast<int32_t>(seg.length_in_sequence_space());
        _lower = wrap(_reassembler.first_unassembled_byte(), __isn.value()) + 1 + seg.header().fin;
        return true;
    }
    return false;
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!__isn)
        return std::nullopt;
    return _lower;
}

size_t TCPReceiver::window_size() const {
    // std::cout << "cap: " << _capacity << " unassembled: " << unassembled_bytes()
    //           << " stream remain: " << stream_out().remaining_capacity() << std::endl;
    // return _capacity - unassembled_bytes();
    return stream_out().remaining_capacity();
}
