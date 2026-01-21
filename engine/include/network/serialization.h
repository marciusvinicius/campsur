#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

namespace criogenio {

class NetWriter {
public:
  void WriteBytes(const void *data, size_t size) {
    const uint8_t *b = static_cast<const uint8_t *>(data);
    buffer.insert(buffer.end(), b, b + size);
  }

  template <typename T> void Write(const T &value) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "NetWriter::Write requires trivially copyable type");
    WriteBytes(&value, sizeof(T));
  }

  void WriteString(const std::string &s) {
    uint16_t len = static_cast<uint16_t>(s.size());
    Write(len);
    WriteBytes(s.data(), len);
  }

  const std::vector<uint8_t> &Data() const { return buffer; }
  size_t Size() const { return buffer.size(); }

private:
  std::vector<uint8_t> buffer;
};

class NetReader {
public:
  NetReader(const uint8_t *data, size_t size) : data(data), size(size) {}

  template <typename T> T Read() {
    static_assert(std::is_trivially_copyable_v<T>,
                  "NetReader::Read requires trivially copyable type");

    if (offset + sizeof(T) > size)
      throw std::runtime_error("NetReader overflow");

    T value;
    std::memcpy(&value, data + offset, sizeof(T));
    offset += sizeof(T);
    return value;
  }

  std::string ReadString() {
    uint16_t len = Read<uint16_t>();
    if (offset + len > size)
      throw std::runtime_error("NetReader overflow");

    std::string s(reinterpret_cast<const char *>(data + offset), len);
    offset += len;
    return s;
  }

  bool End() const { return offset >= size; }

private:
  const uint8_t *data;
  size_t size;
  size_t offset = 0;
};

} // namespace criogenio
