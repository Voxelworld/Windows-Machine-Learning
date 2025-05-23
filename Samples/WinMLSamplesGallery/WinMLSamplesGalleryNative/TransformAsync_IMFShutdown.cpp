#include "pch.h"
#include "TransformAsync.h"
#include <mferror.h>
#include <mfapi.h>

HRESULT TransformAsync::GetShutdownStatus(
    MFSHUTDOWN_STATUS* pStatus)
{
    if (pStatus == nullptr)
    {
        return E_POINTER;
    }
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_shutdown == false)
        {
            return MF_E_INVALIDREQUEST;
        }

        if (m_outputSampleAllocator) {
            m_outputSampleAllocator->UninitializeSampleAllocator();
        }

        *pStatus = MFSHUTDOWN_COMPLETED;
    }

    return S_OK;
}

HRESULT TransformAsync::Shutdown()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    RETURN_IF_FAILED(ShutdownEventQueue());
    m_shutdown = true;
    return S_OK;
}
