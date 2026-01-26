#include <bitcoinfuzz/basemodule.h>
#include <memory>
#include <span>

struct KernelApiTable;

namespace bitcoinfuzz {
namespace module {
class DynamicKernel : public BaseModule {
public:
  DynamicKernel(const std::string &name, const std::string &library_path);

  std::optional<std::string>
  kernel_block(std::span<const uint8_t> buffer) const override;
  // showcase function, will be removed later
  std::optional<std::string> library_tag() const override;

  ~DynamicKernel() noexcept override;

private:
  std::shared_ptr<const KernelApiTable> loaded_api_table;

  char *libbitcoinkernel_block(std::span<const uint8_t> data) const;
};
} // namespace module
} // namespace bitcoinfuzz
