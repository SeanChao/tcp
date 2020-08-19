#include "tcp_connection.hh"
// #include "spdlog/spdlog.h"
#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return timer.interval(); }

void TCPConnection::segment_received(const TCPSegment &seg) {
    spdlog::info("CC: <- 📦 " + seg.header().summary() + " len: " + to_string(seg.length_in_sequence_space()) +
                 " data: " + seg.payload().copy().substr(0, 40));
    // cout << "CC: <- 📦 " << seg.header().summary() << " len: " << seg.length_in_sequence_space()
    //  << " data: " << seg.payload().copy().substr(0, 40) << endl;
    timer.mark();
    conn_end_detect();
    if (!_active)
        return;
    // LISTEN mode, ignore all RST
    if (!state_connect && seg.header().rst)
        return;
    // LISTEN mode, not SYN yet, any ACK should result in RST
    // ignore (relaxed)
    if (!state_connect && listen_wait_syn && !seg.header().syn) {
        // send_rst();
        return;
    }
    bool in_win = _receiver.segment_received(seg);
    if (!in_win) {
        spdlog::info("RX: Rejected an inbound segment");
        // No ack to out-of-window RST
        if (seg.header().rst) {
            if (seg.header().ack && seg.header().ackno == seg.header().seqno + 1) {
                reset();
                return;
            } else
                return;
        }
        // Waiting for SYN, do not send empty segment
        if (!_receiver.ackno().has_value()) {
            // Good ACK, ignore
            if (seg.header().ack && seg.header().ackno != seg.header().seqno + 1)
                // send_rst(seg.header().ackno);
                return;
        }
        _sender.send_empty_segment();
        this->send_out();
        return;
    }
    bool send_ack = false;
    if (seg.length_in_sequence_space() > 0)
        send_ack = true;
    if (seg.header().ack) {
        bool s_valid = _sender.ack_received(seg.header().ackno, seg.header().win);
        if (!s_valid)
            send_ack = true;
    } else if (seg.header().syn) {
        if (!state_connect)
            listen_wait_syn = false;
        _sender.fill_window();
        send_ack = false;  // No need to manually create a segment
    }
    if (seg.header().syn) {
        if (state_connect)
            send_ack = true;
        // _sender.send_empty_segment();
    }
    if (seg.header().rst) {
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
        _active = false;
    }
    // ack incoming ack with fin
    if (seg.header().fin && seg.header().ack) {
        send_ack = true;
        // _sender.send_empty_segment();
    }

    if (send_ack)
        _sender.send_empty_segment();
    this->send_out();
}

bool TCPConnection::active() const {
    // conn_end_detect();
    // spdlog::info("active: {}", _active);
    return _active;
}

size_t TCPConnection::write(const string &data) {
    size_t size = _sender.stream_in().write(data);
    _sender.fill_window();
    this->send_out();
    return size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    timer.update(ms_since_last_tick);
    _sender.tick(ms_since_last_tick);
    if (ms_since_last_tick != 0) {
        spdlog::info("CC: ⌛ " + to_string(ms_since_last_tick) + "ms");
        conn_end_detect();
    }
    // If the stream is ongoing
    if (_sender.next_seqno_absolute() > _sender.bytes_in_flight() && (!_sender.stream_in().eof() || !fin_sent)) {
        _sender.fill_window();
    }
    this->send_out();
}

void TCPConnection::end_input_stream() {
    conn_end_detect();
    spdlog::info("CC: sender input stream end");
    _sender.stream_in().end_input();
    _sender.fill_window();
    this->send_out();
}

// Connect to a remote server
void TCPConnection::connect() {
    spdlog::info("🎉 Connect");
    this->state_connect = true;
    _sender.fill_window();
    this->send_out();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            // need to send a RST segment to the peer
            spdlog::info("CC: deconstructor called while the connection is active");
            send_rst();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::send_out() {
    std::queue<TCPSegment> &sender_seg_out = _sender.segments_out();
    while (!sender_seg_out.empty()) {
        TCPSegment seg = sender_seg_out.front();
        std::optional<WrappingInt32> ackno = _receiver.ackno();
        // Before sending the segment, the TCPConnection will ask the TCPReceiver for the
        // fields it’s responsible for on outgoing segments: ackno and window size. If there is an
        // ackno, it will set the ack flag and the fields in the TCPSegment.
        if (ackno) {
            seg.header().ack = true;
            seg.header().ackno = ackno.value();
        }
        if (seg.header().fin)
            fin_sent = true;
        seg.header().win = _receiver.window_size();
        _segments_out.push(seg);
        spdlog::info("CC: -> 📦 " + seg.header().summary() + " len: {}", seg.length_in_sequence_space());
        sender_seg_out.pop();
    }
}

void TCPConnection::reset() {
    cout << "CC: ⚠ reset" << endl;
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _active = false;
}

void TCPConnection::send_rst(optional<WrappingInt32> seqno) {
    while (!_sender.segments_out().empty())
        _sender.segments_out().pop();
    _sender.send_empty_segment();
    TCPSegment top = _sender.segments_out().front();
    _sender.segments_out().pop();
    top.header().rst = 1;
    if (seqno)
        top.header().seqno = seqno.value();
    _segments_out.push(top);
    spdlog::info("CC: send RST " + top.header().summary());
    // set streams to error
    reset();
    _linger_after_streams_finish = false;
    spdlog::info("❌ Connection reset.");
}

bool TCPConnection::conn_end_detect() {
    if (!_active)
        return true;
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        spdlog::info("CC: 🤦‍♂️ reset due to too many retransmissions.");
        send_rst(_sender.next_seqno() - _sender.bytes_in_flight());
        return true;
    }
    // Passive close: The remote peers ends first
    if (_receiver.stream_out().input_ended() && !_sender.stream_in().eof() && _linger_after_streams_finish) {
        _linger_after_streams_finish = false;
        spdlog::info("CC: (Passive Close Enabled)");
    }
    // bytes_in_flight == 0 implies fin acked
    bool fin = _sender.stream_in().input_ended() && _sender.stream_in().eof() && _receiver.stream_out().eof() &&
               fin_sent && bytes_in_flight() == 0;
    if (fin) {
        if (_linger_after_streams_finish) {
            uint64_t linger_time = timer.interval();
            if (linger_time >= 10 * _cfg.rt_timeout) {
                _active = false;
                spdlog::info("👋 Connection closed. (Timeout)");
            }
        } else {
            // connection is done
            _active = false;
            spdlog::info("👋 Connection closed.");
        }
    }
    spdlog::info("CC: fin={}, conditions[s_stream_end={}, eof={}, r_stream_end={}, fin_sent={}, bytes_in_flight={}] "
                 "rx_stream: {} tx_stream: {}",
                 fin,
                 _sender.stream_in().input_ended(),
                 _sender.stream_in().eof(),
                 _receiver.stream_out().eof(),
                 fin_sent,
                 bytes_in_flight(),
                 _receiver.stream_out().summary(),
                 _sender.stream_in().summary());
    return fin;
}

std::string rand_string(const int len) {
    static const char alphanum[] = "0123456789"
                                   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                   "abcdefghijklmnopqrstuvwxyz";
    std::string s;
    for (int i = 0; i < len; ++i) {
        s.push_back(alphanum[rand() % (sizeof(alphanum) - 1)]);
    }
    return s;
}