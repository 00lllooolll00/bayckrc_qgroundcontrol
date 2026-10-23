#include "FormationProtocol.h"

#include <cmath>

namespace FormationProtocol {

CommandParams formConfigParams(const FormationConfig& config)
{
    CommandParams params;
    params.param1 = static_cast<float>(config.formationId);
    params.param2 = static_cast<float>(config.shape);
    params.param3 = config.spacingM;
    params.param4 = config.vAngleDeg;
    params.param5 = static_cast<float>(config.leaderOverride);
    params.param6 = config.enabled ? 1.0f : 0.0f;
    params.param7 = 0.0f;
    return params;
}

CommandParams formMemberParams(const FormationMemberCommand& member)
{
    CommandParams params;
    params.param1 = static_cast<float>(member.formationId);
    params.param2 = member.join ? 1.0f : 0.0f;
    params.param3 = 0.0f;
    params.param4 = 0.0f;
    params.param5 = 0.0f;
    params.param6 = 0.0f;
    params.param7 = 0.0f;
    return params;
}

CommandParams formStopParams(const FormationStop& stop)
{
    CommandParams params;
    params.param1 = static_cast<float>(stop.formationId);
    params.param2 = stop.allMembers ? 1.0f : 0.0f;
    return params;
}

bool validateConfig(const FormationConfig& config, QString* error)
{
    const auto fail = [error](const QString& reason) {
        if (error) {
            *error = reason;
        }
        return false;
    };

    if (config.formationId == 0) {
        return fail(QStringLiteral("编队 id 不能为 0"));
    }
    if (config.shape > kMaxShape) {
        return fail(QStringLiteral("队形模板超出范围"));
    }
    if (!std::isfinite(config.spacingM) || config.spacingM < kMinSpacingM || config.spacingM > kMaxSpacingM) {
        return fail(QStringLiteral("间距必须在 3..500m"));
    }
    if (!std::isfinite(config.vAngleDeg) || config.vAngleDeg < 0.0f || config.vAngleDeg > kMaxVAngleDeg) {
        return fail(QStringLiteral("V 字夹角必须在 0..90°"));
    }
    if (config.leaderOverride != 0 && config.leaderOverride < 0x10) {
        return fail(QStringLiteral("领机 node_id 非法"));
    }
    return true;
}

uint16_t nextFormationId(uint16_t current)
{
    if (current == 0 || current >= kFormationIdLimit) {
        return 1;
    }
    return static_cast<uint16_t>(current + 1U);
}

}  // namespace FormationProtocol
