//
// Created by jglrxavpok on 22/05/2021.
//

#include "Expressions.h"
#include "ExpressionVisitor.h"

namespace Carrot {
#define VISIT(Type) \
    std::any Type ## Expression::visit(BaseExpressionVisitor& visitor) {    \
        return visitor._visit ## Type(*this);                                \
    }

#define VISIT_T(Type) \
    template<> VISIT(Type)

    VISIT(Constant)
    VISIT(GetVariable);
    VISIT(SetVariable)
    VISIT(Compound);
    VISIT_T(Add);
    VISIT_T(Sub);
    VISIT_T(Mult);
    VISIT_T(Div);
    VISIT_T(Mod);
    VISIT_T(Min);
    VISIT_T(Max);

    VISIT_T(Less);
    VISIT_T(LessOrEquals);
    VISIT_T(Greater);
    VISIT_T(GreaterOrEquals);
    VISIT_T(Equals);
    VISIT_T(NotEquals);

    VISIT_T(Or);
    VISIT_T(And);
    VISIT_T(Xor);
    VISIT(BoolNegate);

    VISIT_T(Sin);
    VISIT_T(Cos);
    VISIT_T(Tan);
    VISIT_T(Exp);
    VISIT_T(Abs);
    VISIT_T(Sqrt);
    VISIT_T(Log);

    VISIT(Placeholder);
    VISIT(Once);
    VISIT(Prefixed);

#undef VISIT
#undef VISIT_T

    std::atomic<std::uint32_t> ExpressionType::ids = 0;
}