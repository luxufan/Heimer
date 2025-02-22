// This file is part of Heimer.
// Copyright (C) 2022 Jussi Lind <jussi.lind@iki.fi>
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

#include "alz_file_io_worker.hpp"

#include "../../application/progress_manager.hpp"
#include "../../application/service_container.hpp"
#include "../../common/constants.hpp"
#include "../../common/test_mode.hpp"
#include "../../common/types.hpp"
#include "../../common/utils.hpp"
#include "../../domain/graph.hpp"
#include "../../domain/image_manager.hpp"
#include "../../domain/mind_map_data.hpp"
#include "../../view/scene_items/edge.hpp"
#include "../../view/scene_items/node.hpp"
#include "xml_reader.hpp"
#include "xml_writer.hpp"

#include "simple_logger.hpp"

#include <functional>
#include <map>
#include <set>

#include <QApplication>
#include <QDebug>
#include <QDomElement>
#include <QFile>
#include <QTemporaryDir>

namespace IO {

namespace DataKeywords::MindMap {

const auto APPLICATION_VERSION = "version";

namespace V2 {

const auto APPLICATION_VERSION = "application-version";

const auto ALZ_FORMAT_VERSION = "alz-format-version";

namespace Metadata {

const auto METADATA = "metadata";

} // namespace Metadata

namespace Style {

const auto STYLE = "style";

} // namespace Style

} // namespace V2

const auto ARROW_SIZE = "arrow-size";

const auto COLOR = "color";

const auto CORNER_RADIUS = "corner-radius";

const auto EDGE_COLOR = "edge-color";

const auto EDGE_THICKNESS = "edge-width";

const auto FONT_FAMILY = "font-family";

const auto FONT_BOLD = "bold";

const auto FONT_OVERLINE = "overline";

const auto FONT_STRIKE_OUT = "strike-out";

const auto FONT_UNDERLINE = "underline";

const auto FONT_WEIGHT = "weight";

const auto FONT_ITALIC = "italic";

const auto GRAPH = "graph";

const auto GRID_COLOR = "grid-color";

const auto HEIMER_MIND_MAP = "heimer-mind-map";

const auto IMAGE = "image";

const auto TEXT_SIZE = "text-size";

// Used for Design and Node
namespace Color {

const auto R = "r";

const auto G = "g";

const auto B = "b";
} // namespace Color

namespace Graph {

const auto NODE = "node";

namespace Node {

const auto COLOR = "color";

const auto IMAGE = "image";

const auto INDEX = "index";

const auto TEXT = "text";

const auto TEXT_COLOR = "text-color";

const auto X = "x";

const auto Y = "y";

const auto W = "w";

const auto H = "h";

namespace Image {

const auto REF = "ref";
} // namespace Image

namespace V2 {

const auto INDEX = "i";

}

} // namespace Node

const auto EDGE = "edge";

namespace Edge {

const auto ARROW_MODE = "arrow-mode";

const auto DASHED_LINE = "dashed-line";

const auto INDEX0 = "index0";

const auto INDEX1 = "index1";

const auto REVERSED = "reversed";

namespace V2 {

const auto INDEX0 = "i0";

const auto INDEX1 = "i1";

} // namespace V2

} // namespace Edge
} // namespace Graph

namespace Image {

const auto ID = "id";

const auto PATH = "path";

} // namespace Image

// Note!!: Moved into metadata element in V2
namespace LayoutOptimizer {

const auto ASPECT_RATIO = "aspect-ratio";

const auto LAYOUT_OPTIMIZER = "layout-optimizer";

const auto MIN_EDGE_LENGTH = "min-edge-length";

} // namespace LayoutOptimizer

} // namespace DataKeywords::MindMap

static const double SCALE = 1000; // https://bugreports.qt.io/browse/QTBUG-67129

static void writeColor(QDomElement & parent, QDomDocument & doc, QColor color, QString elementName)
{
    auto colorElement = doc.createElement(elementName);
    colorElement.setAttribute(DataKeywords::MindMap::Color::R, color.red());
    colorElement.setAttribute(DataKeywords::MindMap::Color::G, color.green());
    colorElement.setAttribute(DataKeywords::MindMap::Color::B, color.blue());
    parent.appendChild(colorElement);
}

static void writeImageRef(QDomElement & parent, QDomDocument & doc, size_t imageRef, QString elementName)
{
    auto colorElement = doc.createElement(elementName);
    colorElement.setAttribute(DataKeywords::MindMap::Graph::Node::Image::REF, static_cast<int>(imageRef));
    parent.appendChild(colorElement);
}

static void writeNodes(MindMapDataS mindMapData, QDomElement & root, QDomDocument & doc, AlzFormatVersion outputVersion)
{
    for (auto && node : mindMapData->graph().getNodes()) {

        using namespace DataKeywords::MindMap::Graph;

        auto nodeElement = doc.createElement(NODE);
        nodeElement.setAttribute(outputVersion == AlzFormatVersion::V1 ? Node::INDEX : Node::V2::INDEX, node->index());
        nodeElement.setAttribute(Node::X, static_cast<int>(node->location().x() * SCALE));
        nodeElement.setAttribute(Node::Y, static_cast<int>(node->location().y() * SCALE));
        nodeElement.setAttribute(Node::W, static_cast<int>(node->size().width() * SCALE));
        nodeElement.setAttribute(Node::H, static_cast<int>(node->size().height() * SCALE));
        root.appendChild(nodeElement);

        // Create a child node for the text content
        if (!node->text().isEmpty()) {
            auto textElement = doc.createElement(Node::TEXT);
            textElement.appendChild(doc.createTextNode(node->text()));
            nodeElement.appendChild(textElement);
        }

        // Create a child node for color
        writeColor(nodeElement, doc, node->color(), Node::COLOR);

        // Create a child node for text color
        writeColor(nodeElement, doc, node->textColor(), Node::TEXT_COLOR);

        // Create a child node for image ref
        if (node->imageRef()) {
            writeImageRef(nodeElement, doc, node->imageRef(), Node::IMAGE);
        }
    }
}

static void writeEdges(MindMapDataS mindMapData, QDomElement & root, QDomDocument & doc, AlzFormatVersion outputVersion)
{
    for (auto && node : mindMapData->graph().getNodes()) {
        for (auto && edge : mindMapData->graph().getEdgesFromNode(node)) {
            using namespace DataKeywords::MindMap::Graph;
            auto edgeElement = doc.createElement(EDGE);
            edgeElement.setAttribute(Edge::ARROW_MODE, static_cast<int>(edge->arrowMode()));
            if (edge->dashedLine()) {
                edgeElement.setAttribute(Edge::DASHED_LINE, edge->dashedLine());
            }
            edgeElement.setAttribute(outputVersion == AlzFormatVersion::V1 ? Edge::INDEX0 : Edge::V2::INDEX0, edge->sourceNode().index());
            edgeElement.setAttribute(outputVersion == AlzFormatVersion::V1 ? Edge::INDEX1 : Edge::V2::INDEX1, edge->targetNode().index());
            if (edge->reversed()) {
                edgeElement.setAttribute(Edge::REVERSED, edge->reversed());
            }
            root.appendChild(edgeElement);

            // Create a child node for the text content
            if (!edge->text().isEmpty()) {
                auto textElement = doc.createElement(Node::TEXT);
                edgeElement.appendChild(textElement);
                const auto textNode = doc.createTextNode(edge->text());
                textElement.appendChild(textNode);
            }
        }
    }
}

static void writeGraph(MindMapDataS mindMapData, QDomElement & root, QDomDocument & doc, AlzFormatVersion outputVersion)
{
    auto graph = doc.createElement(DataKeywords::MindMap::GRAPH);
    root.appendChild(graph);

    writeNodes(mindMapData, graph, doc, outputVersion);

    writeEdges(mindMapData, graph, doc, outputVersion);
}

static void writeStyle(MindMapDataS mindMapData, QDomElement & root, QDomDocument & doc, AlzFormatVersion outputVersion)
{
    const auto doWriteStyle = [](MindMapDataS mindMapData, QDomElement & root, QDomDocument & doc) {
        writeColor(root, doc, mindMapData->backgroundColor(), DataKeywords::MindMap::COLOR);

        writeColor(root, doc, mindMapData->edgeColor(), DataKeywords::MindMap::EDGE_COLOR);

        writeColor(root, doc, mindMapData->gridColor(), DataKeywords::MindMap::GRID_COLOR);

        auto arrowSizeElement = doc.createElement(DataKeywords::MindMap::ARROW_SIZE);
        arrowSizeElement.appendChild(doc.createTextNode(QString::number(static_cast<int>(mindMapData->arrowSize() * SCALE))));
        root.appendChild(arrowSizeElement);

        auto edgeWidthElement = doc.createElement(DataKeywords::MindMap::EDGE_THICKNESS);
        edgeWidthElement.appendChild(doc.createTextNode(QString::number(static_cast<int>(mindMapData->edgeWidth() * SCALE))));
        root.appendChild(edgeWidthElement);

        auto fontFamilyElement = doc.createElement(DataKeywords::MindMap::FONT_FAMILY);
        fontFamilyElement.setAttribute(DataKeywords::MindMap::FONT_BOLD, mindMapData->font().bold());
        fontFamilyElement.setAttribute(DataKeywords::MindMap::FONT_ITALIC, mindMapData->font().italic());
        fontFamilyElement.setAttribute(DataKeywords::MindMap::FONT_OVERLINE, mindMapData->font().overline());
        fontFamilyElement.setAttribute(DataKeywords::MindMap::FONT_STRIKE_OUT, mindMapData->font().strikeOut());
        fontFamilyElement.setAttribute(DataKeywords::MindMap::FONT_UNDERLINE, mindMapData->font().underline());
        fontFamilyElement.setAttribute(DataKeywords::MindMap::FONT_WEIGHT, Utils::fontWeightToInt(mindMapData->font().weight()));
        fontFamilyElement.appendChild(doc.createTextNode(mindMapData->font().family()));
        root.appendChild(fontFamilyElement);

        auto textSizeElement = doc.createElement(DataKeywords::MindMap::TEXT_SIZE);
        textSizeElement.appendChild(doc.createTextNode(QString::number(static_cast<int>(mindMapData->textSize() * SCALE))));
        root.appendChild(textSizeElement);

        auto cornerRadiusElement = doc.createElement(DataKeywords::MindMap::CORNER_RADIUS);
        cornerRadiusElement.appendChild(doc.createTextNode(QString::number(static_cast<int>(mindMapData->cornerRadius() * SCALE))));
        root.appendChild(cornerRadiusElement);
    };

    if (outputVersion == AlzFormatVersion::V1) {
        doWriteStyle(mindMapData, root, doc);
    } else {
        auto styleElement = doc.createElement(DataKeywords::MindMap::V2::Style::STYLE);
        doWriteStyle(mindMapData, styleElement, doc);
        root.appendChild(styleElement);
    }
}

static void writeVersion(QDomElement & root, QDomDocument & doc, AlzFormatVersion outputVersion)
{
    if (outputVersion == AlzFormatVersion::V1) {
        root.setAttribute(DataKeywords::MindMap::APPLICATION_VERSION, Constants::Application::APPLICATION_VERSION);
    } else {
        root.setAttribute(DataKeywords::MindMap::V2::APPLICATION_VERSION, Constants::Application::APPLICATION_VERSION);
        root.setAttribute(DataKeywords::MindMap::V2::ALZ_FORMAT_VERSION, static_cast<int>(Constants::Application::ALZ_FORMAT_VERSION));
    }
    doc.appendChild(root);
}

static QString getBase64Data(std::string path)
{
    if (!TestMode::enabled()) {
        QFile in(path.c_str());
        if (!in.open(QIODevice::ReadOnly)) {
            throw std::runtime_error("Cannot open file: '" + path + "'");
        }
        return in.readAll().toBase64(QByteArray::Base64Encoding);
    } else {
        TestMode::logDisabledCode("getBase64Data");
        return {};
    }
}

static QImage base64ToQImage(const std::string & base64, size_t imageId, std::string imagePath)
{
    juzzlin::L().debug() << "Extracting embedded Image id=" << imageId << ", path=" << imagePath;
    const auto ba = QByteArray::fromBase64(base64.c_str(), QByteArray::Base64Encoding);
    QImage in;
    if (!in.loadFromData(ba)) {
        juzzlin::L().error() << "Could not load embedded Image id=" << imageId << ", path=" << imagePath;
    }
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    juzzlin::L().debug() << "Image size: " << in.sizeInBytes() << " bytes";
#endif
    return in;
}

static void writeImages(MindMapDataS mindMapData, QDomElement & root, QDomDocument & doc)
{
    std::set<size_t> writtenImageRefs;
    for (auto && node : mindMapData->graph().getNodes()) {
        if (node->imageRef()) {
            if (writtenImageRefs.count(node->imageRef())) {
                juzzlin::L().debug() << "Image id=" << node->imageRef() << " already written";
            } else {
                if (const auto image = mindMapData->imageManager().getImage(node->imageRef()); image.has_value()) {
                    auto imageElement = doc.createElement(DataKeywords::MindMap::IMAGE);
                    imageElement.setAttribute(DataKeywords::MindMap::Image::ID, static_cast<int>(image->id()));
                    imageElement.setAttribute(DataKeywords::MindMap::Image::PATH, image->path().c_str());
                    root.appendChild(imageElement);

                    // Create a child node for the image content
                    if (!TestMode::enabled()) {
                        QTemporaryDir dir;
                        const QFileInfo info(image->path().c_str());
                        const QString tempImagePath = (dir.path() + QDir::separator() + info.fileName());
                        image->image().save(tempImagePath);
                        imageElement.appendChild(doc.createTextNode(getBase64Data(tempImagePath.toStdString())));
                        writtenImageRefs.insert(image->id());
                    } else {
                        TestMode::logDisabledCode("writeImages");
                    }
                } else {
                    throw std::runtime_error("Image id=" + std::to_string(node->imageRef()) + " doesn't exist!");
                }
            }
        }
    }
}

static void writeLayoutOptimizer(MindMapDataS mindMapData, QDomElement & root, QDomDocument & doc)
{
    auto layoutOptimizerElement = doc.createElement(DataKeywords::MindMap::LayoutOptimizer::LAYOUT_OPTIMIZER);
    layoutOptimizerElement.setAttribute(DataKeywords::MindMap::LayoutOptimizer::ASPECT_RATIO, mindMapData->aspectRatio() * SCALE);
    layoutOptimizerElement.setAttribute(DataKeywords::MindMap::LayoutOptimizer::MIN_EDGE_LENGTH, mindMapData->minEdgeLength() * SCALE);
    root.appendChild(layoutOptimizerElement);
}

static void writeMetadata(MindMapDataS mindMapData, QDomElement & root, QDomDocument & doc, AlzFormatVersion outputVersion)
{
    if (outputVersion == AlzFormatVersion::V1) {
        writeLayoutOptimizer(mindMapData, root, doc);
    } else {
        auto metadataElement = doc.createElement(DataKeywords::MindMap::V2::Metadata::METADATA);
        writeLayoutOptimizer(mindMapData, metadataElement, doc);
        root.appendChild(metadataElement);
    }
}

static QColor readColorElement(const QDomElement & element)
{
    return {
        element.attribute(DataKeywords::MindMap::Color::R, "255").toInt(),
        element.attribute(DataKeywords::MindMap::Color::G, "255").toInt(),
        element.attribute(DataKeywords::MindMap::Color::B, "255").toInt()
    };
}

static size_t readImageElement(const QDomElement & element)
{
    return static_cast<size_t>(element.attribute(DataKeywords::MindMap::Graph::Node::Image::REF, "0").toInt());
}

static QString readFirstTextNodeContent(const QDomElement & element)
{
    for (int i = 0; i < element.childNodes().count(); i++) {
        const auto child = element.childNodes().at(i);
        if (child.isText()) {
            // See: https://github.com/juzzlin/Heimer/issues/73
            return child.toText().nodeValue().replace(QChar(QChar::CarriageReturn), "");
        }
    }
    return {};
}

static void elementWarning(const QDomElement & element)
{
    juzzlin::L().warning() << "Unknown element '" << element.nodeName().toStdString() << "'";
}

// Generic helper that loops through element's children
static void readChildren(const QDomElement & root, std::map<QString, std::function<void(const QDomElement &)>> handlerMap)
{
    auto domNode = root.firstChild();
    while (!domNode.isNull()) {
        SIC::instance().progressManager()->updateProgress();
        const auto element = domNode.toElement();
        if (!element.isNull()) {
            if (handlerMap.count(element.nodeName())) {
                handlerMap[element.nodeName()](element);
            } else {
                elementWarning(element);
            }
        }

        domNode = domNode.nextSibling();
    }
}

static NodeU readNode(const QDomElement & element)
{
    // Init a new node. QGraphicsScene will take the ownership eventually.
    using namespace DataKeywords::MindMap::Graph;
    auto node = std::make_unique<SceneItems::Node>();
    const auto noIndex = "-1";
    if (element.hasAttribute(Node::V2::INDEX)) {
        node->setIndex(element.attribute(Node::V2::INDEX, noIndex).toInt());
    } else {
        node->setIndex(element.attribute(Node::INDEX, noIndex).toInt());
    }

    node->setLocation(QPointF(
      element.attribute(Node::X, "0").toInt() / SCALE,
      element.attribute(Node::Y, "0").toInt() / SCALE));

    if (element.hasAttribute(Node::W) && element.hasAttribute(Node::H)) {
        node->setSize(QSizeF(
          element.attribute(Node::W).toInt() / SCALE,
          element.attribute(Node::H).toInt() / SCALE));
    }

    readChildren(element, { { QString(Node::TEXT), [&node](const QDomElement & e) {
                                 node->setText(readFirstTextNodeContent(e));
                             } },
                            { QString(Node::COLOR), [&node](const QDomElement & e) {
                                 node->setColor(readColorElement(e));
                             } },
                            { QString(Node::TEXT_COLOR), [&node](const QDomElement & e) {
                                 node->setTextColor(readColorElement(e));
                             } },
                            { QString(Node::IMAGE), [&node](const QDomElement & e) {
                                 node->setImageRef(readImageElement(e));
                             } } });

    return node;
}

static EdgeU readEdge(const QDomElement & element, MindMapData & data)
{
    using namespace DataKeywords::MindMap::Graph;

    const int arrowMode = element.attribute(Edge::ARROW_MODE, "0").toInt();
    const bool dashedLine = element.attribute(Edge::DASHED_LINE, "0").toInt();

    const auto noIndex = "-1";

    const int index0 = element.hasAttribute(Edge::V2::INDEX0) ? element.attribute(Edge::V2::INDEX0, noIndex).toInt()
                                                              : element.attribute(Edge::INDEX0, noIndex).toInt();

    const int index1 = element.hasAttribute(Edge::V2::INDEX1) ? element.attribute(Edge::V2::INDEX1, noIndex).toInt()
                                                              : element.attribute(Edge::INDEX1, noIndex).toInt();

    const bool reversed = element.attribute(Edge::REVERSED, "0").toInt();

    // Initialize a new edge. QGraphicsScene will take the ownership eventually.
    auto edge = std::make_unique<SceneItems::Edge>(data.graph().getNode(index0), data.graph().getNode(index1));
    edge->setArrowMode(static_cast<SceneItems::EdgeModel::ArrowMode>(arrowMode));
    edge->setDashedLine(dashedLine);
    edge->setReversed(reversed);

    readChildren(element, { { QString(DataKeywords::MindMap::Graph::Node::TEXT), [&edge](const QDomElement & e) {
                                 edge->setText(readFirstTextNodeContent(e));
                             } } });

    return edge;
}

static void readLayoutOptimizer(const QDomElement & element, MindMapData & data)
{
    double aspectRatio = element.attribute(DataKeywords::MindMap::LayoutOptimizer::ASPECT_RATIO, "-1").toDouble() / SCALE;
    aspectRatio = std::min(aspectRatio, Constants::LayoutOptimizer::MAX_ASPECT_RATIO);
    aspectRatio = std::max(aspectRatio, Constants::LayoutOptimizer::MIN_ASPECT_RATIO);
    data.setAspectRatio(aspectRatio);

    double minEdgeLength = element.attribute(DataKeywords::MindMap::LayoutOptimizer::MIN_EDGE_LENGTH, "-1").toDouble() / SCALE;
    minEdgeLength = std::min(minEdgeLength, Constants::LayoutOptimizer::MAX_EDGE_LENGTH);
    minEdgeLength = std::max(minEdgeLength, Constants::LayoutOptimizer::MIN_EDGE_LENGTH);
    data.setMinEdgeLength(minEdgeLength);
}

static void readMetadata(const QDomElement & element, MindMapData & data)
{
    readChildren(element, { { QString(DataKeywords::MindMap::LayoutOptimizer::LAYOUT_OPTIMIZER), [&](const QDomElement & e) {
                                 readLayoutOptimizer(e, data);
                             } } });
}

static void readStyle(const QDomElement & element, MindMapData & data)
{
    readChildren(element, { { QString(DataKeywords::MindMap::ARROW_SIZE), [&data](const QDomElement & e) {
                                 data.setArrowSize(readFirstTextNodeContent(e).toDouble() / SCALE);
                             } },
                            { QString(DataKeywords::MindMap::COLOR), [&data](const QDomElement & e) {
                                 data.setBackgroundColor(readColorElement(e));
                             } },
                            { QString(DataKeywords::MindMap::EDGE_COLOR), [&data](const QDomElement & e) {
                                 data.setEdgeColor(readColorElement(e));
                             } },
                            { QString(DataKeywords::MindMap::FONT_FAMILY), [&data](const QDomElement & e) {
                                 QFont font;
                                 font.setFamily(readFirstTextNodeContent(e));
                                 font.setBold(e.attribute(DataKeywords::MindMap::FONT_BOLD).toInt());
                                 font.setItalic(e.attribute(DataKeywords::MindMap::FONT_ITALIC).toUInt());
                                 font.setOverline(e.attribute(DataKeywords::MindMap::FONT_OVERLINE).toInt());
                                 font.setUnderline(e.attribute(DataKeywords::MindMap::FONT_UNDERLINE).toInt());
                                 font.setStrikeOut(e.attribute(DataKeywords::MindMap::FONT_STRIKE_OUT).toInt());
                                 font.setWeight(Utils::intToFontWeight(e.attribute(DataKeywords::MindMap::FONT_WEIGHT).toInt()));
                                 data.changeFont(font);
                             } },
                            { QString(DataKeywords::MindMap::GRID_COLOR), [&data](const QDomElement & e) {
                                 data.setGridColor(readColorElement(e));
                             } },
                            { QString(DataKeywords::MindMap::EDGE_THICKNESS), [&data](const QDomElement & e) {
                                 data.setEdgeWidth(readFirstTextNodeContent(e).toDouble() / SCALE);
                             } },
                            { QString(DataKeywords::MindMap::TEXT_SIZE), [&data](const QDomElement & e) {
                                 data.setTextSize(static_cast<int>(readFirstTextNodeContent(e).toDouble() / SCALE));
                             } },
                            { QString(DataKeywords::MindMap::CORNER_RADIUS), [&data](const QDomElement & e) {
                                 data.setCornerRadius(static_cast<int>(readFirstTextNodeContent(e).toDouble() / SCALE));
                             } } });
}

static void readGraph(const QDomElement & graph, MindMapData & data)
{
    readChildren(graph, {
                          { QString(DataKeywords::MindMap::Graph::NODE), [&data](const QDomElement & e) {
                               data.graph().addNode(readNode(e));
                           } },
                          { QString(DataKeywords::MindMap::Graph::EDGE), [&data](const QDomElement & e) {
                               data.graph().addEdge(readEdge(e, data));
                           } },
                        });
}

// Import always assumes the newest ALZ-format version, but it's backwards compatible
MindMapDataU fromXml(QDomDocument document)
{
    auto data = std::make_unique<MindMapData>();

    const auto root = document.documentElement();
    const auto undefinedVersion = "UNDEFINED";
    data->setApplicationVersion(root.hasAttribute(DataKeywords::MindMap::V2::APPLICATION_VERSION) ? //
                                  root.attribute(DataKeywords::MindMap::V2::APPLICATION_VERSION, undefinedVersion)
                                                                                                  : //
                                  root.attribute(DataKeywords::MindMap::APPLICATION_VERSION, undefinedVersion));

    data->setAlzFormatVersion(
      static_cast<IO::AlzFormatVersion>(
        root.attribute(DataKeywords::MindMap::V2::ALZ_FORMAT_VERSION, "1").toInt()));

    readChildren(root, { { QString(DataKeywords::MindMap::GRAPH), [&data](const QDomElement & e) {
                              readGraph(e, *data);
                          } },
                         { QString(DataKeywords::MindMap::ARROW_SIZE), [&data](const QDomElement & e) {
                              data->setArrowSize(readFirstTextNodeContent(e).toDouble() / SCALE);
                          } },
                         { QString(DataKeywords::MindMap::COLOR), [&data](const QDomElement & e) {
                              data->setBackgroundColor(readColorElement(e));
                          } },
                         { QString(DataKeywords::MindMap::EDGE_COLOR), [&data](const QDomElement & e) {
                              data->setEdgeColor(readColorElement(e));
                          } },
                         { QString(DataKeywords::MindMap::FONT_FAMILY), [&data](const QDomElement & e) {
                              QFont font;
                              font.setFamily(readFirstTextNodeContent(e));
                              font.setBold(e.attribute(DataKeywords::MindMap::FONT_BOLD).toInt());
                              font.setItalic(e.attribute(DataKeywords::MindMap::FONT_ITALIC).toUInt());
                              font.setOverline(e.attribute(DataKeywords::MindMap::FONT_OVERLINE).toInt());
                              font.setUnderline(e.attribute(DataKeywords::MindMap::FONT_UNDERLINE).toInt());
                              font.setStrikeOut(e.attribute(DataKeywords::MindMap::FONT_STRIKE_OUT).toInt());
                              font.setWeight(Utils::intToFontWeight(e.attribute(DataKeywords::MindMap::FONT_WEIGHT).toInt()));
                              data->changeFont(font);
                          } },
                         { QString(DataKeywords::MindMap::GRID_COLOR), [&data](const QDomElement & e) {
                              data->setGridColor(readColorElement(e));
                          } },
                         { QString(DataKeywords::MindMap::EDGE_THICKNESS), [&data](const QDomElement & e) {
                              data->setEdgeWidth(readFirstTextNodeContent(e).toDouble() / SCALE);
                          } },
                         { QString(DataKeywords::MindMap::IMAGE), [&data](const QDomElement & e) {
                              const auto id = e.attribute(DataKeywords::MindMap::Image::ID).toUInt();
                              const auto path = e.attribute(DataKeywords::MindMap::Image::PATH).toStdString();
                              Image image(base64ToQImage(readFirstTextNodeContent(e).toStdString(), id, path), path);
                              image.setId(id);
                              data->imageManager().setImage(image);
                          } },
                         { QString(DataKeywords::MindMap::TEXT_SIZE), [&data](const QDomElement & e) {
                              data->setTextSize(static_cast<int>(readFirstTextNodeContent(e).toDouble() / SCALE));
                          } },
                         { QString(DataKeywords::MindMap::CORNER_RADIUS), [&data](const QDomElement & e) {
                              data->setCornerRadius(static_cast<int>(readFirstTextNodeContent(e).toDouble() / SCALE));
                          } },
                         { QString(DataKeywords::MindMap::LayoutOptimizer::LAYOUT_OPTIMIZER), [&data](const QDomElement & e) {
                              readLayoutOptimizer(e, *data);
                          } },
                         { QString(DataKeywords::MindMap::V2::Metadata::METADATA), [&data](const QDomElement & e) {
                              readMetadata(e, *data);
                          } },
                         { QString(DataKeywords::MindMap::V2::Style::STYLE), [&data](const QDomElement & e) {
                              readStyle(e, *data);
                          } } });

    return data;
}

QDomDocument toXml(MindMapDataS mindMapData, AlzFormatVersion outputVersion)
{
    QDomDocument doc;
    doc.appendChild(doc.createProcessingInstruction("xml", "version='1.0' encoding='UTF-8'"));
    auto root = doc.createElement(DataKeywords::MindMap::HEIMER_MIND_MAP);

    writeVersion(root, doc, outputVersion);

    writeStyle(mindMapData, root, doc, outputVersion);

    writeGraph(mindMapData, root, doc, outputVersion);

    writeImages(mindMapData, root, doc);

    writeMetadata(mindMapData, root, doc, outputVersion);

    return doc;
}

AlzFileIOWorker::AlzFileIOWorker(AlzFormatVersion outputVersion)
  : m_outputVersion(outputVersion)
{
}

AlzFileIOWorker::~AlzFileIOWorker() = default;

MindMapDataU AlzFileIOWorker::fromFile(QString path) const
{
    return IO::fromXml(XmlReader::readFromFile(path));
}

bool AlzFileIOWorker::toFile(MindMapDataS mindMapData, QString path) const
{
    return XmlWriter::writeToFile(IO::toXml(mindMapData, m_outputVersion), path);
}

MindMapDataU AlzFileIOWorker::fromXml(QString xml) const
{
    QDomDocument document;
    document.setContent(xml, false);
    SIC::instance().progressManager()->updateProgress();
    return IO::fromXml(document);
}

QString AlzFileIOWorker::toXml(MindMapDataS mindMapData) const
{
    return IO::toXml(mindMapData, m_outputVersion).toString();
}

} // namespace IO
