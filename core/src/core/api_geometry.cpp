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

#include "vector.h"
using namespace ipl;

#include "phonon.h"

#define STEAMAUDIO_SKIP_API_FUNCTIONS
#include "phonon_interfaces.h"
#include "api_context.h"

namespace api {

// --------------------------------------------------------------------------------------------------------------------
// CContext
// --------------------------------------------------------------------------------------------------------------------

IPLVector3 CContext::calculateRelativeDirection(IPLVector3 sourcePosition,
                                                IPLVector3 listenerPosition,
                                                IPLVector3 listenerAhead,
                                                IPLVector3 listenerUp)
{
    auto _sourcePosition = Vector3f(sourcePosition.x, sourcePosition.y, sourcePosition.z);
    auto _listenerPosition = Vector3f(listenerPosition.x, listenerPosition.y, listenerPosition.z);
    auto _listenerAhead = Vector3f(listenerAhead.x, listenerAhead.y, listenerAhead.z);
    auto _listenerUp = Vector3f(listenerUp.x, listenerUp.y, listenerUp.z);
    auto _listenerRight = Vector3f::unitVector(Vector3f::cross(_listenerAhead, _listenerUp));

    auto listenerToSource = _sourcePosition - _listenerPosition;
    auto _relativeDirection = Vector3f::kYAxis;

    if (listenerToSource.length() > 1e-5f)
    {
        listenerToSource = Vector3f::unitVector(listenerToSource);

        _relativeDirection.x() = (Vector3f::dot(listenerToSource, _listenerRight));
        _relativeDirection.y() = (Vector3f::dot(listenerToSource, _listenerUp));
        _relativeDirection.z() = (-Vector3f::dot(listenerToSource, _listenerAhead));
    }

    IPLVector3 relativeDirection = {_relativeDirection.x(), _relativeDirection.y(), _relativeDirection.z()};
    return relativeDirection;
}

}
