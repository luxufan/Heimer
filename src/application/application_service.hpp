// This file is part of Heimer.
// Copyright (C) 2018 Jussi Lind <jussi.lind@iki.fi>
//
// Heimer is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// Heimer is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Heimer. If not, see <http://www.gnu.org/licenses/>.

#ifndef APPLICATION_SERVICE_HPP
#define APPLICATION_SERVICE_HPP

#include <QFont>
#include <QObject>
#include <QPointF>
#include <QString>

#include "../common/types.hpp"
#include "../domain/mind_map_data.hpp"
#include "../view/scene_items/node.hpp"

class MouseAction;
class EditorScene;
class EditorView;
class ExportParams;
class Graph;
class MainWindow;
class NodeAction;
class QGraphicsItem;
struct ShadowEffectParams;

namespace SceneItems {
class NodeHandle;
}

/*! Acts as a communication channel between MainWindow and editor components:
 *
 *  - MainWindow <-> ApplicationService <-> QGraphicsScene / EditorView / EditorService
 *  - EditorView <-> ApplicationService <-> EditorService
 */
class ApplicationService : public QObject
{
    Q_OBJECT

public:
    explicit ApplicationService(MainWindowS mainWindow);

    ~ApplicationService() override;

    void addEdge(NodeR node1, NodeR node2);

    void addItem(QGraphicsItem & item, bool adjustSceneRect = true);

    void addNodeToSelectionGroup(NodeR node, bool isImplicit = false);

    void adjustSceneRect();

    bool areDirectlyConnected(NodeCR node1, NodeCR node2) const;

    bool areSelectedNodesConnectable() const;

    bool areSelectedNodesDisconnectable() const;

    bool canBeSaved() const;

    void changeFont(const QFont & font);

    void clearSelectionGroup(bool onlyImplicitNodes = false);

    void connectEdgeToUndoMechanism(EdgeS edge);

    void connectNodeToUndoMechanism(NodeS node);

    void connectNodeToImageManager(NodeS node);

    size_t copyStackSize() const;

    // Create a new node and add edge to the source (parent) node
    NodeS createAndAddNode(int sourceNodeIndex, QPointF pos);

    // Create a new floating node
    NodeS createAndAddNode(QPointF pos);

    MouseAction & mouseAction();

    void deleteEdge(EdgeR edge);

    QString fileName() const;

    NodeS getBestOverlapNode(NodeCR source);

    NodeS getNodeByIndex(int index);

    bool hasNodes() const;

    void initializeNewMindMap();

    void initiateNewNodeDrag(SceneItems::NodeHandle & nodeHandle);

    void initiateNodeDrag(NodeR node);

    void initializeView();

    bool isInBetween(NodeR node);

    bool isInSelectionGroup(NodeR node);

    bool isLeafNode(NodeR node);

    bool isUndoable() const;

    bool isRedoable() const;

    bool isModified() const;

    void moveSelectionGroup(NodeR reference, QPointF location);

    size_t nodeCount() const;

    bool nodeHasImageAttached() const;

    NodeS pasteNodeAt(NodeR source, QPointF pos);

    MindMapDataS mindMapData() const;

    bool openMindMap(QString fileName);

    void performNodeAction(const NodeAction & action);

    void redo();

    void removeItem(QGraphicsItem & item);

    bool saveMindMapAs(QString fileName);

    bool saveMindMap();

    QSize sceneRectSize() const;

    EdgeP selectedEdge() const;

    NodeP selectedNode() const;

    size_t selectionGroupSize() const;

    void setEditorView(EditorView & editorView);

    void setGridSize(int size, bool autoSnap);

    //! \returns number of nodes in the current rectangle.
    size_t setRectagleSelection(QRectF rect);

    void setSelectedEdge(EdgeP edge);

    void showStatusText(QString statusText);

    void toggleNodeInSelectionGroup(NodeR node, bool updateNodeConnectionActions = true);

    void undo();

    void unselectImplicitlySelectedNodes();

    void unselectSelectedNode();

    void unselectText();

public slots:

    void enableAutosave(bool enable);

    void enableUndo(bool enable);

    void enableRedo(bool enable);

    void exportToPng(const ExportParams & exportParams);

    void exportToSvg(const ExportParams & exportParams);

    void mirror(bool vertically);

    void saveUndoPoint();

    void setArrowSize(double arrowSize);

    void setBackgroundColor(QColor color);

    void setCornerRadius(int value);

    void setEdgeColor(QColor color);

    void setEdgeWidth(double value);

    void setGridColor(QColor color);

    void setSearchText(QString text);

    void setShadowEffect(const ShadowEffectParams & params);

    void setTextSize(int textSize);

    QSize zoomForExport(bool dryRun = false);

    void zoomToFit();

private slots:

    void zoomIn();

    void zoomOut();

signals:

    void pngExportFinished(bool success);

    void svgExportFinished(bool success);

private:
    void addExistingGraphToScene(bool zoomToFitAfterNodesLoaded = false);

    using GridLineVector = std::vector<QGraphicsLineItem *>;
    GridLineVector addGridForExport();

    double calculateNodeOverlapScore(NodeCR node1, NodeCR node2) const;

    void connectGraphToUndoMechanism();

    void connectGraphToImageManager();

    void connectSelectedNodes();

    void disconnectSelectedNodes();

    void paste();

    void removeLineItems(const GridLineVector & lines);

    void setupMindMapAfterUndoOrRedo();

    void updateNodeConnectionActions();

    void updateProgress();

    EditorServiceS m_editorService;

    std::unique_ptr<EditorScene> m_editorScene;

    EditorView * m_editorView = nullptr;

    MainWindowS m_mainWindow;
};

#endif // APPLICATION_SERVICE_HPP
