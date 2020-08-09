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
    , buffer(string(buf_len, 'x'))
    , start(0)
    , end(0)
    , inputEndFlag(false)
    , read_cnt(0)
    , write_cnt(0) {}

size_t ByteStream::write(const string &data) {
    size_t count = 0;
    for (size_t i = 0; i < data.length(); i++) {
        if (((end + 1) % buf_len) == start) {
            // buffer is full
            break;
        }
        end = (end + 1) % buf_len;
        buffer[end] = data[i];
        count++;
    }
    this->write_cnt += count;
    return count;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t pos = (start + 1) % buf_len;
    size_t cnt = 0;
    std::string peek = "";
    while (pos != ((end + 1) % buf_len) && cnt < len) {
        peek.push_back(buffer[pos]);
        pos = (pos + 1) % buf_len;
        cnt++;
    }
    return peek;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t size = len;
    while (size > 0 && start != end) {
        start = (start + 1) % buf_len;
        size--;
    }
    this->read_cnt += len;
}

std::string ByteStream::read(const size_t len) {
    const auto ret = peek_output(len);
    pop_output(len);
    this->read_cnt += len;
    return ret;
}

void ByteStream::end_input() { this->inputEndFlag = true; }

bool ByteStream::input_ended() const { return this->inputEndFlag; }

size_t ByteStream::buffer_size() const { return (buf_len + end - start) % buf_len; }

bool ByteStream::buffer_empty() const { return start == end; }

bool ByteStream::eof() const { return buffer_empty() && input_ended(); }

size_t ByteStream::bytes_written() const { return this->write_cnt; }

size_t ByteStream::bytes_read() const { return this->read_cnt; }

size_t ByteStream::remaining_capacity() const { return capacity - this->buffer_size(); }
