/* This file is part of the KDE project

   Copyright (C) 2006 Thorsten Zachmann <zachmann@kde.org>
   Copyright (C) 2006-2007 Thomas Zander <zander@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
*/

#include "KoInteractionTool.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QBitmap>
#include <QTransform>

#include "KoPointerEvent.h"
#include "KoToolManager.h"
#include "KoShape.h"
#include "KoSelection.h"
#include "KoShapeManager.h"
#include "KoInteractionStrategy.h"
#include "KoInteractionToolWidget.h"
#include "KoCanvasBase.h"
#include "KoPanTool.h"
#include "KoCanvasResourceProvider.h"
#include "commands/KoShapeMoveCommand.h"

#include <QUndoCommand>
#include <kaction.h>
#include <kicon.h>
#include <kcursor.h>
#include <kstandarddirs.h>
#include <k3staticdeleter.h>
#include <kdebug.h>

#define HANDLE_DISTANCE 10

class KoInteractionTool::Private {
public:
    Private() : lastHandle(KoFlake::NoHandle), mouseWasInsideHandles( false ), moveCommand(0) { }
    ~Private() {
        moveCommand = 0;
    }

    KoFlake::SelectionHandle lastHandle;
    bool mouseWasInsideHandles;
    QPointF selectionBox[8];
    QPolygonF selectionOutline;
    QPointF lastPoint;
    KoShapeMoveCommand *moveCommand;
    QTime lastUsedMoveCommand;

    // TODO alter these 3 arrays to be static const instead
    QCursor sizeCursors[8];
    QCursor rotateCursors[8];
    QCursor shearCursors[8];
    double angle;
};

KoInteractionTool::KoInteractionTool( KoCanvasBase *canvas )
    : KoTool( canvas ),
    m_currentStrategy(0),
    d( new Private())
{
    setupActions();

    QPixmap rotatePixmap, shearPixmap;
    rotatePixmap.load(KStandardDirs::locate("data", "koffice/icons/rotate.png"));
    shearPixmap.load(KStandardDirs::locate("data", "koffice/icons/shear.png"));

    d->rotateCursors[0] = QCursor(rotatePixmap.transformed(QMatrix().rotate(45)));
    d->rotateCursors[1] = QCursor(rotatePixmap.transformed(QMatrix().rotate(90)));
    d->rotateCursors[2] = QCursor(rotatePixmap.transformed(QMatrix().rotate(135)));
    d->rotateCursors[3] = QCursor(rotatePixmap.transformed(QMatrix().rotate(180)));
    d->rotateCursors[4] = QCursor(rotatePixmap.transformed(QMatrix().rotate(225)));
    d->rotateCursors[5] = QCursor(rotatePixmap.transformed(QMatrix().rotate(270)));
    d->rotateCursors[6] = QCursor(rotatePixmap.transformed(QMatrix().rotate(315)));
    d->rotateCursors[7] = QCursor(rotatePixmap);
/*
    d->rotateCursors[0] = QCursor(Qt::RotateNCursor);
    d->rotateCursors[1] = QCursor(Qt::RotateNECursor);
    d->rotateCursors[2] = QCursor(Qt::RotateECursor);
    d->rotateCursors[3] = QCursor(Qt::RotateSECursor);
    d->rotateCursors[4] = QCursor(Qt::RotateSCursor);
    d->rotateCursors[5] = QCursor(Qt::RotateSWCursor);
    d->rotateCursors[6] = QCursor(Qt::RotateWCursor);
    d->rotateCursors[7] = QCursor(Qt::RotateNWCursor);
*/
    d->shearCursors[0] = QCursor(shearPixmap);
    d->shearCursors[1] = QCursor(shearPixmap.transformed(QMatrix().rotate(45)));
    d->shearCursors[2] = QCursor(shearPixmap.transformed(QMatrix().rotate(90)));
    d->shearCursors[3] = QCursor(shearPixmap.transformed(QMatrix().rotate(135)));
    d->shearCursors[4] = QCursor(shearPixmap.transformed(QMatrix().rotate(180)));
    d->shearCursors[5] = QCursor(shearPixmap.transformed(QMatrix().rotate(225)));
    d->shearCursors[6] = QCursor(shearPixmap.transformed(QMatrix().rotate(270)));
    d->shearCursors[7] = QCursor(shearPixmap.transformed(QMatrix().rotate(315)));
    d->sizeCursors[0] = Qt::SizeVerCursor;
    d->sizeCursors[1] = Qt::SizeBDiagCursor;
    d->sizeCursors[2] = Qt::SizeHorCursor;
    d->sizeCursors[3] = Qt::SizeFDiagCursor;
    d->sizeCursors[4] = Qt::SizeVerCursor;
    d->sizeCursors[5] = Qt::SizeBDiagCursor;
    d->sizeCursors[6] = Qt::SizeHorCursor;
    d->sizeCursors[7] = Qt::SizeFDiagCursor;
}

KoInteractionTool::~KoInteractionTool()
{
    delete d;
    delete m_currentStrategy;
    m_currentStrategy = 0;
}

bool KoInteractionTool::wantsAutoScroll()
{
    return true;
}

void KoInteractionTool::setupActions()
{
    KAction* actionBringToFront = new KAction( KIcon( "bring_forward" ),
                                               i18n( "Bring to &Front" ), this );
    addAction( "object_move_totop", actionBringToFront );
    actionBringToFront->setShortcut( QKeySequence( "Ctrl+Shift+]" ) );
//    connect(actionBringToFront, SIGNAL(triggered()), this, SLOT(selectionBringToFront()));

    KAction* actionRaise = new KAction( KIcon( "raise" ), i18n( "&Raise" ), this );
    addAction( "object_move_up", actionRaise );
    actionRaise->setShortcut( QKeySequence( "Ctrl+]" ) );
//    connect(actionRaise, SIGNAL(triggered()), this, SLOT(selectionMoveUp()));

    KAction* actionLower = new KAction( KIcon( "lower" ), i18n( "&Lower" ), this );
    addAction( "object_move_down", actionLower );
    actionLower->setShortcut( QKeySequence( "Ctrl+[" ) );
//    connect(actionLower, SIGNAL(triggered()), this, SLOT(selectionMoveDown()));

    KAction* actionSendToBack = new KAction( KIcon( "send_backward" ),
                                             i18n( "Send to &Back" ), this );
    addAction( "object_move_tobottom", actionSendToBack );
    actionSendToBack->setShortcut( QKeySequence( "Ctrl+Shift+[" ) );
//    connect(actionSendToBack, SIGNAL(triggered()), this, SLOT(selectionSendToBack()));

    KAction* actionAlignLeft = new KAction( KIcon( "aoleft" ),
                                            i18n( "Align Left" ), this );
    addAction( "object_align_horizontal_left", actionAlignLeft );
//    connect(actionAlignLeft, SIGNAL(triggered()), this, SLOT(selectionAlignHorizontalLeft()));

    KAction* actionAlignCenter = new KAction( KIcon( "aocenterh" ),
                                              i18n( "Horizontally Center" ), this );
    addAction( "object_align_horizontal_center", actionAlignCenter );
//    connect(actionAlignCenter, SIGNAL(triggered()), this, SLOT(selectionAlignHorizontalCenter()));

    KAction* actionAlignRight = new KAction( KIcon( "aoright" ),
                                             i18n( "Align Right" ), this );
    addAction( "object_align_horizontal_right", actionAlignRight );
//    connect(actionAlignRight, SIGNAL(triggered()), this, SLOT(selectionAlignHorizontalRight()));

    KAction* actionAlignTop = new KAction( KIcon( "aotop" ), i18n( "Align Top" ), this );
    addAction( "object_align_vertical_top", actionAlignTop );
//    connect(actionAlignTop, SIGNAL(triggered()), this, SLOT(selectionAlignVerticalTop()));

    KAction* actionAlignMiddle = new KAction( KIcon( "aocenterv" ),
                                              i18n( "Vertically Center" ), this );
    addAction( "object_align_vertical_center", actionAlignMiddle );
//    connect(actionAlignMiddle, SIGNAL(triggered()), this, SLOT(selectionAlignVerticalCenter()));

    KAction* actionAlignBottom = new KAction( KIcon( "aobottom" ),
                                              i18n( "Align Bottom" ), this );
    addAction( "object_align_vertical_bottom", actionAlignBottom );
//    connect(actionAlignBottom, SIGNAL(triggered()), this, SLOT(selectionAlignVerticalBottom()));
}

double KoInteractionTool::rotationOfHandle( KoFlake::SelectionHandle handle, bool useEdgeRotation )
{
    QPointF selectionCenter = koSelection()->absolutePosition();
    QPointF direction;

    switch( handle )
    {
        case KoFlake::TopMiddleHandle:
            if( useEdgeRotation )
            {
                direction = koSelection()->absolutePosition( KoFlake::TopRightCorner )
                    - koSelection()->absolutePosition( KoFlake::TopLeftCorner );
            }
            else
            {
                QPointF handlePosition = koSelection()->absolutePosition( KoFlake::TopLeftCorner );
                handlePosition += 0.5 * ( koSelection()->absolutePosition( KoFlake::TopRightCorner ) - handlePosition );
                direction = handlePosition - selectionCenter;
            }
            break;
        case KoFlake::TopRightHandle:
            direction = koSelection()->absolutePosition( KoFlake::TopRightCorner ) - selectionCenter;
            break;
        case KoFlake::RightMiddleHandle:
            if( useEdgeRotation )
            {
                direction = koSelection()->absolutePosition( KoFlake::BottomRightCorner )
                        - koSelection()->absolutePosition( KoFlake::TopRightCorner );
            }
            else
            {
                QPointF handlePosition = koSelection()->absolutePosition( KoFlake::TopRightCorner );
                handlePosition += 0.5 * ( koSelection()->absolutePosition( KoFlake::BottomRightCorner ) - handlePosition );
                direction = handlePosition - selectionCenter;
            }
            break;
        case KoFlake::BottomRightHandle:
            direction = koSelection()->absolutePosition( KoFlake::BottomRightCorner ) - selectionCenter;
            break;
        case KoFlake::BottomMiddleHandle:
            if( useEdgeRotation )
            {
                direction = koSelection()->absolutePosition( KoFlake::BottomLeftCorner )
                        - koSelection()->absolutePosition( KoFlake::BottomRightCorner );
            }
            else
            {
                QPointF handlePosition = koSelection()->absolutePosition( KoFlake::BottomLeftCorner );
                handlePosition += 0.5 * ( koSelection()->absolutePosition( KoFlake::BottomRightCorner ) - handlePosition );
                direction = handlePosition - selectionCenter;
            }
            break;
        case KoFlake::BottomLeftHandle:
            direction = koSelection()->absolutePosition( KoFlake::BottomLeftCorner ) - selectionCenter;
            break;
        case KoFlake::LeftMiddleHandle:
            if( useEdgeRotation )
            {
                direction = koSelection()->absolutePosition( KoFlake::TopLeftCorner )
                        - koSelection()->absolutePosition( KoFlake::BottomLeftCorner );
            }
            else
            {
                QPointF handlePosition = koSelection()->absolutePosition( KoFlake::TopLeftCorner );
                handlePosition += 0.5 * ( koSelection()->absolutePosition( KoFlake::BottomLeftCorner ) - handlePosition );
                direction = handlePosition - selectionCenter;
            }
            break;
        case KoFlake::TopLeftHandle:
            direction = koSelection()->absolutePosition( KoFlake::TopLeftCorner ) - selectionCenter;
            break;
        case KoFlake::NoHandle:
            return 0.0;
            break;
    }

    double rotation = atan2( direction.y(), direction.x() ) * 180.0 / M_PI;

    switch( handle )
    {
        case KoFlake::TopMiddleHandle:
            if( useEdgeRotation )
                rotation -= 0.0;
            else
                rotation -= 270.0;
            break;
        case KoFlake::TopRightHandle:
            rotation -= 315.0;
            break;
        case KoFlake::RightMiddleHandle:
            if( useEdgeRotation )
                rotation -= 90.0;
            else
                rotation -= 0.0;
            break;
        case KoFlake::BottomRightHandle:
            rotation -= 45.0;
            break;
        case KoFlake::BottomMiddleHandle:
            if( useEdgeRotation )
                rotation -= 180.0;
            else
                rotation -= 90.0;
            break;
        case KoFlake::BottomLeftHandle:
            rotation -= 135.0;
            break;
        case KoFlake::LeftMiddleHandle:
            if( useEdgeRotation )
                rotation -= 270.0;
            else
                rotation -= 180.0;
            break;
        case KoFlake::TopLeftHandle:
            rotation -= 225.0;
            break;
        case KoFlake::NoHandle:
            break;
    }

    if( rotation < 0.0 )
        rotation += 360.0;

    return rotation;
}

void KoInteractionTool::updateCursor() {
    QCursor cursor = Qt::ArrowCursor;

    if(koSelection()->count() > 0) { // has a selection
        bool editable=false;
        foreach(KoShape *shape, koSelection()->selectedShapes(KoFlake::StrippedSelection)) {
            if(!shape->isLocked())
                editable = true;
        }

        if(!d->mouseWasInsideHandles) {
            d->angle = rotationOfHandle( d->lastHandle, true );
            int rotOctant = 8 + int(8.5 + d->angle / 45);

            switch(d->lastHandle) {
                case KoFlake::TopMiddleHandle:
                    cursor = d->shearCursors[(0 +rotOctant)%8];
                    break;
                case KoFlake::TopRightHandle:
                    cursor = d->rotateCursors[(1 +rotOctant)%8];
                    break;
                case KoFlake::RightMiddleHandle:
                    cursor = d->shearCursors[(2 +rotOctant)%8];
                    break;
                case KoFlake::BottomRightHandle:
                    cursor = d->rotateCursors[(3 +rotOctant)%8];
                    break;
                case KoFlake::BottomMiddleHandle:
                    cursor = d->shearCursors[(4 +rotOctant)%8];
                    break;
                case KoFlake::BottomLeftHandle:
                    cursor = d->rotateCursors[(5 +rotOctant)%8];
                    break;
                case KoFlake::LeftMiddleHandle:
                    cursor = d->shearCursors[(6 +rotOctant)%8];
                    break;
                case KoFlake::TopLeftHandle:
                    cursor = d->rotateCursors[(7 +rotOctant)%8];
                    break;
                case KoFlake::NoHandle:
                    cursor = Qt::ArrowCursor;
                    break;
             }
        }
        else {
            d->angle = rotationOfHandle( d->lastHandle, false );
            int rotOctant = 8 + int(8.5 + d->angle / 45);
            switch(d->lastHandle) {
                case KoFlake::TopMiddleHandle:
                    cursor = d->sizeCursors[(0 +rotOctant)%8];
                    break;
                case KoFlake::TopRightHandle:
                    cursor = d->sizeCursors[(1 +rotOctant)%8];
                    break;
                case KoFlake::RightMiddleHandle:
                    cursor = d->sizeCursors[(2 +rotOctant)%8];
                    break;
                case KoFlake::BottomRightHandle:
                    cursor = d->sizeCursors[(3 +rotOctant)%8];
                    break;
                case KoFlake::BottomMiddleHandle:
                    cursor = d->sizeCursors[(4 +rotOctant)%8];
                    break;
                case KoFlake::BottomLeftHandle:
                    cursor = d->sizeCursors[(5 +rotOctant)%8];
                    break;
                case KoFlake::LeftMiddleHandle:
                    cursor = d->sizeCursors[(6 +rotOctant)%8];
                    break;
                case KoFlake::TopLeftHandle:
                    cursor = d->sizeCursors[(7 +rotOctant)%8];
                    break;
                case KoFlake::NoHandle:
                    cursor = Qt::SizeAllCursor;
                    break;
            }
        }
        if( !editable)
            cursor = Qt::ArrowCursor;
    }
    useCursor(cursor);
}

void KoInteractionTool::paint( QPainter &painter, const KoViewConverter &converter) {
    if ( m_currentStrategy )
        m_currentStrategy->paint( painter, converter);
    else if(koSelection()->count() > 0) {
        SelectionDecorator decorator(d->mouseWasInsideHandles ? d->lastHandle : KoFlake::NoHandle,
                 true, true);
        decorator.setSelection(koSelection());
        decorator.setHandleRadius( m_canvas->resourceProvider()->handleRadius() );
        decorator.paint(painter, converter);
    }
}

void KoInteractionTool::mousePressEvent( KoPointerEvent *event ) {
    if(m_currentStrategy) { // possible if the user presses an extra mouse button
        m_currentStrategy->cancelInteraction();
        delete m_currentStrategy;
        m_currentStrategy = 0;
        event->accept();
        updateCursor();
        return;
    }
    m_currentStrategy = KoInteractionStrategy::createStrategy(event, this, m_canvas);
    d->moveCommand = 0;
    updateCursor();
}

void KoInteractionTool::mouseMoveEvent( KoPointerEvent *event ) {
    if(m_currentStrategy) {
        d->lastPoint = event->point;
        m_currentStrategy->handleMouseMove( d->lastPoint, event->modifiers() );
    }
    else {
        if(koSelection()->count() > 0) {
            QRectF bound = handlesSize();
            if(bound.contains(event->point)) {
                bool inside;
                KoFlake::SelectionHandle newDirection = handleAt(event->point, &inside);
                if(inside != d->mouseWasInsideHandles || d->lastHandle != newDirection) {
                    d->lastHandle = newDirection;
                    d->mouseWasInsideHandles = inside;
                    repaintDecorations();
                }
            } else {
                if(d->lastHandle != KoFlake::NoHandle)
                    repaintDecorations();
                d->lastHandle = KoFlake::NoHandle;
                d->mouseWasInsideHandles = false;
            }
        }
        event->ignore();
    }
    updateCursor();
}

QRectF KoInteractionTool::handlesSize() {
    QRectF bound = koSelection()->boundingRect();
    // expansion Border
    if ( !m_canvas || !m_canvas->viewConverter() ) return bound;

    QPointF border = m_canvas->viewConverter()->viewToDocument(QPointF(HANDLE_DISTANCE, HANDLE_DISTANCE));
    bound.adjust(-border.x(), -border.y(), border.x(), border.y());
    return bound;
}

void KoInteractionTool::mouseReleaseEvent( KoPointerEvent *event ) {
    Q_UNUSED(event);
    if ( m_currentStrategy )
    {
        m_currentStrategy->finishInteraction( event->modifiers() );
        QUndoCommand *command = m_currentStrategy->createCommand();
        if(command)
            m_canvas->addCommand(command);
        delete m_currentStrategy;
        m_currentStrategy = 0;
        repaintDecorations();
    }
    else
        event->ignore();
    updateCursor();
}

void KoInteractionTool::mouseDoubleClickEvent( KoPointerEvent *event ) {
    QList<KoShape*> shapes;
    foreach(KoShape *shape, koSelection()->selectedShapes()) {
        if(shape->boundingRect().contains(event->point) && // first 'cheap' check
                shape->outline().contains(event->point)) // this is more expensive but weeds out the almost hits
            shapes.append(shape);
    }
    if(shapes.count() == 0) { // nothing in the selection was clicked on.
        KoShape *shape = m_canvas->shapeManager()->shapeAt (event->point, KoFlake::ShapeOnTop);
        if(shape)
            shapes.append(shape);
    }

    KoToolManager::instance()->switchToolRequested(
            KoToolManager::instance()->preferredToolForSelection(shapes));
}

void KoInteractionTool::keyPressEvent(QKeyEvent *event) {
    event->ignore();
    if(m_currentStrategy &&
       (event->key() == Qt::Key_Control ||
            event->key() == Qt::Key_Alt || event->key() == Qt::Key_Shift ||
            event->key() == Qt::Key_Meta)) {
        m_currentStrategy->handleMouseMove( d->lastPoint, event->modifiers() );
        event->accept();
    } else if(m_currentStrategy == 0) {
        double x=0.0, y=0.0;
        if(event->key() == Qt::Key_Left)
            x=-0.5;
        else if(event->key() == Qt::Key_Right)
            x=0.5;
        else if(event->key() == Qt::Key_Up)
            y=-0.5;
        else if(event->key() == Qt::Key_Down)
            y=0.5;

        if(x != 0.0 || y != 0.0) { // actually move
            if((event->modifiers() & Qt::ShiftModifier) == 0) { // no shift used
                x *= 10;
                y *= 10;
            }

            QList<QPointF> prevPos;
            QList<QPointF> newPos;
            QList<KoShape*> shapes;
            foreach(KoShape* shape, koSelection()->selectedShapes(KoFlake::StrippedSelection)) {
                if(shape->isLocked())
                    continue;
                shapes.append(shape);
                QPointF p = shape->position();
                prevPos.append(p);
                p.setX(p.x() + x);
                p.setY(p.y() + y);
                newPos.append(p);
            }
            if(shapes.count() > 0) {
                // use a timeout to make sure we don't reuse a command possibly deleted by the commandHistory
                if(d->lastUsedMoveCommand.msecsTo(QTime::currentTime()) > 5000)
                    d->moveCommand = 0;
                if(d->moveCommand) { // alter previous instead of creating new one.
                    d->moveCommand->setNewPositions(newPos);
                    d->moveCommand->redo();
                } else {
                    d->moveCommand = new KoShapeMoveCommand(shapes, prevPos, newPos);
                    m_canvas->addCommand(d->moveCommand);
                }
                d->lastUsedMoveCommand = QTime::currentTime();
                event->accept();
            }
        }
    }
}

void KoInteractionTool::keyReleaseEvent(QKeyEvent *event) {
    if(m_currentStrategy == 0) { // catch all cases where no current strategy is needed
        if(event->key() == Qt::Key_Space)
            emit activateTemporary(KoPanTool_ID);
    }
    else if(event->key() == Qt::Key_Escape) {
        m_currentStrategy->cancelInteraction();
        delete m_currentStrategy;
        m_currentStrategy = 0;
        event->accept();
    }
    else if(event->key() == Qt::Key_Control ||
            event->key() == Qt::Key_Alt || event->key() == Qt::Key_Shift ||
            event->key() == Qt::Key_Meta) {
        m_currentStrategy->handleMouseMove( d->lastPoint, event->modifiers() );
    }
}

void KoInteractionTool::repaintDecorations() {
    Q_ASSERT(koSelection());
    if ( koSelection()->count() > 0 )
        m_canvas->updateCanvas(handlesSize());
}

KoSelection *KoInteractionTool::koSelection() {
    Q_ASSERT(m_canvas);
    Q_ASSERT(m_canvas->shapeManager());
    return m_canvas->shapeManager()->selection();
}

KoFlake::SelectionHandle KoInteractionTool::handleAt(const QPointF &point, bool *innerHandleMeaning) {
    // check for handles in this order; meaning that when handles overlap the one on top is chosen
    static const KoFlake::SelectionHandle handleOrder[] = {
        KoFlake::BottomRightHandle,
        KoFlake::TopLeftHandle,
        KoFlake::BottomLeftHandle,
        KoFlake::TopRightHandle,
        KoFlake::BottomMiddleHandle,
        KoFlake::RightMiddleHandle,
        KoFlake::LeftMiddleHandle,
        KoFlake::TopMiddleHandle,
        KoFlake::NoHandle
    };

    if( koSelection()->count() == 0 )
        return KoFlake::NoHandle;

    recalcSelectionBox();
    const KoViewConverter *converter = m_canvas->viewConverter();
    if ( !converter ) return KoFlake::NoHandle;

    if(innerHandleMeaning != 0)
    {
        QPainterPath path;
        path.addPolygon(d->selectionOutline);
        *innerHandleMeaning =  path.contains(point);
    }
    for ( int i = 0; i < KoFlake::NoHandle; ++i ) {
        KoFlake::SelectionHandle handle = handleOrder[i];
        QPointF pt = converter->documentToView(point) - converter->documentToView(d->selectionBox[handle]);

        // if just inside the outline
        if(qAbs(pt.x()) < HANDLE_DISTANCE &&
                qAbs(pt.y()) < HANDLE_DISTANCE) {
            if(innerHandleMeaning != 0)
            {
                if(qAbs(pt.x()) < 4 && qAbs(pt.y()) < 4)
                    *innerHandleMeaning = true;
            }
            return handle;
        }
    }
    return KoFlake::NoHandle;
}

void KoInteractionTool::recalcSelectionBox() {
    if(koSelection()->count()==0)
        return;

    if(koSelection()->count()>1)
    {
        QMatrix matrix = koSelection()->absoluteTransformation(0);
        d->selectionOutline = matrix.map(QPolygonF(QRectF(QPointF(0, 0), koSelection()->size())));
        d->angle = 0.0; //koSelection()->rotation();
    }
    else
    {
        QMatrix matrix = koSelection()->firstSelectedShape()->absoluteTransformation(0);
        d->selectionOutline = matrix.map(QPolygonF(QRectF(QPointF(0, 0), koSelection()->firstSelectedShape()->size())));
        d->angle = 0.0; //koSelection()->firstSelectedShape()->rotation();
    }
    QPolygonF outline = d->selectionOutline; //shorter name in the following :)
    d->selectionBox[KoFlake::TopMiddleHandle] = (outline.value(0)+outline.value(1))/2;
    d->selectionBox[KoFlake::TopRightHandle] = outline.value(1);
    d->selectionBox[KoFlake::RightMiddleHandle] = (outline.value(1)+outline.value(2))/2;
    d->selectionBox[KoFlake::BottomRightHandle] = outline.value(2);
    d->selectionBox[KoFlake::BottomMiddleHandle] = (outline.value(2)+outline.value(3))/2;
    d->selectionBox[KoFlake::BottomLeftHandle] = outline.value(3);
    d->selectionBox[KoFlake::LeftMiddleHandle] = (outline.value(3)+outline.value(0))/2;
    d->selectionBox[KoFlake::TopLeftHandle] = outline.value(0);
    if(koSelection()->count() == 1) {
        KoShape *s = koSelection()->firstSelectedShape();
        // TODO detect mirroring
        /*
        if(s->scaleX() < 0) { // vertically mirrored: swap left / right
            qSwap(d->selectionBox[KoFlake::TopLeftHandle], d->selectionBox[KoFlake::TopRightHandle]);
            qSwap(d->selectionBox[KoFlake::LeftMiddleHandle], d->selectionBox[KoFlake::RightMiddleHandle]);
            qSwap(d->selectionBox[KoFlake::BottomLeftHandle], d->selectionBox[KoFlake::BottomRightHandle]);
        }
        if(s->scaleY() < 0) { // vertically mirrored: swap top / bottom
            qSwap(d->selectionBox[KoFlake::TopLeftHandle], d->selectionBox[KoFlake::BottomLeftHandle]);
            qSwap(d->selectionBox[KoFlake::TopMiddleHandle], d->selectionBox[KoFlake::BottomMiddleHandle]);
            qSwap(d->selectionBox[KoFlake::TopRightHandle], d->selectionBox[KoFlake::BottomRightHandle]);
        }
        */
    }
}

void KoInteractionTool::activate(bool temporary) {
    Q_UNUSED(temporary);
    d->mouseWasInsideHandles = false;
    d->lastHandle = KoFlake::NoHandle;
    useCursor(Qt::ArrowCursor, true);
    repaintDecorations();
}

QWidget* KoInteractionTool::createOptionWidget() {
    return new KoInteractionToolWidget( this );
}


// ##########  SelectionDecorator ############
QImage * SelectionDecorator::s_rotateCursor=0;
static K3StaticDeleter<QImage> staticRotateCursorDeleter;

SelectionDecorator::SelectionDecorator(KoFlake::SelectionHandle arrows,
        bool rotationHandles, bool shearHandles)
: m_rotationHandles(rotationHandles)
, m_shearHandles(shearHandles)
, m_arrows(arrows)
, m_handleRadius( 3 )
{
    if(SelectionDecorator::s_rotateCursor == 0) {
        staticRotateCursorDeleter.setObject(s_rotateCursor, new QImage());
        s_rotateCursor->load(KStandardDirs::locate("lib", "flake/rotate.png"));
    }
}

void SelectionDecorator::setSelection(KoSelection *selection) {
    m_selection = selection;
}

void SelectionDecorator::setHandleRadius( int radius ) {
    m_handleRadius = radius;
}

void SelectionDecorator::paint(QPainter &painter, const KoViewConverter &converter) {
    QPen pen( Qt::green );
    QPolygonF outline;

    painter.save();

    painter.setPen( pen );
    bool editable=false;
    foreach(KoShape *shape, m_selection->selectedShapes(KoFlake::StrippedSelection)) {


        QMatrix matrix = shape->absoluteTransformation(0);
        outline = matrix.map(QPolygonF(QRectF(QPointF(0, 0), shape->size())));
        for(int i =0; i<outline.count(); i++)
            outline[i] = converter.documentToView(outline.value(i));

        // position the points in the middle of the pixels
        // for sharper on-screen rendering
        outline = outline.toPolygon();
        outline.translate(0.5,0.5);

        painter.drawPolygon(outline);
        if(!shape->isLocked())
            editable = true;
    }
    painter.restore();

    if(m_selection->count()>1)
    {
        QMatrix matrix = m_selection->absoluteTransformation(0);
        outline = matrix.map(QPolygonF(QRectF(QPointF(0, 0), m_selection->size())));
        for(int i =0; i<outline.count(); i++)
            outline[i] = converter.documentToView(outline.value(i));
        pen = QPen( Qt::blue );
        painter.setPen(pen);
        painter.drawPolygon(outline);
    }
    else if( m_selection->firstSelectedShape() )
    {
        QMatrix matrix = m_selection->firstSelectedShape()->absoluteTransformation(0);
        outline = matrix.map(QPolygonF(QRectF(QPointF(0, 0), m_selection->firstSelectedShape()->size())));
        for(int i =0; i<outline.count(); i++)
            outline[i] = converter.documentToView(outline.value(i));
    }

    if( !editable)
        return;

    painter.setRenderHint( QPainter::Antialiasing, false );

    pen = QPen( Qt::black );
    pen.setWidth(1);
    painter.setPen(pen);
    painter.setBrush(Qt::yellow);

    // the 8 move rects
    QRectF rect( QPointF(0.5,0.5), QSizeF(2*m_handleRadius,2*m_handleRadius) );
    pen.setWidthF(0);
    painter.setPen(pen);
    rect.moveCenter(outline.value(0));
    painter.drawRect(rect);
    rect.moveCenter(outline.value(1));
    painter.drawRect(rect);
    rect.moveCenter(outline.value(2));
    painter.drawRect(rect);
    rect.moveCenter(outline.value(3));
    painter.drawRect(rect);
    rect.moveCenter((outline.value(0)+outline.value(1))/2);
    painter.drawRect(rect);
    rect.moveCenter((outline.value(1)+outline.value(2))/2);
    painter.drawRect(rect);
    rect.moveCenter((outline.value(2)+outline.value(3))/2);
    painter.drawRect(rect);
    rect.moveCenter((outline.value(3)+outline.value(0))/2);
    painter.drawRect(rect);


#if 0
    // draw the move arrow(s)
    if(m_arrows != KoFlake::NoHandle && bounds.width() > 45 && bounds.height() > 45) {
        double x1,x2,y1,y2; // 2 is where the arrow head is
        switch(m_arrows) {
            case KoFlake::TopMiddleHandle:
                x1=bounds.center().x(); x2=x1; y2=bounds.y()+8; y1=y2+20;
                break;
            case KoFlake::TopRightHandle:
                x2=bounds.right()-8; x1=x2-20; y2=bounds.y()+8; y1=y2+20;
                break;
            case KoFlake::RightMiddleHandle:
                x2=bounds.right()-8; x1=x2-20; y1=bounds.center().y(); y2=y1;
                break;
            case KoFlake::BottomRightHandle:
                x2=bounds.right()-8; x1=x2-20; y2=bounds.bottom()-8; y1=y2-20;
                break;
            case KoFlake::BottomMiddleHandle:
                x1=bounds.center().x(); x2=x1; y2=bounds.bottom()-8; y1=y2-20;
                break;
            case KoFlake::BottomLeftHandle:
                x2=bounds.left()+8; x1=x2+20; y2=bounds.bottom()-8; y1=y2-20;
                break;
            case KoFlake::LeftMiddleHandle:
                x2=bounds.left()+8; x1=x2+20; y1=bounds.center().y(); y2=y1;
                break;
            default:
            case KoFlake::TopLeftHandle:
                x2=bounds.left()+8; x1=x2+20; y2=bounds.y()+8; y1=y2+20;
                break;
        }
        painter.drawLine(QLineF(x1, y1, x2, y2));
        //pen.setColor(Qt::white);
        //painter.setPen(pen);
        //painter.drawLine(QLineF(x1-1, y1-1, x2-1, y2-1));
    }

    QPointF border(HANDLE_DISTANCE, HANDLE_DISTANCE);
    bounds.adjust(-border.x(), -border.y(), border.x(), border.y());

    if(m_rotationHandles) {
        painter.save();
        painter.translate(bounds.x(), bounds.y());
        QRectF rect(QPointF(0,0), QSizeF(22, 22));
        painter.drawImage(rect, *s_rotateCursor, rect);
        painter.translate(bounds.width(), 0);
        painter.rotate(90);
        if(bounds.width() > 45 && bounds.height() > 45)
            painter.drawImage(rect, *s_rotateCursor, rect);
        painter.translate(bounds.height(), 0);
        painter.rotate(90);
        painter.drawImage(rect, *s_rotateCursor, rect);
        painter.translate(bounds.width(), 0);
        painter.rotate(90);
        if(bounds.width() > 45 && bounds.height() > 45)
            painter.drawImage(rect, *s_rotateCursor, rect);
        painter.restore();
    }

    /*if(m_shearHandles) {
        pen.setWidthF(0);
        painter.setPen(pen);
        QRectF rect(bounds.topLeft(), QSizeF(6, 6));
        rect.moveLeft(bounds.x() + bounds.width() /2 -3);
        painter.drawRect(rect);
        rect.moveBottom(bounds.bottom());
        painter.drawRect(rect);
        rect.moveLeft(bounds.left());
        rect.moveTop(bounds.top() + bounds.width() / 2 -3);
        painter.drawRect(rect);
        rect.moveRight(bounds.right());
        painter.drawRect(rect);
    } */
#endif
}
