/***************************************************************************
 *   Copyright (c) 2009 Jürgen Riegel <FreeCAD@juergen-riegel.net>         *
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

#ifndef BASE_UNITSSCHEMAINTERNAL_H
#define BASE_UNITSSCHEMAINTERNAL_H

#include "UnitsSchema.h"

namespace Base
{

/** The standard units schema
 *  Here is defined what internal (base) units FreeCAD uses.
 *  FreeCAD uses a mm/kg/deg scala.
 *  Also it defines how the units get presented.
 */
class UnitsSchemaInternal: public UnitsSchema
{
public:
    std::string
    schemaTranslate(const Base::Quantity& quant, double& factor, std::string& unitString) override;
};

}  // namespace Base

#endif  // BASE_UNITSSCHEMAINTERNAL_H
