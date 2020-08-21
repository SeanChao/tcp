#include "stream_reassembler.hh"

#include <iostream>
#include <spdlog/spdlog.h>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

using namespace std;

// TODO: refactor this lib with real `capacity` limit

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _next_index(0), _unassembled_seg(), _unassembled_bytes(0) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // build segment
    struct segment *seg = new segment(data, index, eof);
    // spdlog::info("BS: pushed {} next_idx {}", seg.summary(), _next_index);
    // eliminate overlapped segments
    for (auto it = _unassembled_seg.begin(); it != _unassembled_seg.end();) {
        struct segment *s = *it;  // get a reference to original segment
        if (s->get_start() >= seg->get_start() && s->get_end() <= seg->get_end()) {
            // spdlog::info("BS: erase duplicate {}", s.summary());
            this->_unassembled_bytes -= s->get_len();
            free(s);
            it = _unassembled_seg.erase(it);
        } else if (s->get_start() <= seg->get_start() && s->get_end() >= seg->get_end()) {
            // spdlog::info("BS: ignore {}", seg.summary());
            return;
        } else
            it++;
    }

    // insert this segment to multiset
    _unassembled_seg.insert(seg);
    this->_unassembled_bytes += data.length();

    // try to write data to bytestream
    for (auto it = _unassembled_seg.begin(); it != _unassembled_seg.end();) {
        struct segment *s = *it;
        // nothing to read now
        if (s->get_start() > _next_index)
            break;
        // the segment doesn't include data to read
        if (s->get_end() < _next_index ||
            (s->get_start() <= this->_next_index && _next_index - s->get_start() >= s->_data.length())) {
            this->_unassembled_bytes -= s->get_len();
            if (s->_eof) {
                // spdlog::info("BS: end input");
                _output.end_input();
            }
            // spdlog::info("BS: erase nodata {}", s.summary());
            free(s);
            it = _unassembled_seg.erase(it);
            continue;
        }
        if (s->get_start() <= this->_next_index) {
            size_t start_pos = _next_index - s->get_start();
            size_t stream_remain_size = _output.remaining_capacity();
            size_t written = _output.write(s->_data.substr(start_pos, stream_remain_size));
            if (s->_eof) {
                // spdlog::info("BS: end input");
                _output.end_input();
            }
            this->_next_index += written;
            this->_unassembled_bytes -= written;
            // spdlog::info("BS: erase written {}", s.summary());
            free(s);
            it = _unassembled_seg.erase(it);
        } else
            it++;
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_seg.empty(); }
