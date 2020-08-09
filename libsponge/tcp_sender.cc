#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <iostream>
#include <memory>
#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _window_size(1)
    , _checkpoint(0)
    , timer(_initial_retransmission_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    if (this->fin)
        return;

    TCPSegment seg;
    TCPHeader header;

    size_t size_under_window = _window_size - bytes_in_flight();
    size_t size = size_under_window < TCPConfig::MAX_PAYLOAD_SIZE ? size_under_window : TCPConfig::MAX_PAYLOAD_SIZE;

    if (_next_seqno == 0)
        header.syn = true;
    if (_stream.input_ended() && _stream.buffer_size() < size_under_window) {
        header.fin = true;
        this->fin = true;
    }

    if (size <= 0)
        return;
    std::string data = _stream.read(size);
    Buffer buf(std::move(data));
    seg.payload() = buf;

    header.seqno = wrap(_next_seqno, _isn);
    seg.header() = header;
    _next_seqno += seg.length_in_sequence_space();
    if (!header.syn && !header.fin && buf.size() == 0)
        return;
    send_segment(seg);
    _outstanding.push(seg);
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//! \returns `false` if the ackno appears invalid (acknowledges something the TCPSender hasn't sent yet)
bool TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    // if (this->fin)
    // return true;
    uint64_t u_ackno = _unwrap(ackno);
    cout << "Received: w-ackno: " << ackno << " u " << u_ackno << endl;
    if (_next_seqno < u_ackno)
        return false;
    _window_size = window_size;
    if (!_outstanding.empty()) {
        uint64_t allowed_min_ackno =
            _unwrap(_outstanding.front().header().seqno) + _outstanding.front().length_in_sequence_space();
        if (u_ackno < allowed_min_ackno)
            // wrong ackno, return with true
            return true;
    }
    if (u_ackno > cur_max_ackno) {
        cur_max_ackno = u_ackno;
        timer.set_rto(_initial_retransmission_timeout);
        if (!_outstanding.empty())
            timer.restart();
        _consecutive_retransmit = 0;
    }
    // The TCPSender should look through its collection of outstanding segments and remove any
    // that have now been fully acknowledged (the ackno is greater than all of the sequence
    // numbers in the segment).
    while (!_outstanding.empty()) {
        TCPSegment seg = _outstanding.front();
        if (_unwrap(seg.header().seqno) <= u_ackno) {
            _outstanding.pop();
            cout << "bytes in flight " << _bytes_in_flight << " reduce by " << seg.length_in_sequence_space() << " "
                 << seg.header().summary() << " " << seg.payload().copy() << endl;
            _bytes_in_flight -= seg.length_in_sequence_space();
        } else
            break;
    }
    _next_seqno = _unwrap(ackno);
    // When all outstanding data has been acknowledged, turn off the retransmission timer.
    if (_outstanding.empty())
        timer.stop();
    return true;
}

// Time has passed; the TCPSender will check if the retransmission timer has expired
// and, if so, retransmit the outstanding segment with the lowest sequence number
// \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    timer.update(ms_since_last_tick);
    bool expired = timer.expire();
    if (expired) {
        if (!_outstanding.empty()) {
            TCPSegment seg = _outstanding.front();
            _segments_out.push(seg);
        }
        if (_window_size > 0) {
            _consecutive_retransmit++;
            timer.double_rto();
            timer.restart();
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmit; }

void TCPSender::send_segment(TCPSegment &segment) {
    _segments_out.push(segment);
    if (segment.length_in_sequence_space() > 0) {
        timer.start();
    }
    _bytes_in_flight += segment.length_in_sequence_space();
    // update checkpoint
    this->_checkpoint = _unwrap(segment.header().seqno) + segment.length_in_sequence_space();
    cout << "Sent a segment " << segment.header().summary() << " payload: " << segment.payload().copy() << endl;
}

uint64_t TCPSender::_unwrap(WrappingInt32 n) const { return unwrap(n, _isn, _checkpoint); }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
    cout << "send empty segment: " << seg.header().summary() << endl;
}

void Timer::start() { running = true; }

void Timer::stop() {
    running = false;
    time = 0;
}

void Timer::update(size_t ms_since_last_tick) { time += running ? ms_since_last_tick : 0; }

bool Timer::expire() const { return time >= rto; }

void Timer::double_rto() { this->rto *= 2; }

void Timer::set_rto(uint64_t timeout) { this->rto = timeout; }

void Timer::restart() { this->time = 0; }
