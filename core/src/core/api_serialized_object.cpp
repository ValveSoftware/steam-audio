//
// Copyright 2017-2023 Valve Corporation.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "serialized_object.h"
using namespace ipl;

#include "phonon.h"
#include "util.h"

#define STEAMAUDIO_SKIP_API_FUNCTIONS
#include "phonon_interfaces.h"
#include "api_context.h"
#include "api_serialized_object.h"

namespace api {

// --------------------------------------------------------------------------------------------------------------------
// CSerializedObject
// --------------------------------------------------------------------------------------------------------------------

CSerializedObject::CSerializedObject(CContext* context,
                                     IPLSerializedObjectSettings* settings)
{
    auto _context = context->mHandle.get();
    if (!_context)
        throw Exception(Status::Failure);

    if (settings->data && settings->size > 0)
    {
        new (&mHandle) Handle<SerializedObject>(ipl::make_shared<SerializedObject>(settings->size, settings->data), _context);
    }
    else
    {
        new (&mHandle) Handle<SerializedObject>(ipl::make_shared<SerializedObject>(), _context);
    }
}

ISerializedObject* CSerializedObject::retain()
{
    mHandle.retain();
    return this;
}

void CSerializedObject::release()
{
    if (mHandle.release())
    {
        this->~CSerializedObject();
        gMemory().free(this);
    }
}

IPLsize CSerializedObject::getSize()
{
    auto _serializedObject = mHandle.get();
    if (!_serializedObject)
        return 0;

    return _serializedObject->size();
}

IPLbyte* CSerializedObject::getData()
{
    auto _serializedObject = mHandle.get();
    if (!_serializedObject)
        return 0;

    return (IPLbyte*) _serializedObject->data();
}


// --------------------------------------------------------------------------------------------------------------------
// CContext
// --------------------------------------------------------------------------------------------------------------------

IPLerror CContext::createSerializedObject(IPLSerializedObjectSettings* settings,
                                          ISerializedObject** serializedObject)
{
    if (!settings || !serializedObject)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _serializedObject = reinterpret_cast<CSerializedObject*>(gMemory().allocate(sizeof(CSerializedObject), Memory::kDefaultAlignment));
        new (_serializedObject) CSerializedObject(this, settings);
        *serializedObject = _serializedObject;
    }
    catch (Exception exception)
    {
        return static_cast<IPLerror>(exception.status());
    }

    return IPL_STATUS_SUCCESS;
}

}
