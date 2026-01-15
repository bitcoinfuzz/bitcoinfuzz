#include <bitcoinfuzz/basemodule.h>
#include <span>

namespace bitcoinfuzz {
namespace module {
class DynamicKernel : public BaseModule {
public:
    const std::string library_path;
    void *lib_handle;

    DynamicKernel(const std::string &name, const std::string &library_path);

    std::optional<std::string>
    kernel_block(std::span<const uint8_t> buffer) const override;
    ~DynamicKernel() noexcept override;
};
} // namespace module
} // namespace bitcoinfuzz