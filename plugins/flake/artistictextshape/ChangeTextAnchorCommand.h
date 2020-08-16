/* This file is part of the KDE project
 * Copyright (C) 2007 Jan Hambrecht <jaham@gmx.net>
 * Copyright (C) 2008 Rob Buis <buis@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef CHANGETEXTANCHORCOMMAND_H
#define CHANGETEXTANCHORCOMMAND_H

#include "ArtisticTextShape.h"
#include <kundo2command.h>

class ChangeTextAnchorCommand : public KUndo2Command
{
public:
    ChangeTextAnchorCommand(ArtisticTextShape *shape, ArtisticTextShape::TextAnchor anchor);
    void undo() override;
    void redo() override;
private:
    ArtisticTextShape *m_shape;
    ArtisticTextShape::TextAnchor m_anchor;
    ArtisticTextShape::TextAnchor m_oldAnchor;
};

#endif // CHANGETEXTANCHORCOMMAND_H