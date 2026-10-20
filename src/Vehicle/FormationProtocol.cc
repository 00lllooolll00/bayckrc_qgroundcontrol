#include "FormationProtocol.h"

namespace FormationProtocol {

CommandParams formAssignParams(const FormationAssign& assign)
{
    CommandParams params;
    params.param1 = static_cast<float>(assign.formationId);
    params.param2 = static_cast<float>(assign.slot);
    params.param3 = static_cast<float>(assign.leaderId);
    params.param4 = assign.dxEast;
    params.param5 = assign.dyNorth;
    params.param6 = assign.dz;
    params.param7 = assign.yawDeg;
    return params;
}

uint32_t nextFormationId(uint32_t current)
{
    // 用 >= limit-1 判断，避免 current=UINT32_MAX 时回绕成 0
    if (current == 0 || current >= kFormationIdLimit - 1) {
        return 1;
    }
    return current + 1;
}

}  // namespace FormationProtocol
