#include "byte_stream.hh"

#include <algorithm>
#include <iterator>
#include <stdexcept>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

using namespace std;

ByteStream::ByteStream(const size_t cap)
    : capacity(cap)
    , buf_len(cap + 1)
    , buffer(string(buf_len, 0))
    , start(0)
    , end(0)
    , inputEndFlag(false)
    , read_cnt(0)
    , write_cnt(0) {}

size_t ByteStream::write(const string &data) {
    if (error())
        return 0;
    size_t rc = remaining_capacity();
    size_t s_len = data.length();
    size_t len = rc < s_len ? rc : s_len;
    if (end + len >= buf_len) {
        buffer.replace(end + 1, buf_len - end - 1, data, 0, buf_len - end - 1);
        buffer.replace(0, len - buf_len + end + 1, data, buf_len - end - 1, len - buf_len + end + 1);
    } else {
        buffer.replace(end + 1, len, data, 0, len);
    }
    end = (end + len) < buf_len ? (end + len) : (end + len) % buf_len;
    this->write_cnt += len;
    return len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    if (error() || eof()) {
        return "";
    }
    size_t buf_size = buffer_size();
    size_t max_size = len < buf_size ? len : buf_size;
    std::string peek = "";
    if (start + max_size >= buf_len) {
        peek.append(buffer.substr(start + 1));
        peek.append(buffer.substr(0, max_size - buf_len + start + 1));
    } else {
        peek.append(buffer.substr(start + 1, max_size));
    }
    return peek;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    if (error() || eof()) {
        return;
    }
    size_t buf_size = buffer_size();
    size_t max_size = len < buf_size ? len : buf_size;
    start = (start + max_size) < buf_len ? (start + max_size) : (start + max_size) % buf_len;
    this->read_cnt += max_size;
}

std::string ByteStream::read(const size_t len) {
    if (error() || eof()) {
        return "";
    }
    size_t buf_size = buffer_size();
    size_t max_size = len < buf_size ? len : buf_size;
    std::string peek = "";
    if (start + max_size >= buf_len) {
        peek.append(buffer.substr(start + 1));
        peek.append(buffer.substr(0, max_size - buf_len + start + 1));
    } else {
        peek.append(buffer.substr(start + 1, max_size));
    }
    start = (start + max_size) < buf_len ? (start + max_size) : (start + max_size) % buf_len;
    this->read_cnt += max_size;
    return peek;
}

void ByteStream::end_input() { this->inputEndFlag = true; }

bool ByteStream::input_ended() const { return this->inputEndFlag; }

size_t ByteStream::buffer_size() const { return (buf_len + end - start) % buf_len; }

bool ByteStream::buffer_empty() const { return start == end; }

bool ByteStream::eof() const { return buffer_empty() && input_ended(); }

size_t ByteStream::bytes_written() const { return this->write_cnt; }

size_t ByteStream::bytes_read() const { return this->read_cnt; }

size_t ByteStream::remaining_capacity() const { return capacity - this->buffer_size(); }

std::string ByteStream::summary() const {
    return "end: " + to_string(input_ended()) + " err: " + to_string(error()) + " eof: " + to_string(eof()) +
           " written: " + to_string(bytes_written()) + " read: " + to_string(bytes_read()) +
           " empty: " + to_string(buffer_empty());
}
