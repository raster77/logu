#include "TempFile.hpp"

#include <random>
#include <stdexcept>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <cstdio>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace logutils::temp_file {

  namespace {

    std::filesystem::path tempRoot() {
      std::error_code error;
      const std::filesystem::path root = std::filesystem::temp_directory_path(error);

      if (error) {
        throw std::runtime_error("could not locate the system temp directory (" + error.message() + ")");
      }

      return root;
    }

    // Eight characters drawn from a real entropy source. The exclusive create
    // below is what actually makes this safe; an unguessable name additionally
    // means an attacker can't camp on the path in advance.
    std::string randomStem() {
      static constexpr char ALPHABET[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
      static constexpr std::size_t ALPHABET_SIZE = sizeof(ALPHABET) - 1;

      std::random_device entropy;
      std::uniform_int_distribution<std::size_t> pick(0, ALPHABET_SIZE - 1);

      std::string out;
      out.reserve(8);

      for (int i = 0; i < 8; ++i) {
        out += ALPHABET[pick(entropy)];
      }

      return out;
    }

    // Creates `path` if and only if it doesn't already exist, and returns
    // whether it did. Deliberately not std::ofstream: that truncates whatever
    // is already there and follows a symlink to it, which is the pair of
    // behaviours this whole file exists to avoid.
    bool createExclusively(const std::filesystem::path &path) {
#ifdef _WIN32
      // "x" is C11 exclusive-create: fails rather than truncating.
      if (std::FILE *created = std::fopen(path.string().c_str(), "wx")) {
        std::fclose(created);
        return true;
      }

      return false;
#else
      // O_EXCL makes O_CREAT fail on an existing name, and -- specifically --
      // refuse to follow a symlink. 0600 keeps the merged log out of every
      // other user's reach on a shared machine.
      const int fd = ::open(path.c_str(), O_CREAT | O_EXCL | O_WRONLY, S_IRUSR | S_IWUSR);

      if (fd < 0) {
        return false;
      }
      // The caller reopens by path through std::ofstream. Closing here leaves
      // a brief gap, but the file now exists, is ours, and is mode 0600 -- and
      // the temp directory is sticky, so no other user can remove or replace
      // it in between.
      ::close(fd);

      return true;
#endif
    }

  } // namespace

  std::filesystem::path createPrivate(const std::string &prefix, const std::string &suffix) {
    const std::filesystem::path root = tempRoot();

    // Retried because a collision is possible in principle, if vanishingly
    // unlikely with a random stem -- and because on a full or read-only temp
    // directory every attempt fails for a reason retrying won't fix, which the
    // bounded loop turns into an error instead of a hang.
    for (int attempt = 0; attempt < 100; ++attempt) {
      const std::filesystem::path candidate = root / (prefix + randomStem() + suffix);

      if (createExclusively(candidate)) {
        return candidate;
      }
    }

    throw std::runtime_error("could not create a temp file in " + root.string());
  }

} // namespace logutils::temp_file
