#ifndef PIE_SRC_INCLUDE_STATUS_HPP__
#define PIE_SRC_INCLUDE_STATUS_HPP__

namespace PIE {

enum status_code_t {
  kOk = 0,
  kInsertKeyExist = 1,
  kNotFound = 2,
  kNotDefined = 3,
  kNeedSplit = 4,  // Used for any index has "split" operation
  kFailed = 5
};

inline const char *StatusString(status_code_t code) {
  static const char *codestring[] = {"kOk", "kInsertKeyExist", "kNotFound",
                                     "kNotDefined", "kNeedSplit"};

  if (code < sizeof(codestring) / sizeof(char *)) {
    return codestring[code];
  }
  return "";
}

}  // namespace PIE

#endif
