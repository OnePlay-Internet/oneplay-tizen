#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

#include "nacl_io/nacl_io.h"

class MoonlightInstance : public pp::Instance {
    public:
        explicit MoonlightInstance(PP_Instance instance) : pp::Instance(instance) {            
            // This function MUST be used otherwise sockets don't work (nacl_io_init() doesn't work!)            
            nacl_io_init_ppapi(pp_instance(), pp::Module::Get()->get_browser_interface());
        }
        virtual ~MoonlightInstance() {}
};

class MoonlightModule : public pp::Module {
    public:
        MoonlightModule() : pp::Module() {}
        virtual ~MoonlightModule() {}

        virtual pp::Instance* CreateInstance(PP_Instance instance) {
            return new MoonlightInstance(instance);
        }
};

namespace pp {
Module* CreateModule() {
    
    return new MoonlightModule();
}
}  // namespace pp