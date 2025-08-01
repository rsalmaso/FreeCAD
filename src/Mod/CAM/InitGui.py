# -*- coding: utf-8 -*-
# ***************************************************************************
# *   Copyright (c) 2014 Yorik van Havre <yorik@uncreated.net>              *
# *                                                                         *
# *   This file is part of the FreeCAD CAx development system.              *
# *                                                                         *
# *   This program is free software; you can redistribute it and/or modify  *
# *   it under the terms of the GNU Lesser General Public License (LGPL)    *
# *   as published by the Free Software Foundation; either version 2 of     *
# *   the License, or (at your option) any later version.                   *
# *   for detail see the LICENCE text file.                                 *
# *                                                                         *
# *   FreeCAD is distributed in the hope that it will be useful,            *
# *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
# *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
# *   GNU Lesser General Public License for more details.                   *
# *                                                                         *
# *   You should have received a copy of the GNU Library General Public     *
# *   License along with FreeCAD; if not, write to the Free Software        *
# *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  *
# *   USA                                                                   *
# *                                                                         *
# ***************************************************************************
import FreeCAD
from PySide.QtCore import QT_TRANSLATE_NOOP
import Path.Dressup.Gui.Preferences as PathPreferencesPathDressup
import Path.Tool.assets.ui.preferences as AssetPreferences
import Path.Main.Gui.PreferencesJob as PathPreferencesPathJob
import Path.Base.Gui.PreferencesAdvanced as PathPreferencesAdvanced
import Path.Op.Base
import Path.Tool


FreeCAD.__unit_test__ += ["TestCAMGui"]


if FreeCAD.GuiUp:
    import FreeCADGui

    FreeCADGui.addPreferencePage(
        PathPreferencesPathJob.JobPreferencesPage,
        QT_TRANSLATE_NOOP("QObject", "CAM"),
    )
    FreeCADGui.addPreferencePage(
        AssetPreferences.AssetPreferencesPage,
        QT_TRANSLATE_NOOP("QObject", "CAM"),
    )
    FreeCADGui.addPreferencePage(
        PathPreferencesPathDressup.DressupPreferencesPage,
        QT_TRANSLATE_NOOP("QObject", "CAM"),
    )
    FreeCADGui.addPreferencePage(
        PathPreferencesAdvanced.AdvancedPreferencesPage,
        QT_TRANSLATE_NOOP("QObject", "CAM"),
    )


class PathCommandGroup:
    def __init__(self, cmdlist, menu, tooltip=None):
        self.cmdlist = cmdlist
        self.menu = menu
        if tooltip is None:
            self.tooltip = menu
        else:
            self.tooltip = tooltip

    def GetCommands(self):
        return tuple(self.cmdlist)

    def GetResources(self):
        return {"MenuText": self.menu, "ToolTip": self.tooltip}

    def IsActive(self):
        if FreeCAD.ActiveDocument is not None:
            for o in FreeCAD.ActiveDocument.Objects:
                if o.Name[:3] == "Job":
                    return True
        return False


class CAMWorkbench(Workbench):
    "CAM workbench"

    def __init__(self):
        self.__class__.Icon = FreeCAD.getResourceDir() + "Mod/CAM/Resources/icons/CAMWorkbench.svg"
        self.__class__.MenuText = "CAM"
        self.__class__.ToolTip = "CAM workbench"

    def Initialize(self):
        global PathCommandGroup

        # Add preferences pages - before loading PathGui to properly order pages of Path group
        import Path.Dressup.Gui.Preferences as PathPreferencesPathDressup
        import Path.Tool.assets.ui.preferences as AssetPreferences
        import Path.Main.Gui.PreferencesJob as PathPreferencesPathJob

        # load the builtin modules
        import Path
        import PathScripts
        import PathGui

        FreeCADGui.addLanguagePath(":/translations")
        FreeCADGui.addIconPath(":/icons")
        import Path.GuiInit

        from Path.Main.Gui import JobCmd as PathJobCmd
        from Path.Main.Gui import SanityCmd as SanityCmd
        from Path.Tool.toolbit.ui import cmd as PathToolBitCmd
        from Path.Tool.library.ui import cmd as PathToolBitLibraryCmd

        from Path.Tool.camassets import cam_assets

        cam_assets.setup()

        from PySide.QtCore import QT_TRANSLATE_NOOP

        import PathCommands
        import subprocess
        from packaging.version import Version, parse

        Path.GuiInit.Startup()

        # build commands list
        projcmdlist = ["CAM_Job", "CAM_Post", "CAM_Sanity"]
        toolcmdlist = [
            "CAM_Inspect",
            "CAM_Simulator",
            "CAM_SimulatorGL",
            "CAM_SelectLoop",
            "CAM_OpActiveToggle",
        ]
        prepcmdlist = [
            "CAM_Fixture",
            "CAM_Comment",
            "CAM_Stop",
            "CAM_Custom",
            "CAM_Probe",
        ]
        twodopcmdlist = [
            "CAM_Profile",
            "CAM_Pocket_Shape",
            "CAM_MillFace",
            "CAM_Helix",
            "CAM_Adaptive",
            "CAM_Slot",
        ]
        threedopcmdlist = ["CAM_Pocket3D"]
        engravecmdlist = ["CAM_Engrave", "CAM_Deburr", "CAM_Vcarve"]
        drillingcmdlist = ["CAM_Drilling", "CAM_Tapping"]
        modcmdlist = ["CAM_OperationCopy", "CAM_Array", "CAM_SimpleCopy"]
        dressupcmdlist = [
            "CAM_DressupArray",
            "CAM_DressupAxisMap",
            "CAM_DressupPathBoundary",
            "CAM_DressupDogbone",
            "CAM_DressupDragKnife",
            "CAM_DressupLeadInOut",
            "CAM_DressupRampEntry",
            "CAM_DressupTag",
            "CAM_DressupZCorrect",
        ]
        extracmdlist = []
        specialcmdlist = []

        toolcmdlist.extend(PathToolBitLibraryCmd.BarList)
        toolbitcmdlist = PathToolBitLibraryCmd.MenuList

        engravecmdgroup = ["CAM_EngraveTools"]
        FreeCADGui.addCommand(
            "CAM_EngraveTools",
            PathCommandGroup(
                engravecmdlist,
                QT_TRANSLATE_NOOP("CAM_EngraveTools", "Engraving Operations"),
            ),
        )
        drillingcmdgroup = ["CAM_DrillingTools"]
        FreeCADGui.addCommand(
            "CAM_DrillingTools",
            PathCommandGroup(
                drillingcmdlist,
                QT_TRANSLATE_NOOP("CAM_DrillingTools", "Drilling Operations"),
            ),
        )
        dressupcmdgroup = ["CAM_DressupTools"]
        FreeCADGui.addCommand(
            "CAM_DressupTools",
            PathCommandGroup(
                dressupcmdlist,
                QT_TRANSLATE_NOOP("CAM_DressupTools", "Dressup Operations"),
            ),
        )
        threedcmdgroup = threedopcmdlist
        if Path.Preferences.experimentalFeaturesEnabled():
            prepcmdlist.append("CAM_PathShapeTC")
            extracmdlist.extend(["CAM_Area", "CAM_Area_Workplane"])
            specialcmdlist.append("CAM_ThreadMilling")

        if Path.Preferences.advancedOCLFeaturesEnabled():
            try:
                r = subprocess.run(
                    ["camotics", "--version"], capture_output=True, text=True
                ).stderr.strip()
                v = parse(r)

                if v >= Version("1.2.2"):
                    toolcmdlist.append("CAM_Camotics")
            except (FileNotFoundError, ModuleNotFoundError):
                pass
            except subprocess.CalledProcessError as e:
                print(f"Failed to execute camotics command: {e}")
            except ValueError as ve:
                print(f"Version error: {ve}")
            except Exception as ex:
                print(f"An unexpected error occurred: {ex}")

            try:
                try:
                    import ocl  # pylint: disable=unused-variable
                except ImportError:
                    import opencamlib as ocl
                from Path.Op.Gui import Surface
                from Path.Op.Gui import Waterline

                threedopcmdlist.extend(["CAM_Surface", "CAM_Waterline"])
                threedcmdgroup = ["CAM_3dTools"]
                FreeCADGui.addCommand(
                    "CAM_3dTools",
                    PathCommandGroup(
                        threedopcmdlist,
                        QT_TRANSLATE_NOOP("CAM_3dTools", "3D Operations"),
                    ),
                )
            except ImportError:
                if not Path.Preferences.suppressOpenCamLibWarning():
                    FreeCAD.Console.PrintError("OpenCamLib is not working!\n")

        self.appendToolbar(QT_TRANSLATE_NOOP("Workbench", "Project Setup"), projcmdlist)
        self.appendToolbar(QT_TRANSLATE_NOOP("Workbench", "Tool Commands"), toolcmdlist)
        self.appendToolbar(
            QT_TRANSLATE_NOOP("Workbench", "New Operations"),
            twodopcmdlist + drillingcmdgroup + engravecmdgroup + threedcmdgroup,
        )
        self.appendToolbar(
            QT_TRANSLATE_NOOP("Workbench", "Path Modification"), modcmdlist + dressupcmdgroup
        )
        if extracmdlist:
            self.appendToolbar(QT_TRANSLATE_NOOP("Workbench", "Helpful Tools"), extracmdlist)

        self.appendMenu(
            [QT_TRANSLATE_NOOP("Workbench", "&CAM")],
            projcmdlist
            + ["CAM_ExportTemplate", "Separator"]
            + toolcmdlist
            + toolbitcmdlist
            + ["Separator"]
            + twodopcmdlist
            + drillingcmdlist
            + engravecmdlist
            + ["Separator"]
            + threedopcmdlist
            + ["Separator"],
        )
        self.appendMenu(
            [
                QT_TRANSLATE_NOOP("Workbench", "&CAM"),
                QT_TRANSLATE_NOOP("Workbench", "Path Dressup"),
            ],
            dressupcmdlist,
        )
        self.appendMenu(
            [
                QT_TRANSLATE_NOOP("Workbench", "&CAM"),
                QT_TRANSLATE_NOOP("Workbench", "Supplemental Commands"),
            ],
            prepcmdlist,
        )
        self.appendMenu(
            [
                QT_TRANSLATE_NOOP("Workbench", "&CAM"),
                QT_TRANSLATE_NOOP("Workbench", "Path Modification"),
            ],
            modcmdlist,
        )
        if specialcmdlist:
            self.appendMenu(
                [
                    QT_TRANSLATE_NOOP("Workbench", "&CAM"),
                    QT_TRANSLATE_NOOP("Workbench", "Specialty Operations"),
                ],
                specialcmdlist,
            )
        if extracmdlist:
            self.appendMenu([QT_TRANSLATE_NOOP("Workbench", "&CAM")], extracmdlist)

        self.appendMenu([QT_TRANSLATE_NOOP("Workbench", "&CAM")], ["Separator"])
        self.appendMenu(
            [
                QT_TRANSLATE_NOOP("Workbench", "&CAM"),
                QT_TRANSLATE_NOOP("Workbench", "Utils"),
            ],
            ["CAM_PropertyBag"],
        )

        self.dressupcmds = dressupcmdlist

        curveAccuracy = Path.Preferences.defaultLibAreaCurveAccuracy()
        if curveAccuracy:
            Path.Area.setDefaultParams(Accuracy=curveAccuracy)

        Log("Loading CAM workbench... done\n")

    def GetClassName(self):
        return "Gui::PythonWorkbench"

    def Activated(self):
        # update the translation engine
        FreeCADGui.updateLocale()
        # Msg("CAM workbench activated\n")

    def Deactivated(self):
        # Msg("CAM workbench deactivated\n")
        pass

    def ContextMenu(self, recipient):
        menuAppended = False
        if len(FreeCADGui.Selection.getSelection()) == 1:
            obj = FreeCADGui.Selection.getSelection()[0]
            if obj.isDerivedFrom("Path::Feature"):
                self.appendContextMenu("", "Separator")
                self.appendContextMenu("", ["CAM_Inspect"])
                selectedName = obj.Name
                if "Remote" in selectedName:
                    self.appendContextMenu("", ["Refresh_Path"])
                if "Job" in selectedName:
                    self.appendContextMenu("", ["CAM_ExportTemplate"] + self.toolbitctxmenu)
                menuAppended = True
            if isinstance(obj.Proxy, Path.Op.Base.ObjectOp):
                self.appendContextMenu("", ["CAM_OperationCopy", "CAM_OpActiveToggle"])
                if hasattr(obj, "StartPoint"):
                    self.appendContextMenu("", ["CAM_SetStartPoint"])
                menuAppended = True
            if obj.isDerivedFrom("Path::Feature"):
                if (
                    "Profile" in selectedName
                    or "Contour" in selectedName
                    or "Dressup" in selectedName
                ):
                    self.appendContextMenu("", "Separator")
                    # self.appendContextMenu("", ["Set_StartPoint"])
                    # self.appendContextMenu("", ["Set_EndPoint"])
                    for cmd in self.dressupcmds:
                        self.appendContextMenu("Dressup", [cmd])
                    menuAppended = True
            if isinstance(obj.Proxy, Path.Tool.ToolBit):
                self.appendContextMenu("", ["CAM_ToolBitSave", "CAM_ToolBitSaveAs"])
                menuAppended = True
        if menuAppended:
            self.appendContextMenu("", "Separator")


Gui.addWorkbench(CAMWorkbench())

FreeCAD.addImportType(
    "GCode (*.nc *.NC *.gc *.GC *.ncc *.NCC *.ngc *.NGC *.cnc *.CNC *.tap *.TAP *.gcode *.GCODE)",
    "PathGui",
)
