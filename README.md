# Container for experimentations

## HOWTO

### Add backtrace

In the file where you want to print the backtrace:
```c++
#define BOOST_STACKTRACE_USE_ADDR2LINE
#include <boost/stacktrace.hpp>
```
```c++
std::cout << boost::stacktrace::stacktrace();
```

In the ThirdpartyToolchain.cmake file of arrow, remove slim boost source URLs:
```bash
  set_urls(
    BOOST_SOURCE_URL
    # These are trimmed boost bundles we maintain.
    # See cpp/build_support/trim-boost.sh
    # "https://dl.bintray.com/ursalabs/arrow-boost/boost_${ARROW_BOOST_BUILD_VERSION_UNDERSCORES}.tar.gz"
    "https://dl.bintray.com/boostorg/release/${ARROW_BOOST_BUILD_VERSION}/source/boost_${ARROW_BOOST_BUILD_VERSION_UNDERSCORES}.tar.gz"
    "https://github.com/boostorg/boost/archive/boost-${ARROW_BOOST_BUILD_VERSION}.tar.gz"
    # FIXME(ARROW-6407) automate uploading this archive to ensure it reflects
    # our currently used packages and doesn't fall out of sync with
    # ${ARROW_BOOST_BUILD_VERSION_UNDERSCORES}
    # 
  )
```
