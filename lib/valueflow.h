/*
 * Cppcheck - A tool for static C/C++ code analysis
 * Copyright (C) 2007-2019 Cppcheck team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

//---------------------------------------------------------------------------
#ifndef valueflowH
#define valueflowH
//---------------------------------------------------------------------------

#include "config.h"
#include "utils.h"

#include <list>
#include <string>
#include <utility>

class ErrorLogger;
class Settings;
class SymbolDatabase;
class Token;
class TokenList;
class Variable;

namespace ValueFlow {
    class CPPCHECKLIB Value {
    public:
        typedef std::pair<const Token *, std::string> ErrorPathItem;
        typedef std::list<ErrorPathItem> ErrorPath;

        explicit Value(long long val = 0)
            : valueType(ValueType::INT),
              intvalue(val),
              tokvalue(nullptr),
              floatValue(0.0),
              moveKind(MoveKind::NonMovedVariable),
              varvalue(val),
              condition(nullptr),
              varId(0U),
              conditional(false),
              defaultArg(false),
              lifetimeKind(LifetimeKind::Object),
              lifetimeScope(LifetimeScope::Local),
              valueKind(ValueKind::Possible)
        {}
        Value(const Token *c, long long val);

        bool operator==(const Value &rhs) const {
            if (valueType != rhs.valueType)
                return false;
            switch (valueType) {
            case ValueType::INT:
                if (intvalue != rhs.intvalue)
                    return false;
                break;
            case ValueType::TOK:
                if (tokvalue != rhs.tokvalue)
                    return false;
                break;
            case ValueType::FLOAT:
                // TODO: Write some better comparison
                if (floatValue > rhs.floatValue || floatValue < rhs.floatValue)
                    return false;
                break;
            case ValueType::MOVED:
                if (moveKind != rhs.moveKind)
                    return false;
                break;
            case ValueType::UNINIT:
                break;
            case ValueType::BUFFER_SIZE:
                if (intvalue != rhs.intvalue)
                    return false;
                break;
            case ValueType::CONTAINER_SIZE:
                if (intvalue != rhs.intvalue)
                    return false;
                break;
            case ValueType::LIFETIME:
                if (tokvalue != rhs.tokvalue)
                    return false;
            }

            return varvalue == rhs.varvalue &&
                   condition == rhs.condition &&
                   varId == rhs.varId &&
                   conditional == rhs.conditional &&
                   defaultArg == rhs.defaultArg &&
                   valueKind == rhs.valueKind;
        }

        std::string infoString() const;

        enum ValueType { INT, TOK, FLOAT, MOVED, UNINIT, CONTAINER_SIZE, LIFETIME, BUFFER_SIZE } valueType;
        bool isIntValue() const {
            return valueType == ValueType::INT;
        }
        bool isTokValue() const {
            return valueType == ValueType::TOK;
        }
        bool isFloatValue() const {
            return valueType == ValueType::FLOAT;
        }
        bool isMovedValue() const {
            return valueType == ValueType::MOVED;
        }
        bool isUninitValue() const {
            return valueType == ValueType::UNINIT;
        }
        bool isContainerSizeValue() const {
            return valueType == ValueType::CONTAINER_SIZE;
        }
        bool isLifetimeValue() const {
            return valueType == ValueType::LIFETIME;
        }
        bool isBufferSizeValue() const {
            return valueType == ValueType::BUFFER_SIZE;
        }

        bool isLocalLifetimeValue() const {
            return valueType == ValueType::LIFETIME && lifetimeScope == LifetimeScope::Local;
        }

        bool isArgumentLifetimeValue() const {
            return valueType == ValueType::LIFETIME && lifetimeScope == LifetimeScope::Argument;
        }

        /** int value */
        long long intvalue;

        /** token value - the token that has the value. this is used for pointer aliases, strings, etc. */
        const Token *tokvalue;

        /** float value */
        double floatValue;

        /** kind of moved  */
        enum class MoveKind {NonMovedVariable, MovedVariable, ForwardedVariable} moveKind;

        /** For calculated values - variable value that calculated value depends on */
        long long varvalue;

        /** Condition that this value depends on */
        const Token *condition;

        ErrorPath errorPath;

        /** For calculated values - varId that calculated value depends on */
        nonneg int varId;

        /** Conditional value */
        bool conditional;

        /** Is this value passed as default parameter to the function? */
        bool defaultArg;

        enum class LifetimeKind {Object, Lambda, Iterator, Address} lifetimeKind;

        enum class LifetimeScope { Local, Argument } lifetimeScope;

        static const char * toString(MoveKind moveKind) {
            switch (moveKind) {
            case MoveKind::NonMovedVariable:
                return "NonMovedVariable";
            case MoveKind::MovedVariable:
                return "MovedVariable";
            case MoveKind::ForwardedVariable:
                return "ForwardedVariable";
            }
            return "";
        }

        /** How known is this value */
        enum class ValueKind {
            /** This value is possible, other unlisted values may also be possible */
            Possible,
            /** Only listed values are possible */
            Known,
            /** Inconclusive */
            Inconclusive
        } valueKind;

        void setKnown() {
            valueKind = ValueKind::Known;
        }

        bool isKnown() const {
            return valueKind == ValueKind::Known;
        }

        void setPossible() {
            valueKind = ValueKind::Possible;
        }

        bool isPossible() const {
            return valueKind == ValueKind::Possible;
        }

        void setInconclusive(bool inconclusive = true) {
            if (inconclusive)
                valueKind = ValueKind::Inconclusive;
        }

        bool isInconclusive() const {
            return valueKind == ValueKind::Inconclusive;
        }

        void changeKnownToPossible() {
            if (isKnown())
                valueKind = ValueKind::Possible;
        }

        bool errorSeverity() const {
            return !condition && !defaultArg;
        }
    };

    /// Constant folding of expression. This can be used before the full ValueFlow has been executed (ValueFlow::setValues).
    const ValueFlow::Value * valueFlowConstantFoldAST(Token *expr, const Settings *settings);

    /// Perform valueflow analysis.
    void setValues(TokenList *tokenlist, SymbolDatabase* symboldatabase, ErrorLogger *errorLogger, const Settings *settings);

    std::string eitherTheConditionIsRedundant(const Token *condition);
}

const Variable *getLifetimeVariable(const Token *tok, ValueFlow::Value::ErrorPath &errorPath);

bool isLifetimeBorrowed(const Token *tok, const Settings *settings);

std::string lifetimeType(const Token *tok, const ValueFlow::Value *val);

std::string lifetimeMessage(const Token *tok, const ValueFlow::Value *val, ValueFlow::Value::ErrorPath &errorPath);

ValueFlow::Value getLifetimeObjValue(const Token *tok);

#endif // valueflowH
