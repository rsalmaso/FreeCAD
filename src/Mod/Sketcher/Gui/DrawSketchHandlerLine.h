/***************************************************************************
 *   Copyright (c) 2022 Abdullah Tahiri <abdullah.tahiri.yo@gmail.com>     *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/


#ifndef SKETCHERGUI_DrawSketchHandlerLine_H
#define SKETCHERGUI_DrawSketchHandlerLine_H

#include <Gui/BitmapFactory.h>
#include <Gui/Notifications.h>
#include <Gui/Command.h>
#include <Gui/CommandT.h>
#include <Gui/InputHint.h>

#include <Mod/Sketcher/App/SketchObject.h>

#include "DrawSketchDefaultWidgetController.h"
#include "DrawSketchControllableHandler.h"

#include "GeometryCreationMode.h"
#include "Utils.h"

#include <vector>
#include <algorithm>

namespace SketcherGui
{

extern GeometryCreationMode geometryCreationMode;  // defined in CommandCreateGeo.cpp

class DrawSketchHandlerLine;

namespace ConstructionMethods
{

enum class LineConstructionMethod
{
    OnePointLengthAngle,
    OnePointWidthHeight,
    TwoPoints,
    End  // Must be the last one
};

}

using DSHLineController =
    DrawSketchDefaultWidgetController<DrawSketchHandlerLine,
                                      /*SelectModeT*/ StateMachines::TwoSeekEnd,
                                      /*PAutoConstraintSize =*/2,
                                      /*OnViewParametersT =*/OnViewParameters<4, 4, 4>,  // NOLINT
                                      /*WidgetParametersT =*/WidgetParameters<0, 0, 0>,  // NOLINT
                                      /*WidgetCheckboxesT =*/WidgetCheckboxes<0, 0, 0>,  // NOLINT
                                      /*WidgetComboboxesT =*/WidgetComboboxes<1, 1, 1>,  // NOLINT
                                      ConstructionMethods::LineConstructionMethod,
                                      /*bool PFirstComboboxIsConstructionMethod =*/true>;

using DSHLineControllerBase = DSHLineController::ControllerBase;

using DrawSketchHandlerLineBase = DrawSketchControllableHandler<DSHLineController>;


class DrawSketchHandlerLine: public DrawSketchHandlerLineBase
{
    friend DSHLineController;
    friend DSHLineControllerBase;

public:
    explicit DrawSketchHandlerLine(
        ConstructionMethod constrMethod = ConstructionMethod::OnePointLengthAngle)
        : DrawSketchHandlerLineBase(constrMethod)
        , length(0.0) {};
    ~DrawSketchHandlerLine() override = default;

private:
    void updateDataAndDrawToPosition(Base::Vector2d onSketchPos) override
    {
        switch (state()) {
            case SelectMode::SeekFirst: {
                toolWidgetManager.drawPositionAtCursor(onSketchPos);

                startPoint = onSketchPos;

                seekAndRenderAutoConstraint(sugConstraints[0],
                                            onSketchPos,
                                            Base::Vector2d(0.f, 0.f));
            } break;
            case SelectMode::SeekSecond: {
                toolWidgetManager.drawDirectionAtCursor(onSketchPos, startPoint);

                endPoint = onSketchPos;

                try {
                    CreateAndDrawShapeGeometry();
                }
                catch (const Base::ValueError&) {
                }  // equal points while hovering raise an objection that can be safely ignored

                seekAndRenderAutoConstraint(sugConstraints[1],
                                            onSketchPos,
                                            onSketchPos - startPoint);
            } break;
            default:
                break;
        }
    }

    void executeCommands() override
    {
        try {
            createShape(false);

            Gui::Command::openCommand(QT_TRANSLATE_NOOP("Command", "Add sketch line"));

            commandAddShapeGeometryAndConstraints();

            Gui::Command::commitCommand();
        }
        catch (const Base::Exception&) {
            Gui::NotifyError(sketchgui,
                             QT_TRANSLATE_NOOP("Notifications", "Error"),
                             QT_TRANSLATE_NOOP("Notifications", "Failed to add line"));

            Gui::Command::abortCommand();
            THROWM(Base::RuntimeError,
                   QT_TRANSLATE_NOOP(
                       "Notifications",
                       "Tool execution aborted") "\n")  // This prevents constraints from being
                                                        // applied on non existing geometry
        }
    }

    void generateAutoConstraints() override
    {
        int LineGeoId = getHighestCurveIndex();

        // Generate temporary autoconstraints (but do not actually add them to the sketch)
        if (avoidRedundants) {
            removeRedundantHorizontalVertical(getSketchObject(),
                                              sugConstraints[0],
                                              sugConstraints[1]);
        }

        auto& ac1 = sugConstraints[0];
        auto& ac2 = sugConstraints[1];

        generateAutoConstraintsOnElement(ac1, LineGeoId, Sketcher::PointPos::start);
        generateAutoConstraintsOnElement(ac2, LineGeoId, Sketcher::PointPos::end);

        // Ensure temporary autoconstraints do not generate a redundancy and that the geometry
        // parameters are accurate This is particularly important for adding widget mandated
        // constraints.
        removeRedundantAutoConstraints();
    }

    void createAutoConstraints() override
    {
        // execute python command to create autoconstraints
        createGeneratedAutoConstraints(true);

        sugConstraints[0].clear();
        sugConstraints[1].clear();
    }

    std::string getToolName() const override
    {
        return "DSH_Line";
    }

    QString getCrosshairCursorSVGName() const override
    {
        if (constructionMethod() == ConstructionMethod::OnePointLengthAngle) {
            return QStringLiteral("Sketcher_Pointer_Create_Line_Polar");
        }
        else {
            return QStringLiteral("Sketcher_Pointer_Create_Line.svg");
        }
    }

    std::unique_ptr<QWidget> createWidget() const override
    {
        return std::make_unique<SketcherToolDefaultWidget>();
    }

    bool isWidgetVisible() const override
    {
        ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath(
            "User parameter:BaseApp/Preferences/Mod/Sketcher/Tools");
        auto index = hGrp->GetInt("OnViewParameterVisibility", 1);
        return index != 0;
    };

    QPixmap getToolIcon() const override
    {
        return Gui::BitmapFactory().pixmap("Sketcher_CreateLine");
    }

    QString getToolWidgetText() const override
    {
        return QString(tr("Line parameters"));
    }

    bool canGoToNextMode() override
    {
        if (state() == SelectMode::SeekSecond && length < Precision::Confusion()) {
            // Prevent validation of null line.
            return false;
        }
        return true;
    }

    void angleSnappingControl() override
    {
        if (state() == SelectMode::SeekSecond) {
            setAngleSnapping(true, startPoint);
        }

        else {
            setAngleSnapping(false);
        }
    }

private:
    Base::Vector2d startPoint, endPoint;
    double length;

    void createShape(bool onlyeditoutline) override
    {
        Q_UNUSED(onlyeditoutline);
        ShapeGeometry.clear();

        Base::Vector2d vecL = endPoint - startPoint;
        length = vecL.Length();
        if (length > Precision::Confusion()) {

            addLineToShapeGeometry(toVector3d(startPoint),
                                   toVector3d(endPoint),
                                   isConstructionMode());
        }
    }

    std::list<Gui::InputHint> getToolHints() const override
    {
        using State = std::pair<ConstructionMethod, SelectMode>;
        using enum Gui::InputHint::UserInput;

        const Gui::InputHint switchModeHint {tr("%1 switch mode"), {KeyM}};

        return Gui::lookupHints<State>(
            {constructionMethod(), state()},
            {
                // OnePointLengthAngle method
                {.state = {ConstructionMethod::OnePointLengthAngle, SelectMode::SeekFirst},
                 .hints =
                     {
                         {tr("%1 pick first point"), {MouseLeft}},
                         switchModeHint,
                     }},
                {.state = {ConstructionMethod::OnePointLengthAngle, SelectMode::SeekSecond},
                 .hints =
                     {
                         {tr("%1 pick second point"), {MouseLeft}},
                         switchModeHint,
                     }},

                // OnePointWidthHeight method
                {.state = {ConstructionMethod::OnePointWidthHeight, SelectMode::SeekFirst},
                 .hints =
                     {
                         {tr("%1 pick first point"), {MouseLeft}},
                         switchModeHint,
                     }},
                {.state = {ConstructionMethod::OnePointWidthHeight, SelectMode::SeekSecond},
                 .hints =
                     {
                         {tr("%1 pick second point"), {MouseLeft}},
                         switchModeHint,
                     }},

                // TwoPoints method
                {.state = {ConstructionMethod::TwoPoints, SelectMode::SeekFirst},
                 .hints =
                     {
                         {tr("%1 pick first point"), {MouseLeft}},
                         switchModeHint,
                     }},
                {.state = {ConstructionMethod::TwoPoints, SelectMode::SeekSecond},
                 .hints =
                     {
                         {tr("%1 pick second point"), {MouseLeft}},
                         switchModeHint,
                     }},
            });
    }
};

template<>
auto DSHLineControllerBase::getState(int labelindex) const
{
    switch (labelindex) {
        case OnViewParameter::First:
        case OnViewParameter::Second:
            return SelectMode::SeekFirst;
            break;
        case OnViewParameter::Third:
        case OnViewParameter::Fourth:
            return SelectMode::SeekSecond;
            break;
        default:
            THROWM(Base::ValueError, "Label index without an associated machine state")
    }
}

template<>
void DSHLineController::configureToolWidget()
{
    if (!init) {  // Code to be executed only upon initialisation
        QStringList names = {QApplication::translate("Sketcher_CreateLine", "Point, length, angle"),
                             QApplication::translate("Sketcher_CreateLine", "Point, width, height"),
                             QApplication::translate("Sketcher_CreateLine", "2 points")};
        toolWidget->setComboboxElements(WCombobox::FirstCombo, names);

        if (isConstructionMode()) {
            toolWidget->setComboboxItemIcon(
                WCombobox::FirstCombo,
                0,
                Gui::BitmapFactory().iconFromTheme("Sketcher_CreateLineAngleLength_Constr"));
            toolWidget->setComboboxItemIcon(
                WCombobox::FirstCombo,
                1,
                Gui::BitmapFactory().iconFromTheme("Sketcher_CreateLineLengthWidth_Constr"));
            toolWidget->setComboboxItemIcon(
                WCombobox::FirstCombo,
                2,
                Gui::BitmapFactory().iconFromTheme("Sketcher_CreateLine_Constr"));
        }
        else {
            toolWidget->setComboboxItemIcon(
                WCombobox::FirstCombo,
                0,
                Gui::BitmapFactory().iconFromTheme("Sketcher_CreateLineAngleLength"));
            toolWidget->setComboboxItemIcon(
                WCombobox::FirstCombo,
                1,
                Gui::BitmapFactory().iconFromTheme("Sketcher_CreateLineLengthWidth"));
            toolWidget->setComboboxItemIcon(
                WCombobox::FirstCombo,
                2,
                Gui::BitmapFactory().iconFromTheme("Sketcher_CreateLine"));
        }
    }

    onViewParameters[OnViewParameter::First]->setLabelType(Gui::SoDatumLabel::DISTANCEX);
    onViewParameters[OnViewParameter::Second]->setLabelType(Gui::SoDatumLabel::DISTANCEY);

    if (handler->constructionMethod() == ConstructionMethod::OnePointLengthAngle) {
        onViewParameters[OnViewParameter::Third]->setLabelType(
            Gui::SoDatumLabel::DISTANCE,
            Gui::EditableDatumLabel::Function::Dimensioning);
        onViewParameters[OnViewParameter::Fourth]->setLabelType(
            Gui::SoDatumLabel::ANGLE,
            Gui::EditableDatumLabel::Function::Dimensioning);
    }
    else if (handler->constructionMethod() == ConstructionMethod::TwoPoints) {
        onViewParameters[OnViewParameter::Third]->setLabelType(
            Gui::SoDatumLabel::DISTANCEX,
            Gui::EditableDatumLabel::Function::Positioning);
        onViewParameters[OnViewParameter::Fourth]->setLabelType(
            Gui::SoDatumLabel::DISTANCEY,
            Gui::EditableDatumLabel::Function::Positioning);
    }
    else {
        onViewParameters[OnViewParameter::Third]->setLabelType(
            Gui::SoDatumLabel::DISTANCEX,
            Gui::EditableDatumLabel::Function::Dimensioning);
        onViewParameters[OnViewParameter::Fourth]->setLabelType(
            Gui::SoDatumLabel::DISTANCEY,
            Gui::EditableDatumLabel::Function::Dimensioning);
    }
}


template<>
void DSHLineControllerBase::doEnforceControlParameters(Base::Vector2d& onSketchPos)
{
    switch (handler->state()) {
        case SelectMode::SeekFirst: {
            auto& firstParam = onViewParameters[OnViewParameter::First];
            auto& secondParam = onViewParameters[OnViewParameter::Second];

            if (firstParam->isSet) {
                onSketchPos.x = firstParam->getValue();
            }

            if (secondParam->isSet) {
                onSketchPos.y = secondParam->getValue();
            }
        } break;
        case SelectMode::SeekSecond: {
            auto& thirdParam = onViewParameters[OnViewParameter::Third];
            auto& fourthParam = onViewParameters[OnViewParameter::Fourth];

            if (handler->constructionMethod() == ConstructionMethod::OnePointWidthHeight) {
                if (thirdParam->isSet) {
                    double length = thirdParam->getValue();
                    if (fabs(length) < Precision::Confusion()) {
                        // Both cannot be 0
                        if (fourthParam->isSet) {
                            double width = fourthParam->getValue();
                            if (fabs(width) < Precision::Confusion()) {
                                unsetOnViewParameter(thirdParam.get());
                                return;
                            }
                        }
                    }
                    int sign = (onSketchPos.x - handler->startPoint.x) >= 0 ? 1 : -1;
                    onSketchPos.x = handler->startPoint.x + sign * length;
                }

                if (fourthParam->isSet) {
                    double width = fourthParam->getValue();
                    if (fabs(width) < Precision::Confusion()) {
                        // Both cannot be 0
                        if (thirdParam->isSet) {
                            double length = thirdParam->getValue();
                            if (fabs(length) < Precision::Confusion()) {
                                unsetOnViewParameter(fourthParam.get());
                                return;
                            }
                        }
                    }
                    int sign = (onSketchPos.y - handler->startPoint.y) >= 0 ? 1 : -1;
                    onSketchPos.y = handler->startPoint.y + sign * width;
                }
            }
            else if (handler->constructionMethod() == ConstructionMethod::OnePointLengthAngle) {

                Base::Vector2d dir = onSketchPos - handler->startPoint;
                if (dir.Length() < Precision::Confusion()) {
                    dir.x = 1.0;  // if direction null, default to (1,0)
                }
                double length = dir.Length();

                if (thirdParam->isSet) {
                    length = thirdParam->getValue();
                    if (length < Precision::Confusion()) {
                        unsetOnViewParameter(thirdParam.get());
                        return;
                    }

                    onSketchPos = handler->startPoint + length * dir.Normalize();
                }

                if (fourthParam->isSet) {
                    double angle = Base::toRadians(fourthParam->getValue());
                    Base::Vector2d ovpDir(cos(angle), sin(angle));
                    onSketchPos.ProjectToLine(onSketchPos - handler->startPoint, ovpDir);
                    onSketchPos += handler->startPoint;
                }
            }
            else {
                if (thirdParam->isSet) {
                    onSketchPos.x = thirdParam->getValue();
                }
                if (fourthParam->isSet) {
                    onSketchPos.y = fourthParam->getValue();
                }
            }

            if (thirdParam->isSet && fourthParam->isSet
                && (onSketchPos - handler->startPoint).Length() < Precision::Confusion()) {
                unsetOnViewParameter(thirdParam.get());
                unsetOnViewParameter(fourthParam.get());
            }
        } break;
        default:
            break;
    }
}

template<>
void DSHLineController::adaptParameters(Base::Vector2d onSketchPos)
{
    switch (handler->state()) {
        case SelectMode::SeekFirst: {
            auto& firstParam = onViewParameters[OnViewParameter::First];
            auto& secondParam = onViewParameters[OnViewParameter::Second];

            if (!firstParam->isSet) {
                setOnViewParameterValue(OnViewParameter::First, onSketchPos.x);
            }

            if (!secondParam->isSet) {
                setOnViewParameterValue(OnViewParameter::Second, onSketchPos.y);
            }

            bool sameSign = onSketchPos.x * onSketchPos.y > 0.;
            firstParam->setLabelAutoDistanceReverse(!sameSign);
            secondParam->setLabelAutoDistanceReverse(sameSign);
            firstParam->setPoints(Base::Vector3d(), toVector3d(onSketchPos));
            secondParam->setPoints(Base::Vector3d(), toVector3d(onSketchPos));
        } break;
        case SelectMode::SeekSecond: {
            auto& thirdParam = onViewParameters[OnViewParameter::Third];
            auto& fourthParam = onViewParameters[OnViewParameter::Fourth];

            if (handler->constructionMethod() == ConstructionMethod::OnePointWidthHeight) {
                Base::Vector3d start = toVector3d(handler->startPoint);
                Base::Vector3d end = toVector3d(handler->endPoint);
                Base::Vector3d vec = end - start;

                if (!thirdParam->isSet) {
                    setOnViewParameterValue(OnViewParameter::Third, fabs(vec.x));
                }

                if (!fourthParam->isSet) {
                    setOnViewParameterValue(OnViewParameter::Fourth, fabs(vec.y));
                }

                bool sameSign = vec.x * vec.y > 0.;

                thirdParam->setLabelAutoDistanceReverse(!sameSign);
                fourthParam->setLabelAutoDistanceReverse(sameSign);

                thirdParam->setPoints(start, end);
                fourthParam->setPoints(start, end);
            }
            else if (handler->constructionMethod() == ConstructionMethod::OnePointLengthAngle) {
                Base::Vector3d start = toVector3d(handler->startPoint);
                Base::Vector3d end = toVector3d(handler->endPoint);
                Base::Vector3d vec = end - start;

                if (!thirdParam->isSet) {
                    setOnViewParameterValue(OnViewParameter::Third, vec.Length());
                }

                double range = (handler->endPoint - handler->startPoint).Angle();
                if (!fourthParam->isSet) {
                    setOnViewParameterValue(OnViewParameter::Fourth,
                                            Base::toDegrees(range),
                                            Base::Unit::Angle);
                }
                else if (vec.Length() > Precision::Confusion()) {
                    double ovpRange = Base::toRadians(fourthParam->getValue());
                    if (fabs(range - ovpRange) > Precision::Confusion()) {
                        setOnViewParameterValue(OnViewParameter::Fourth,
                                                Base::toDegrees(range),
                                                Base::Unit::Angle);
                    }
                }

                thirdParam->setPoints(start, end);
                fourthParam->setPoints(start, Base::Vector3d());
                fourthParam->setLabelRange(range);
            }
            else {
                if (!thirdParam->isSet) {
                    setOnViewParameterValue(OnViewParameter::Third, onSketchPos.x);
                }

                if (!fourthParam->isSet) {
                    setOnViewParameterValue(OnViewParameter::Fourth, onSketchPos.y);
                }

                bool sameSign = onSketchPos.x * onSketchPos.y > 0.;
                thirdParam->setLabelAutoDistanceReverse(!sameSign);
                fourthParam->setLabelAutoDistanceReverse(sameSign);
                thirdParam->setPoints(Base::Vector3d(), toVector3d(onSketchPos));
                fourthParam->setPoints(Base::Vector3d(), toVector3d(onSketchPos));
            }
        } break;
        default:
            break;
    }
}

template<>
void DSHLineController::doChangeDrawSketchHandlerMode()
{
    switch (handler->state()) {
        case SelectMode::SeekFirst: {
            auto& firstParam = onViewParameters[OnViewParameter::First];
            auto& secondParam = onViewParameters[OnViewParameter::Second];

            if (firstParam->hasFinishedEditing && secondParam->hasFinishedEditing) {
                handler->setState(SelectMode::SeekSecond);
            }
        } break;
        case SelectMode::SeekSecond: {
            auto& thirdParam = onViewParameters[OnViewParameter::Third];
            auto& fourthParam = onViewParameters[OnViewParameter::Fourth];

            if (thirdParam->hasFinishedEditing && fourthParam->hasFinishedEditing) {
                handler->setState(SelectMode::End);
            }
        } break;
        default:
            break;
    }
}

// Function responsible to add widget mandated constraints (it is executed before creating
// autoconstraints)
template<>
void DSHLineController::addConstraints()
{
    App::DocumentObject* obj = handler->sketchgui->getObject();

    int firstCurve = handler->getHighestCurveIndex();

    auto x0 = onViewParameters[OnViewParameter::First]->getValue();
    auto y0 = onViewParameters[OnViewParameter::Second]->getValue();
    auto p3 = onViewParameters[OnViewParameter::Third]->getValue();
    auto p4 = onViewParameters[OnViewParameter::Fourth]->getValue();

    auto x0set = onViewParameters[OnViewParameter::First]->isSet;
    auto y0set = onViewParameters[OnViewParameter::Second]->isSet;
    auto p3set = onViewParameters[OnViewParameter::Third]->isSet;
    auto p4set = onViewParameters[OnViewParameter::Fourth]->isSet;

    using namespace Sketcher;

    auto constraintToOrigin = [&]() {
        ConstraintToAttachment(GeoElementId(firstCurve, PointPos::start),
                               GeoElementId::RtPnt,
                               x0,
                               obj);
    };

    auto constraintx0 = [&]() {
        ConstraintToAttachment(GeoElementId(firstCurve, PointPos::start),
                               GeoElementId::VAxis,
                               x0,
                               obj);
    };

    auto constrainty0 = [&]() {
        ConstraintToAttachment(GeoElementId(firstCurve, PointPos::start),
                               GeoElementId::HAxis,
                               y0,
                               obj);
    };

    auto constraintp3DistanceX = [&]() {
        if (fabs(p3) < Precision::Confusion()) {
            Gui::cmdAppObjectArgs(obj,
                                  "addConstraint(Sketcher.Constraint('Vertical',%d)) ",
                                  firstCurve);
        }
        else {
            bool reverse = (handler->endPoint.x - handler->startPoint.x) < 0;
            Gui::cmdAppObjectArgs(obj,
                                  "addConstraint(Sketcher.Constraint('DistanceX',%d,%d,%d,%d,%f)) ",
                                  firstCurve,
                                  reverse ? 2 : 1,
                                  firstCurve,
                                  reverse ? 1 : 2,
                                  fabs(p3));
        }
    };

    auto constraintp3length = [&]() {
        Gui::cmdAppObjectArgs(obj,
                              "addConstraint(Sketcher.Constraint('Distance',%d,%f)) ",
                              firstCurve,
                              fabs(p3));
    };

    auto constraintp3x = [&]() {
        ConstraintToAttachment(GeoElementId(firstCurve, PointPos::end),
                               GeoElementId::VAxis,
                               p3,
                               obj);
    };

    auto constraintp4DistanceY = [&]() {
        if (fabs(p4) < Precision::Confusion()) {
            Gui::cmdAppObjectArgs(obj,
                                  "addConstraint(Sketcher.Constraint('Horizontal',%d)) ",
                                  firstCurve);
        }
        else {
            bool reverse = (handler->endPoint.y - handler->startPoint.y) < 0;
            Gui::cmdAppObjectArgs(obj,
                                  "addConstraint(Sketcher.Constraint('DistanceY',%d,%d,%d,%d,%f)) ",
                                  firstCurve,
                                  reverse ? 2 : 1,
                                  firstCurve,
                                  reverse ? 1 : 2,
                                  fabs(p4));
        }
    };

    auto constraintp4angle = [&]() {
        ConstraintLineByAngle(firstCurve, Base::toRadians(p4), obj);
    };

    auto constraintp4y = [&]() {
        ConstraintToAttachment(GeoElementId(firstCurve, PointPos::end),
                               GeoElementId::HAxis,
                               p4,
                               obj);
    };

    if (handler->AutoConstraints.empty()) {  // No valid diagnosis. Every constraint can be added.

        if (x0set && y0set && x0 == 0. && y0 == 0.) {
            constraintToOrigin();
        }
        else {
            if (x0set) {
                constraintx0();
            }

            if (y0set) {
                constrainty0();
            }
        }

        if (handler->constructionMethod()
            == DrawSketchHandlerLine::ConstructionMethod::OnePointWidthHeight) {
            if (p3set) {
                constraintp3DistanceX();
            }

            if (p4set) {
                constraintp4DistanceY();
            }
        }
        else if (handler->constructionMethod()
                 == DrawSketchHandlerLine::ConstructionMethod::OnePointLengthAngle) {
            if (p3set) {
                constraintp3length();
            }

            if (p4set) {
                constraintp4angle();
            }
        }
        else {
            if (p3set) {
                constraintp3x();
            }

            if (p4set) {
                constraintp4y();
            }
        }
    }
    else {  // Valid diagnosis. Must check which constraints may be added.
        auto startpointinfo = handler->getPointInfo(GeoElementId(firstCurve, PointPos::start));

        if (x0set && startpointinfo.isXDoF()) {
            constraintx0();

            handler->diagnoseWithAutoConstraints();  // ensure we have recalculated parameters after
                                                     // each constraint addition

            startpointinfo = handler->getPointInfo(
                GeoElementId(firstCurve, PointPos::start));  // get updated point position
        }

        if (y0set && startpointinfo.isYDoF()) {
            constrainty0();

            handler->diagnoseWithAutoConstraints();  // ensure we have recalculated parameters after
                                                     // each constraint addition

            startpointinfo = handler->getPointInfo(
                GeoElementId(firstCurve, PointPos::start));  // get updated point position
        }

        auto endpointinfo = handler->getPointInfo(GeoElementId(firstCurve, PointPos::end));

        if (handler->constructionMethod()
            == DrawSketchHandlerLine::ConstructionMethod::OnePointWidthHeight) {

            int DoFs = startpointinfo.getDoFs();
            DoFs += endpointinfo.getDoFs();

            if (p3set && DoFs > 0) {
                constraintp3DistanceX();
                DoFs--;
            }

            if (p4set && DoFs > 0) {
                constraintp4DistanceY();
            }
        }
        else if (handler->constructionMethod()
                 == DrawSketchHandlerLine::ConstructionMethod::OnePointLengthAngle) {

            int DoFs = startpointinfo.getDoFs();
            DoFs += endpointinfo.getDoFs();

            if (p3set && DoFs > 0) {
                constraintp3length();
                DoFs--;
            }

            if (p4set && DoFs > 0) {
                constraintp4angle();
            }
        }
        else {
            if (p3set && endpointinfo.isXDoF()) {
                constraintp3x();

                handler->diagnoseWithAutoConstraints();  // ensure we have recalculated parameters
                                                         // after each constraint addition

                startpointinfo = handler->getPointInfo(
                    GeoElementId(firstCurve, PointPos::start));  // get updated point position
                endpointinfo = handler->getPointInfo(GeoElementId(firstCurve, PointPos::end));
            }

            if (p4set && endpointinfo.isYDoF()) {
                constraintp4y();
            }
        }
    }
}

}  // namespace SketcherGui


#endif  // SKETCHERGUI_DrawSketchHandlerLine_H
