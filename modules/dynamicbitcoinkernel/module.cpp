#include "module.h"
#include <dlfcn.h>
#include <span>
namespace bitcoinfuzz {
namespace module {
DynamicKernel::DynamicKernel(std::string const &name, std::string const &library_path)
: BaseModule(name), library_path(library_path) {
    this->lib_handle = dlopen(library_path.c_str(), RTLD_LAZY | RTLD_LOCAL);
    std::cout << "Loaded " << name << " from " << library_path << std::endl;
    if (!this->lib_handle) {
        throw std::runtime_error(dlerror());
    }

    dlerror();

    //functions
    // not mapped yet

    const char* err = dlerror();
    if (err) {
        dlclose(lib_handle);
        throw std::runtime_error(err);
    }
}

DynamicKernel::~DynamicKernel(){
    if(this->lib_handle){
        dlclose(this->lib_handle);
    }
}


std::optional<std::string>
DynamicKernel::kernel_block(std::span<const uint8_t> buffer) const {
    return std::string("kernel_block not implemented");
}

} // namespace module
} // namespace bitcoinfuzz

