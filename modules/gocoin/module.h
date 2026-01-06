#pragma once

#include <bitcoinfuzz/basemodule.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace bitcoinfuzz {
namespace module {

class Gocoin : public BaseModule {
public:
  Gocoin(void);

  std::optional<bool> script_eval(const std::vector<uint8_t> &input_data,
                                  unsigned int flags,
                                  size_t version) const override;

  ~Gocoin() noexcept override = default;
};

} // namespace module
} // namespace bitcoinfuzz
