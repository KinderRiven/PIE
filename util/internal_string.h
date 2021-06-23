#ifndef PIE_UTIL_INTERNAL_STRING_H__
#define PIE_UTIL_INTERNAL_STRING_H__

#include <cstdint>
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <cassert>
#include <algorithm>

namespace PIE {

  class InternalString {

  public:
    InternalString()=default;

    InternalString(const char *str, size_t len) {
      data = new uint8_t [len+sizeof(uint32_t)];
      *(uint32_t*)data = len;
      memcpy(data+sizeof(uint32_t), str, len);
    }

    // Copy outside memory into a location dst, dst is 
    // managed by current created object.
    // Note that this constructor does not check if space
    // of dst is enough
    InternalString(const char *str, size_t len, uint8_t* dst) {
      data = dst;
      *(uint32_t*)data = len;
      memcpy(data+sizeof(uint32_t), str, len);
    }

    // Receive a raw 8B value and treat it as a pointer
    // pointing to a memory position
    InternalString(uint64_t dst) {
      data = reinterpret_cast<uint8_t*>(dst);
    }

    // TODO: Is it a good idea not to deallocate
    // memory of InternalString?
    // ~InternalString() { delete data; }
    ~InternalString()=default;

    // Copyable Semantics
    InternalString(const InternalString& rhs) {
      data = new uint8_t [rhs.Length() + sizeof(uint32_t)];
      memcpy(data, rhs.data, rhs.Length() + sizeof(uint32_t));
    }

    InternalString& operator= (const InternalString &rhs) {
      if (this->data == rhs.data) { return *this; }
      if (Length() < rhs.Length()) {
        delete data;
        data = new uint8_t [rhs.Length() + sizeof(uint32_t)];
      }
      memcpy(data, rhs.data, rhs.Length() + sizeof(uint32_t));
      return *this;
    }

    InternalString& operator= (InternalString&& rhs) {
      if (this->data == rhs.data) { return *this; }
      delete data;
      data = rhs.data;
      rhs.data = nullptr;
      return *this;
    }

    InternalString& operator= (uint64_t rhs) {
      data = reinterpret_cast<uint8_t*>(rhs);
      return *this;
    }

    // Return the length of string;
    size_t Length() const { 
      if (data == nullptr) { return 0; }
      return *(uint32_t *)data;
    }

    // Return the data of string
    const uint8_t *Data() const { return data + sizeof(uint32_t); }

    // Return the nth Byte of target string array
    // make sure the index is inside the array range
    uint8_t operator[](size_t n) const {
      return *(data + sizeof(uint32_t) + n);  // increase 4 to skip length field
    }

    // Security check
    uint8_t At(size_t n) {
      assert(n < Length());
      return (*this)[n];
    }

    // compare current key with given key
    // Returns:
    //   <  0 iff "*this" <  "b",
    //   == 0 iff "*this" == "b",
    //   >  0 iff "*this" >  "b"  
    // Note: Make sure both StringKeys holds string!!!
    int compare(const InternalString& rhs) const {
      auto min_cmp = std::min(Length(), rhs.Length());
      auto results = memcmp(Data(), rhs.Data(), min_cmp);

      // If one string if another's prefix
      // Compare their length
      if (results == 0) {
        if (Length() == rhs.Length()) { return 0; }
        if (Length() > rhs.Length()) { return 1;}
        return -1;
      }
      
      return results;
    }

    // operator overload for convinience
    bool operator==(const InternalString& rhs) const { 
      return compare(rhs) == 0; 
    }

    bool operator<(const InternalString& rhs) const { 
      return compare(rhs) < 0; 
    }

    bool operator>(const InternalString& rhs) const { 
      return compare(rhs) > 0; 
    }

    bool operator>=(const InternalString& rhs) const { 
      return compare(rhs) >= 0; 
    }

    bool operator<=(const InternalString& rhs) const { 
      return compare(rhs) <= 0;
    }

    // For Convinience, we provide an comparator which
    // compared with an integer, this comparator only 
    // treat input parameter as an 8B integer
    bool operator== (const uint64_t num) const {
      return reinterpret_cast<uint64_t>(data) == num;
    }

    // Return 8B value of this internal string's pointer
    uint64_t Raw() const {  return (uint64_t)data;  }

  private:
    uint8_t *data;
  };

};
#endif