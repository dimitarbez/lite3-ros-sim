#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <sstream>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

constexpr size_t kSequenceRecordCapacity = 4096;

#pragma pack(push, 1)
struct SequenceRecordHeader {
  char magic[4];
  uint32_t version;
  uint64_t sequence;
  uint64_t receive_ns;
  uint32_t length;
};

struct EmotionStateWire {
  uint32_t record_version;
  uint64_t transport_sequence;
  uint64_t state_sequence;
  double valence;
  double arousal;
  char emotion[16];
  char session_id[64];
  char turn_id[128];
};

struct SafetyStateWire {
  uint32_t record_version;
  uint8_t stop;
  uint8_t observed_fresh;
  uint8_t axes_zero;
  uint8_t padding[5];
  uint64_t sequence;
};
#pragma pack(pop)

static_assert(sizeof(SequenceRecordHeader) == 28, "record header layout changed");
static_assert(sizeof(EmotionStateWire) == 244, "emotion record layout changed");
static_assert(sizeof(SafetyStateWire) == 20, "safety record layout changed");

inline bool SafetyRecordAllowsOwnership(const SafetyStateWire& value) {
  return value.record_version == 0x00010000u && value.stop == 0 &&
      value.observed_fresh != 0 && value.axes_zero != 0;
}

inline int64_t SharedMonotonicNs() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}

inline std::string FixedString(const char* value, size_t capacity) {
  return std::string(value, strnlen(value, capacity));
}

template <typename Payload>
class SequenceRecordReader {
 public:
  ~SequenceRecordReader() {
    if (mapping_ != MAP_FAILED) munmap(mapping_, kSequenceRecordCapacity);
    if (fd_ >= 0) close(fd_);
  }

  bool Open(const std::string& path) {
    fd_ = open(path.c_str(), O_RDONLY);
    if (fd_ < 0) return false;
    struct stat details {};
    if (fstat(fd_, &details) != 0 ||
        details.st_size != static_cast<off_t>(kSequenceRecordCapacity)) return false;
    mapping_ = mmap(nullptr, kSequenceRecordCapacity, PROT_READ, MAP_SHARED, fd_, 0);
    return mapping_ != MAP_FAILED;
  }

  bool Read(Payload* output, int64_t* receive_ns) {
    for (int attempt = 0; attempt < 8; ++attempt) {
      SequenceRecordHeader first {};
      SequenceRecordHeader second {};
      Payload candidate {};
      std::memcpy(&first, mapping_, sizeof(first));
      std::atomic_thread_fence(std::memory_order_acquire);
      if (std::memcmp(first.magic, "EBL3", 4) != 0 || first.version != 1 ||
          first.sequence % 2 != 0 || first.length != sizeof(Payload)) continue;
      std::memcpy(&candidate,
                  static_cast<const char*>(mapping_) + sizeof(first),
                  sizeof(candidate));
      std::atomic_thread_fence(std::memory_order_acquire);
      std::memcpy(&second, mapping_, sizeof(second));
      if (std::memcmp(&first, &second, sizeof(first)) == 0) {
        state_ = candidate;
        receive_ns_ = static_cast<int64_t>(first.receive_ns);
        valid_ = true;
        break;
      }
    }
    if (!valid_) return false;
    *output = state_;
    *receive_ns = receive_ns_;
    return true;
  }

 private:
  int fd_{-1};
  void* mapping_{MAP_FAILED};
  bool valid_{false};
  int64_t receive_ns_{0};
  Payload state_{};
};

class SequenceRecordWriter {
 public:
  ~SequenceRecordWriter() {
    if (mapping_ != MAP_FAILED) munmap(mapping_, kSequenceRecordCapacity);
    if (fd_ >= 0) close(fd_);
  }

  bool Open(const std::string& path) {
    fd_ = open(path.c_str(), O_CREAT | O_RDWR, 0640);
    if (fd_ < 0 || ftruncate(fd_, kSequenceRecordCapacity) != 0) return false;
    mapping_ = mmap(nullptr, kSequenceRecordCapacity,
                    PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (mapping_ == MAP_FAILED) return false;
    SequenceRecordHeader empty {{'E','B','L','3'}, 1, 0, 0, 0};
    std::memcpy(mapping_, &empty, sizeof(empty));
    return true;
  }

  bool Write(const std::string& value) {
    if (value.size() > kSequenceRecordCapacity - sizeof(SequenceRecordHeader)) return false;
    SequenceRecordHeader header {};
    std::memcpy(&header, mapping_, sizeof(header));
    uint64_t odd = header.sequence % 2 == 0 ? header.sequence + 1 : header.sequence + 2;
    SequenceRecordHeader writing {{'E','B','L','3'}, 1, odd,
                                  static_cast<uint64_t>(SharedMonotonicNs()),
                                  static_cast<uint32_t>(value.size())};
    std::memcpy(mapping_, &writing, sizeof(writing));
    std::atomic_thread_fence(std::memory_order_release);
    std::memcpy(static_cast<char*>(mapping_) + sizeof(writing),
                value.data(), value.size());
    std::atomic_thread_fence(std::memory_order_release);
    writing.sequence = odd + 1;
    std::memcpy(mapping_, &writing, sizeof(writing));
    return true;
  }

 private:
  int fd_{-1};
  void* mapping_{MAP_FAILED};
};

inline std::string JsonEscape(const std::string& value) {
  std::ostringstream output;
  for (char character : value) {
    if (character == '\\' || character == '"') output << '\\';
    if (character == '\n') output << "\\n";
    else output << character;
  }
  return output.str();
}
