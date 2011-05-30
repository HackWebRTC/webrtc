#ifndef MODULE_H
#define MODULE_H

#include "typedefs.h"

namespace webrtc
{

class Module
{
public:
    // Returns version of the module and its components.
    virtual int32_t Version(char* version,
                            uint32_t& remainingBufferInBytes,
                            uint32_t& position) const = 0;

    // Change the unique identifier of this object.
    virtual int32_t ChangeUniqueId(const int32_t id) = 0;

    // Returns the number of milliseconds until the module want a worker
    // thread to call Process.
    virtual int32_t TimeUntilNextProcess() = 0 ;

    // Process any pending tasks such as timeouts.
    virtual int32_t Process() = 0 ;

protected:
    virtual ~Module() {}
};

} // namespace webrtc

#endif // MODULE_H
