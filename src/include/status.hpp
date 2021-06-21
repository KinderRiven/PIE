#ifndef PIE_SRC_INCLUDE_STATUS_HPP__
#define PIE_SRC_INCLUDE_STATUS_HPP__

namespace PIE {

  enum status_code_t {
    kOk = 0,
    kInsertKeyExist = 1,
    kNotFound = 2,
    kNotDefined = 3
  };

  const char *StatusString(status_code_t code) {
    static const char *codestring[] = {
      "kOk", "kInsertKeyExist", "kNotFound",
      "kNotDefined"
    };

    if (code < sizeof(codestring) / sizeof(char *)) {
      return codestring[code];
    }
    return "";
  }

}

#endif